/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "nv/i3c/driver.h"

#include <algorithm>
#include <cstring>
#include <fsl_debug_console.h>

#include "nv/i3c/task.h"
#include "nv/ipc/mctp_bridge_config.h"
#include "nv/logger/log.h"
#include "nv/nv.h"
#include "fsl_i3c_edma.h"
#include "sys/common/common.h"
#include "sys/smartdma/driver.h"

// The CPU bring-up sequence in nv::i3c::Task::recover_cpu_i3c_bus() chains up to a
// hundred blocking CCCs without yielding, and the runtime watchdog only feeds once
// every monitored task has answered - so a stall here starves the whole MCU, not just
// this task. That sequence stays far inside the 10 s budget only because every SDK
// wait it reaches is capped at I3C_RETRY_TIMES polls. The SDK treats zero as "wait
// forever", which would turn the same sequence into an unbounded hang.
static_assert(I3C_RETRY_TIMES != 0,
              "I3C_RETRY_TIMES must stay bounded: the CPU I3C recovery sequence relies "
              "on it to keep the runtime watchdog fed");

namespace {

static void ibi_callback(I3C_Type*                 base,
                         i3c_master_edma_handle_t* handle,
                         i3c_ibi_type_t            ibi_type,
                         i3c_ibi_state_t           ibi_state)
{
    auto&         driver  = *static_cast<nv::i3c::Driver*>(handle->userData);
    const uint8_t port_id = static_cast<uint8_t>(I3C_GetInstance(base));
    if (ibi_state == kI3C_IbiAckNackPending) {
        // The SDK delegates the ACK/NACK decision to this callback, and merely
        // setting MCTRL.IBIRESP does not emit the response request, so the decision
        // has to be made here.
        //
        // A bridge's Normal IBI is the CPU's MCTP receive path, so it must be
        // answered with the policy currently armed in MCTRL.IBIRESP: NACK during CPU
        // recovery, AckMandatory once the first bring-up TX succeeded. Only bridge
        // builds arm that field outside a transfer - elsewhere it still holds
        // whatever the last transfer left behind.
        bool use_armed_policy = false;
        if constexpr (nv::ipc::MctpBridgeConfig<>::UsI3cBridgeMode) {
            use_armed_policy = ibi_type == kI3C_IbiNormal;
        }

        auto ibi_response = kI3C_IbiRespNack;
        if (use_armed_policy) {
            ibi_response = static_cast<i3c_ibi_response_t>(
                (base->MCTRL & I3C_MCTRL_IBIRESP_MASK) >> I3C_MCTRL_IBIRESP_SHIFT);
        }
        else {
            /* Manual / controller-role requests are unsupported - no AP may take the
               controller role and this firmware has no target mode. NACK to complete
               the HW handshake, then wait for kI3C_IbiReady before notifying the app.
               Not logged for a bridge policy NACK: the CPU retries a refused Normal
               IBI, so that path would flood the log during a recovery. */
            nv::logger::Logger::add_from_isr(nv::logger::Event::I3CManualIbiNack.unique_id,
                                             nv::logger::Level::Info,
                                             {port_id, static_cast<uint8_t>(ibi_type)});
        }

        I3C_MasterEmitIBIResponse(base, ibi_response);
        if (ibi_response == kI3C_IbiRespNack) {
#if defined(I3C_DMA_USE_STOP_OFFLOAD)
            /* SmartDMA N556 path: a NACK may not raise MSTATUS.COMPLETE, so nudge
               the handle into kStopState with IBIWon. Macro used here because
               handle->result is not defined in the N547 SDK. */
            constexpr uint8_t EdmaStopState = 8U;  // kStopState in 556 fsl_i3c_edma.c
            handle->result                  = kStatus_I3C_IBIWon;
            handle->state                   = EdmaStopState;
#endif
        }
        return;
    }
    if (ibi_state != kI3C_IbiReady) {
        nv::logger::Logger::add_from_isr(nv::logger::Event::I3CIgnoreIbiStateV2.unique_id,
                                         nv::logger::Level::Info,
                                         {port_id, static_cast<uint8_t>(ibi_state)});
        return;
    }

    switch (ibi_type) {
        case kI3C_IbiNormal: driver.on_ibi(static_cast<void*>(handle)); break;
        case kI3C_IbiMasterRequest:
            if constexpr (nv::ipc::EnableSmartDMA) {
                /* Manual NACK in previous IRQ above can cause a residual IBI stop
                   flag in SmartDMA */
                sys::smartdma::Driver::clear_ibi_stop(port_id);
            }
            /* CX WAR: a CX target sends a controller-role request where it meant a
               pending-read IBI, and leaves the packet queued forever if we drop it.
               UsI3c bridge buses are not CX; synthesizing 0xAE would start a spurious
               private read toward the CPU. */
            if constexpr (!nv::ipc::MctpBridgeConfig<>::UsI3cBridgeMode) {
                if (!driver.is_gpu()) {
                    driver.on_master_request_ibi(static_cast<void*>(handle));
                    break;
                }
            }
            nv::logger::Logger::add_from_isr(nv::logger::Event::I3CIgnoreIbiTypeV2.unique_id,
                                             nv::logger::Level::Info,
                                             {port_id, static_cast<uint8_t>(ibi_type)});
            break;
        default:
            nv::logger::Logger::add_from_isr(nv::logger::Event::I3CIgnoreIbiTypeV2.unique_id,
                                             nv::logger::Level::Info,
                                             {port_id, static_cast<uint8_t>(ibi_type)});
            return;
    }
}

static void i3c_callback(I3C_Type*                 base,
                         i3c_master_edma_handle_t* handle,
                         status_t                  status,
                         [[maybe_unused]] void*    user_data)
{
    using namespace nv;
    auto& driver = *static_cast<i3c::Driver*>(handle->userData);
    switch (status) {
        case kStatus_Success:
            if (handle->transfer.direction == kI3C_Read) {
                EDMA_StopTransfer(handle->rxDmaHandle);
                size_t length = 0;
                I3C_MasterTransferGetCountEDMA(base, handle, &length);
                handle->transferCount = length;
                I3C_MasterGetFifoCounts(handle->base, &length, nullptr);
                while (length > 0) {
                    // coverity[cert_int31_c_violation] MRDATAB FIFO read one byte a time
                    static_cast<uint8_t*>(
                        handle->transfer.data)[handle->transferCount++] = handle->base->MRDATAB;
                    length--;
                }
                EDMA_AbortTransfer(handle->rxDmaHandle);
                if (handle->transfer.flags & kI3C_TransferNoStopFlag) {
                    (void)I3C_MasterStop(base);
                }
            }
            driver.set_event(i3c::Driver::Event::Success);
            break;
        case kStatus_I3C_Nak:
#if 0
            // Only log when 5 retry failed case
            logger::Logger::add_from_isr(logger::Event::I3CNackV2,
                                         logger::Level::Info,
                                         {static_cast<uint8_t>(I3C_GetInstance(base)),
                                          static_cast<uint8_t>(handle->state)});
#endif
            EDMA_AbortTransfer(handle->rxDmaHandle);

            /* Reset fifos. These flags clear automatically. */
            base->MDATACTRL |= I3C_MDATACTRL_FLUSHTB_MASK | I3C_MDATACTRL_FLUSHFB_MASK;
            driver.set_event(i3c::Driver::Event::Nack);
            break;
        case kStatus_I3C_IBIWon: break;
        // coverity[unterminated_case] expected no break
        case kStatus_I3C_Timeout:
#if 0
            // Only log when 5 retry failed case
            logger::Logger::add_from_isr(logger::Event::I3CTimeoutV2,
                                         logger::Level::Info,
                                         {static_cast<uint8_t>(I3C_GetInstance(base)),
                                          static_cast<uint8_t>(handle->state)});
#endif
            EDMA_AbortTransfer(handle->rxDmaHandle);

            /* Reset fifos. These flags clear automatically. */
            base->MDATACTRL |= I3C_MDATACTRL_FLUSHTB_MASK | I3C_MDATACTRL_FLUSHFB_MASK;

            /* Send a stop command to finalize the transfer. */
            (void)I3C_MasterStop(base);
            // TODO : Add break due to "warning: this statement may fall through
            // [-Wimplicit-fallthrough=]"
            break;
        default:
            driver.set_event(i3c::Driver::Event::Error);
            if (driver._error_log_count < i3c::Driver::ErrorLogThreshold) {
                driver._error_log_count++;
                const uint8_t           port_id = static_cast<uint8_t>(I3C_GetInstance(base));
                const uint8_t           hstate  = static_cast<uint8_t>(handle->state);
                const uint32_t          result_u32 = static_cast<uint32_t>(status);
                const uint32_t          mstatus    = base->MSTATUS;
                const logger::EventData err_data   = {
                    port_id,
                    hstate,
                    static_cast<uint8_t>(result_u32 >> 0 & 0xFF),
                    static_cast<uint8_t>(result_u32 >> 8 & 0xFF),
                    static_cast<uint8_t>(result_u32 >> 16 & 0xFF),
                    static_cast<uint8_t>(result_u32 >> 24 & 0xFF),
                };
                // coverity[cert_int31_c_violation] log raw value
                logger::Logger::add_from_isr(logger::Event::I3CUnhandledErrorV2.unique_id,
                                             logger::Level::Info,
                                             err_data);
                const logger::EventData mstatus_data = {
                    port_id,
                    hstate,
                    static_cast<uint8_t>(mstatus >> 0 & 0xFF),
                    static_cast<uint8_t>(mstatus >> 8 & 0xFF),
                    static_cast<uint8_t>(mstatus >> 16 & 0xFF),
                    static_cast<uint8_t>(mstatus >> 24 & 0xFF),
                };
                logger::Logger::add_from_isr(logger::Event::I3CUnhandledErrorMStatus.unique_id,
                                             logger::Level::Info,
                                             mstatus_data);
            }
    }
}

// Trace every I3C CCC issued on this controller (RSTDAA/ENTDAA/ENEC/SETMRL/
// SETMWL/GETSTATUS). On the CPU-bridge product, port 1 is the CPU upstream (UsI3c) bus.
static void log_ccc_sent(uint8_t port, uint8_t ccc, uint8_t address)
{
    nv::logger::info(nv::logger::Event::I3CCccSentV2, {port, ccc, address});
}

// Snapshot the controller at a CCC failure. Must run before the ForceExit/STOP
// recovery, which clears the state that explains why the frame stalled:
// MSTATUS carries SLVSTART (bit 8), IBIWON (bit 13), ERRWARN (bit 15) and
// STATE (bits [2:0]); MERRWARN[15:0] details the error. `part` numbers the
// segment of a multi-part CCC that failed.
static void log_ccc_fail_detail(uint8_t port, uint8_t part, I3C_Type* base)
{
    const uint32_t mstatus  = base->MSTATUS;
    const uint32_t merrwarn = base->MERRWARN;
    nv::logger::info(nv::logger::Event::I3CCccFailDetailV2,
                     {port,
                      part,
                      static_cast<uint8_t>(mstatus >> 0 & 0xFF),
                      static_cast<uint8_t>(mstatus >> 8 & 0xFF),
                      static_cast<uint8_t>(mstatus >> 16 & 0xFF),
                      static_cast<uint8_t>(mstatus >> 24 & 0xFF),
                      static_cast<uint8_t>(merrwarn >> 0 & 0xFF),
                      static_cast<uint8_t>(merrwarn >> 8 & 0xFF)});
}

// Read out whatever an IBI left queued. The mandatory data byte is pushed as
// soon as the controller ACKs, and anything still there when the next transfer
// starts surfaces as the head of that transfer's RX data.
static void drain_rx_fifo(I3C_Type* base)
{
    uint32_t rx_count = (base->MDATACTRL & I3C_MDATACTRL_RXCOUNT_MASK)
                     >> I3C_MDATACTRL_RXCOUNT_SHIFT;
    while (rx_count-- != 0U) {
        (void)base->MRDATAB;
    }
}

}  // namespace

nv::i3c::Driver::Driver(
    Port port, Freq freq, bool is_gpu, void* task, nv::ipc::EventId event_id, uint32_t clock)
: sys::i3c::Driver()
, _port(port)
, _task(task)
, _event(nv::ipc::Event::make(event_id))
, _is_gpu(is_gpu)
{
#ifdef CPU_MCXN556SCDF
    static_assert(nv::ipc::EnableSmartDMA, "SmartDMA is not enabled for MCXN556SCDF");
#endif
    if (clock != 0) {
        _clock = clock;
    }
    I3C_MasterGetDefaultConfig(&_master_config);
    _master_config.baudRate_Hz.i2cBaud          = freq.i2c;
    _master_config.baudRate_Hz.i3cPushPullBaud  = freq.i3c_pp;
    _master_config.baudRate_Hz.i3cOpenDrainBaud = freq.i2c_od;
    _master_config.enableOpenDrainStop          = false;
    _master_config.disableTimeout               = true;
    // CX8 uses 50:50 duty cycle
    _master_config.enableOpenDrainHigh = is_gpu ? true : false;

    DMA_Type* dma            = nullptr;
    uint32_t  dma_tx_channel = 0;
    uint32_t  dma_rx_channel = 0;
    int32_t   dma_tx_mux     = 0;
    int32_t   dma_rx_mux     = 0;
    switch (port) {
        case Port::Zero:
            _base          = I3C0;
            dma            = DMA0;
            dma_tx_channel = 0;
            dma_rx_channel = 1;
            dma_tx_mux     = kDma0RequestMuxI3c0Tx;
            dma_rx_mux     = kDma0RequestMuxI3c0Rx;
            break;
        case Port::One:
            _base          = I3C1;
            dma            = DMA1;
            dma_tx_channel = 0;
            dma_rx_channel = 1;
            dma_tx_mux     = kDma1RequestMuxI3c1Tx;
            dma_rx_mux     = kDma1RequestMuxI3c1Rx;
            break;
        default: nv::info("unsport port %d\n", port); return;
    }
    // init controller
    EDMA_CreateHandle(&_tx_edma_handle, dma, dma_tx_channel);
    EDMA_CreateHandle(&_rx_edma_handle, dma, dma_rx_channel);
    EDMA_SetChannelMux(dma, dma_tx_channel, dma_tx_mux);
    EDMA_SetChannelMux(dma, dma_rx_channel, dma_rx_mux);
}

void nv::i3c::Driver::init()
{
    if (_is_init) {
        return;
    }
    IRQn_Type const                  kI3cIrqs[] = I3C_IRQS;
    const i3c_master_edma_callback_t Callback   = {
          .slave2Master = nullptr, .ibiCallback = ibi_callback, .transferComplete = i3c_callback};
    const auto instance = I3C_GetInstance(_base);
    I3C_MasterInit(_base, &_master_config, _clock);

    // On a UsI3c bridge (the CPU is the I3C bus owner) the MCU can see sustained
    // I3C interrupt activity, and a looping/storming I3C ISR at a priority above
    // USB pre-empts the USB device IRQ and starves the BMC-facing MCTP-over-USB
    // link (EID 80 drops). For these products lower the I3C IRQ below USB so USB
    // always wins; other products keep the Bug-5947030 pre-emptive priority.
    constexpr uint8_t kI3cIrqPriority = nv::ipc::MctpBridgeConfig<>::UsI3cBridgeMode
                                          ? static_cast<uint8_t>(
                                                nv::ipc::UsbDeviceInterruptPriority + 1U)
                                          : nv::ipc::I3CInterruptPriority;
    static_assert(kI3cIrqPriority >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY,
                  "Priority cannot be set higher than the FreeRTOS maximum for this platform");
    NVIC_SetPriority(kI3cIrqs[instance], kI3cIrqPriority);

    // coverity[cert_exp60_cpp_violation] waive this until I have a good solution
    I3C_MasterTransferCreateHandleEDMA(
        _base, &_i3c_m_handle, &Callback, this, &_rx_edma_handle, &_tx_edma_handle);
    _i3c_m_handle.ibiBuff = _ibi_data.data();
    _is_init              = true;

    // I3C_MasterInit() resets the peripheral, clearing the IBI address rules and
    // MCTRL.IBIRESP - the field ibi_callback() reads back to answer every IBI.
    // Every controller reset lands here (force_bus_idle() fallback,
    // try_recover(), IRQ-storm recovery) and nothing re-arms IBI delivery once
    // bring-up is done, so a reset in steady state would leave the CPU's IBIs
    // ACKed without their mandatory data byte. The CPU keeps the dynamic address
    // and ENINT it was granted across a controller-only reset, so it stays
    // entitled to raise them. GPU products register IBI during DAA, not here.
    if constexpr (nv::ipc::MctpBridgeConfig<>::UsI3cBridgeMode) {
        if (_ibi_address != 0U) {
            (void)register_ibi(_ibi_address);
            set_ibi_ack_mandatory();
        }
    }
    nv::logger::info(nv::logger::Event::I3cDriverInit, {static_cast<uint8_t>(_port), true});
}

void sys::i3c::set_irq_enabled(uint8_t instance, bool enable)
{
    constexpr uint8_t I3cInstanceNum = 2;
    if (instance >= I3cInstanceNum) {
        return;
    }
    IRQn_Type const kI3cIrqs[] = I3C_IRQS;
    if (enable) {
        NVIC_ClearPendingIRQ(kI3cIrqs[instance]);
        NVIC_EnableIRQ(kI3cIrqs[instance]);
    }
    else {
        NVIC_DisableIRQ(kI3cIrqs[instance]);
    }
}

namespace {
// I3C interrupt-storm breaker state (I3C1 / UsI3c only). A runaway I3C ISR keeps
// the controller stuck in a non-idle state and re-enters back-to-back with no
// progress, pinning the CPU and starving every task (WWDT reset, USB/EID80
// loss). This happens with the NXP EDMA state machine's "interrupt delay
// workaround" (SLVSTART left asserted while non-idle) but can also be driven by
// other latched conditions (ERRWARN/IBIWON). Detect it flag-agnostically:
// count consecutive ISR entries where the controller is NOT idle. A completed
// transfer returns the controller to Idle and resets the count, so normal
// traffic never accumulates; a genuine storm crosses the threshold within a
// couple of ms (far below the 10s watchdog) and we disable the IRQ so a task
// can recover the controller.
constexpr uint32_t kI3cIsrStormThreshold = 1024U;
volatile uint32_t  g_i3c1_isr_run        = 0U;
volatile bool      g_i3c1_irq_storm      = false;
// Storm-time controller snapshot captured the instant the storm is detected in
// the ISR (MSTATUS/MERRWARN show which condition is stuck: SLVSTART bit8,
// IBIWON bit13, ERRWARN bit15, STATE bits[2:0]; MERRWARN details the error).
// Read at detection because by recovery time the controller is back to idle.
volatile uint32_t g_i3c1_storm_mstatus  = 0U;
volatile uint32_t g_i3c1_storm_merrwarn = 0U;
}  // namespace

extern "C" void I3C1_DriverIRQHandler(void);

extern "C" void I3C1_IRQHandler(void)
{
    if constexpr (nv::ipc::MctpBridgeConfig<>::UsI3cBridgeMode) {
        if (I3C_MasterGetState(I3C1) != kI3C_MasterStateIdle) {
            const uint32_t isr_run = g_i3c1_isr_run + 1U;
            g_i3c1_isr_run         = isr_run;
            if (isr_run >= kI3cIsrStormThreshold) {
                g_i3c1_isr_run        = 0U;
                g_i3c1_storm_mstatus  = I3C1->MSTATUS;
                g_i3c1_storm_merrwarn = I3C1->MERRWARN;
                g_i3c1_irq_storm      = true;
                sys::i3c::set_irq_enabled(1U, false);
                return;
            }
        }
        else {
            g_i3c1_isr_run = 0U;
        }
    }
    I3C1_DriverIRQHandler();
}

bool nv::i3c::Driver::recover_irq_storm_if_pending()
{
    if (I3C_GetInstance(_base) != 1U || !g_i3c1_irq_storm) {
        return false;
    }
    g_i3c1_irq_storm = false;
    g_i3c1_isr_run   = 0U;

    // Log the storm-time snapshot captured in the ISR (not the current, already
    // recovered, state) so the stuck condition is identifiable: MSTATUS carries
    // SLVSTART/IBIWON/ERRWARN/STATE and MERRWARN[15:0] the specific error.
    const uint32_t mstatus  = g_i3c1_storm_mstatus;
    const uint32_t merrwarn = g_i3c1_storm_merrwarn;
    logger::info(logger::Event::I3CIrqStormV2,
                 {static_cast<uint8_t>(_port),
                  static_cast<uint8_t>(mstatus >> 0 & 0xFF),
                  static_cast<uint8_t>(mstatus >> 8 & 0xFF),
                  static_cast<uint8_t>(mstatus >> 16 & 0xFF),
                  static_cast<uint8_t>(mstatus >> 24 & 0xFF),
                  static_cast<uint8_t>(merrwarn >> 0 & 0xFF),
                  static_cast<uint8_t>(merrwarn >> 8 & 0xFF)});

    // The ISR already disabled the I3C1 NVIC IRQ to stop the storm. Recover the
    // controller here in task context (this path feeds the watchdog): abort the
    // stuck EDMA transfer, drain the pending Slave-Start, reset the SmartDMA
    // handshake, re-init, then re-enable the IRQ. DAA re-bring-up happens on the
    // normal AP-status Querying path.
    I3C_MasterTransferAbortEDMA(_i3c_m_handle.base, &_i3c_m_handle);
    (void)force_bus_idle();
    deinit();
    if constexpr (nv::ipc::EnableSmartDMA) {
        sys::smartdma::Driver::reset_flags(1U);
    }
    init();
    sys::i3c::set_irq_enabled(1U, true);
    return true;
}

void nv::i3c::Driver::deinit()
{
    if (!_is_init) {
        return;
    }
    I3C_MasterDeinit(_base);
    _is_init  = false;
    _daa_done = false;
    nv::logger::info(nv::logger::Event::I3cDriverInit, {static_cast<uint8_t>(_port), false});
}

bool nv::i3c::Driver::disec()
{
    constexpr uint8_t BoradcastAddress = 0x7EU;
    constexpr uint8_t CccDisec         = 0x01U;  // Broadcast DISEC
    // Disable IBI (ENINT), controller-role request (ENCR) and hot-join (ENHJ)
    // so the bus is quiet before RSTDAA/ENTDAA.
    constexpr uint8_t DisableEvents = 0x0BU;
    I3cBuffer         buffer{DisableEvents};
    _last_status = Status::Success;

    i3c_master_transfer_t xfer{};
    xfer.slaveAddress   = BoradcastAddress;
    xfer.subaddress     = CccDisec;
    xfer.subaddressSize = 1U;
    xfer.data           = buffer.data();
    xfer.dataSize       = 1U;
    xfer.direction      = kI3C_Write;
    xfer.busType        = kI3C_TypeI3CSdr;
    xfer.flags          = kI3C_TransferDefaultFlag;
    xfer.ibiResponse    = kI3C_IbiRespNack;
    log_ccc_sent(static_cast<uint8_t>(_port), CccDisec, BoradcastAddress);
    auto result = I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer);
    if (result != kStatus_Success) {
        I3C_MasterStop(_i3c_m_handle.base);
        const logger::EventData data = {
            static_cast<uint8_t>(_port),
            CccDisec,
            static_cast<uint8_t>(result >> 0 & 0xFF),
            static_cast<uint8_t>(result >> 8 & 0xFF),
            static_cast<uint8_t>(result >> 16 & 0xFF),
            static_cast<uint8_t>(result >> 24 & 0xFF),
        };
        logger::info(logger::Event::I3CCccErrorV2, data);
        _last_status = to_driver_status(static_cast<uint32_t>(result));
        return false;
    }
    return true;
}

bool nv::i3c::Driver::force_bus_idle()
{
    auto*          base   = _i3c_m_handle.base;
    const uint32_t before = base->MSTATUS;
    auto           state  = I3C_MasterGetState(base);

    // The CPU pulses a Slave-Start at nearly every transfer entry but clears it
    // within microseconds, so sampling STATE alone misses it. The sticky
    // SLVSTART flag latches the event until we clear it, so use it (together
    // with a live SlvReq) to decide whether a target request is pending.
    const bool slave_start_pending = state == kI3C_MasterStateSlvReq
                                  || (before & I3C_MSTATUS_SLVSTART_MASK) != 0U;
    // A request the controller already won arbitration for shows up as neither
    // of those: SlvReq is gone and SLVSTART was consumed, leaving only IBITYPE
    // and IBIADDR populated while STATE sits in NormAct. The controller still
    // owes that target its mandatory data byte, so it needs the same servicing
    // as a fresh Slave-Start.
    const bool ibi_captured = (before & I3C_MSTATUS_IBITYPE_MASK) != 0U;

    if (state == kI3C_MasterStateIdle && !slave_start_pending) {
        I3C_MasterClearStatusFlags(base, static_cast<uint32_t>(kI3C_MasterClearFlags));
        return true;
    }

    // Own the hardware state machine while draining the pending target request;
    // otherwise the EDMA ISR and this recovery path can issue competing MCTRL
    // requests.
    const uint32_t enabled_interrupts = I3C_MasterGetEnabledInterrupts(base);
    I3C_MasterDisableInterrupts(base, enabled_interrupts);
    I3C_MasterClearErrorStatusFlags(base, I3C_MasterGetErrorStatusFlags(base));

    status_t result = kStatus_Success;
    if (slave_start_pending) {
        // Unconditionally emit START+0x7E arbitration so a pending (possibly
        // already-deasserted) Slave-Start resolves into IBIACK, instead of
        // relying on the transient SlvReq state still being live.
        I3C_MasterClearStatusFlags(base, static_cast<uint32_t>(kI3C_MasterClearFlags));
        I3C_MasterEmitRequest(base, kI3C_RequestAutoIbi);
        result = I3C_MasterWaitForCtrlDone(base, false);
        state  = I3C_MasterGetState(base);
    }

    // AutoIbi under AckMandatory parks in IbiRcv rather than IbiAck once the
    // target starts pushing its mandatory byte, and an IBI captured before this
    // call is past IbiAck as well. Accepting only IbiAck sent both straight to
    // the controller reset below with the byte still unread, so the target kept
    // re-raising the same request and every later frame stalled against it.
    const bool servicing_ibi = state == kI3C_MasterStateIbiAck
                            || state == kI3C_MasterStateIbiRcv
                            || (ibi_captured && state == kI3C_MasterStateNormAct);
    if (result == kStatus_Success && servicing_ibi) {
        if (state == kI3C_MasterStateIbiAck) {
            // Service the CPU IBI (ACK + read its mandatory data byte) so the
            // target considers the request handled and stops re-asserting,
            // rather than NACKing it (which the CPU ignores and retries into a
            // storm).
            I3C_MasterEmitIBIResponse(base, kI3C_IbiRespAckMandatory);
            result = I3C_MasterWaitForCtrlDone(base, false);
        }
        drain_rx_fifo(base);
        state = I3C_MasterGetState(base);
    }

    // A half-open frame parks in NormAct and never reaches idle on its own:
    // an IBI whose payload has just been taken, or a CCC abandoned between its
    // two segments. Close it explicitly rather than waiting for an idle that
    // only a STOP can produce -- that wait is what expired as a bounded-poll
    // timeout and dropped every one of these into the controller reset below.
    // NormAct is the one non-idle state where a STOP is unambiguously valid.
    if (result == kStatus_Success && state == kI3C_MasterStateNormAct) {
        I3C_MasterEmitRequest(base, kI3C_RequestEmitStop);
        result = I3C_MasterWaitForCtrlDone(base, true);
        drain_rx_fifo(base);
        state = I3C_MasterGetState(base);
    }

    const bool idle = result == kStatus_Success && state == kI3C_MasterStateIdle;
    if (idle) {
        I3C_MasterClearStatusFlags(base, static_cast<uint32_t>(kI3C_MasterClearFlags));
        I3C_MasterEnableInterrupts(base, enabled_interrupts);
    }
    else {
        // The bounded SDK waits failed or the controller was in an unexpected
        // state. Reset only the MCU controller rather than issuing ForceExit or
        // STOP in a state where those requests may be invalid.
        I3C_MasterTransferAbortEDMA(base, &_i3c_m_handle);
        deinit();
        // Clear the SmartDMA IBI handshake the way recover_irq_storm_if_pending()
        // does: an ibi_stop / i3c_ibi_won left set by the frame just abandoned
        // survives deinit/init and strands the next IBI.
        if constexpr (nv::ipc::EnableSmartDMA) {
            sys::smartdma::Driver::reset_flags(static_cast<uint8_t>(I3C_GetInstance(base)));
        }
        init();
    }

    const uint32_t after = base->MSTATUS;
    logger::info(logger::Event::I3CForceBusIdleV2,
                 {static_cast<uint8_t>(_port),
                  static_cast<uint8_t>(before >> 0 & 0xFF),
                  static_cast<uint8_t>(before >> 8 & 0xFF),
                  static_cast<uint8_t>(before >> 16 & 0xFF),
                  static_cast<uint8_t>(before >> 24 & 0xFF),
                  // The call's own verdict, not a re-read of STATE: after the
                  // reset branch below the controller always reads back idle,
                  // which hid every controller reset from this event.
                  static_cast<uint8_t>(idle ? 1U : 0U),
                  static_cast<uint8_t>(after >> 16 & 0xFF),
                  static_cast<uint8_t>(after >> 24 & 0xFF)});
    return idle;
}

bool nv::i3c::Driver::reset_daa(bool ping)
{
    constexpr uint8_t BoradcastAddress = 0x7EU;
    constexpr uint8_t CccRstdaa        = 0x06U;
    // Only the bridge's execute_message() consults last_status() to tell a retryable
    // Busy/Timeout from a hard failure. Tracking it is write-only elsewhere.
    _last_status = Status::Success;
    i3c_master_transfer_t xfer{};
    xfer.slaveAddress   = BoradcastAddress;
    xfer.subaddress     = CccRstdaa;
    xfer.subaddressSize = 1U;
    xfer.direction      = kI3C_Write;
    xfer.busType        = kI3C_TypeI3CSdr;
    xfer.flags          = kI3C_TransferDefaultFlag;
    xfer.ibiResponse    = kI3C_IbiRespNack;
    if constexpr (nv::ipc::MctpBridgeConfig<>::UsI3cBridgeMode) {
        log_ccc_sent(static_cast<uint8_t>(_port), CccRstdaa, BoradcastAddress);
    }
    auto result = I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer);
    if (result != kStatus_Success) {
        _last_status = to_driver_status(static_cast<uint32_t>(result));
        I3C_MasterStop(_i3c_m_handle.base);
        nv::info("fail to reset daa %d\n", result);
        if (!ping) {
            logger::info(logger::Event::I3CFailedToResetDaaV2,
                         {static_cast<uint8_t>(_port),
                          static_cast<uint8_t>(result >> 0 & 0xFF),
                          static_cast<uint8_t>(result >> 8 & 0xFF),
                          static_cast<uint8_t>(result >> 16 & 0xFF),
                          static_cast<uint8_t>(result >> 24 & 0xFF)});
        }
        return false;
    }
    return true;
}

bool nv::i3c::Driver::enec()
{
    constexpr uint8_t BoradcastAddress = 0x7EU;
    constexpr uint8_t CccRstdaa        = 0x00;
    constexpr uint8_t Enint            = 0x01;
    I3cBuffer         buffer{
        Enint,
    };
    _last_status = Status::Success;

    i3c_master_transfer_t xfer{};
    xfer.slaveAddress   = BoradcastAddress;
    xfer.subaddress     = CccRstdaa;
    xfer.subaddressSize = 1U;
    xfer.data           = buffer.data();
    xfer.dataSize       = 1;
    xfer.direction      = kI3C_Write;
    xfer.busType        = kI3C_TypeI3CSdr;
    xfer.flags          = kI3C_TransferDefaultFlag;
    xfer.ibiResponse    = kI3C_IbiRespNack;
    if constexpr (nv::ipc::MctpBridgeConfig<>::UsI3cBridgeMode) {
        log_ccc_sent(static_cast<uint8_t>(_port), CccRstdaa, BoradcastAddress);
    }
    auto result = I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer);
    if (result != kStatus_Success) {
        I3C_MasterStop(_i3c_m_handle.base);
        const logger::EventData data = {
            static_cast<uint8_t>(_port),
            CccRstdaa,
            static_cast<uint8_t>(result >> 0 & 0xFF),
            static_cast<uint8_t>(result >> 8 & 0xFF),
            static_cast<uint8_t>(result >> 16 & 0xFF),
            static_cast<uint8_t>(result >> 24 & 0xFF),
        };
        logger::info(logger::Event::I3CCccErrorV2, data);

        auto status  = to_driver_status(static_cast<uint32_t>(result));
        _last_status = status;
        if (status != Status::Success) {
            const auto& task = *static_cast<nv::i3c::Task*>(_task);
            task.record_error(static_cast<uint8_t>(status));
        }
        return false;
    }
    return true;
}

bool nv::i3c::Driver::enec(uint8_t address)
{
    constexpr uint8_t BoradcastAddress = 0x7EU;
    constexpr uint8_t CccEnecDirect    = 0x80U;
    constexpr uint8_t Enint            = 0x01U;
    // Segment identifiers for I3CCccFailDetailV2: ENEC is sent as a broadcast
    // 0x7E+0x80 header held open with NoStop, then the directed ENINT byte.
    constexpr uint8_t PartBroadcastHeader = 1U;
    constexpr uint8_t PartDirectedByte    = 2U;
    _last_status                          = Status::Success;

    // Set when the ENINT byte was delivered and only the idle bus the SDK waits
    // for after STOP is missing; see send_frame().
    bool enint_answered_with_ibi_request = false;

    // Sends both segments of the directed ENEC. On failure it snapshots the
    // controller before the ForceExit/STOP that closes the frame, because that
    // recovery clears the state explaining the stall.
    auto send_frame = [this, address, &enint_answered_with_ibi_request]() -> status_t {
        i3c_master_transfer_t xfer{};
        xfer.slaveAddress   = BoradcastAddress;
        xfer.subaddress     = CccEnecDirect;
        xfer.subaddressSize = 1U;
        xfer.direction      = kI3C_Write;
        xfer.busType        = kI3C_TypeI3CSdr;
        xfer.flags          = kI3C_TransferNoStopFlag;
        xfer.ibiResponse    = kI3C_IbiRespAckMandatory;
        log_ccc_sent(static_cast<uint8_t>(_port), CccEnecDirect, address);
        auto status = I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer);
        if (status != kStatus_Success) {
            log_ccc_fail_detail(
                static_cast<uint8_t>(_port), PartBroadcastHeader, _i3c_m_handle.base);
            I3C_MasterEmitRequest(_i3c_m_handle.base, kI3C_RequestForceExit);
            return status;
        }

        I3cBuffer buffer{
            Enint,
        };
        memset(&xfer, 0, sizeof(xfer));
        xfer.slaveAddress = address;
        xfer.data         = buffer.data();
        xfer.dataSize     = 1;
        xfer.direction    = kI3C_Write;
        xfer.busType      = kI3C_TypeI3CSdr;
        xfer.flags        = kI3C_TransferDefaultFlag;
        xfer.ibiResponse  = kI3C_IbiRespAckMandatory;
        status            = I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer);
        if (status != kStatus_Success) {
            auto* base = _i3c_m_handle.base;
            // The DISEC and RSTDAA that precede this ENEC in bring-up leave the
            // target unable to raise an IBI until this ENINT byte lands. A timeout
            // that finds it holding SDA low with no bus error therefore means the
            // byte and the STOP went out and the target is already requesting the
            // IBI it was just enabled for; the SDK only gave up waiting for an idle
            // bus that the request will not allow.
            if (status == kStatus_I3C_Timeout
                && I3C_MasterGetState(base) == kI3C_MasterStateSlvReq && base->MERRWARN == 0U) {
                const uint32_t mstatus = base->MSTATUS;
                logger::info(logger::Event::I3CEnecIbiRequestV2,
                             {static_cast<uint8_t>(_port),
                              static_cast<uint8_t>(mstatus >> 0 & 0xFF),
                              static_cast<uint8_t>(mstatus >> 8 & 0xFF),
                              static_cast<uint8_t>(mstatus >> 16 & 0xFF),
                              static_cast<uint8_t>(mstatus >> 24 & 0xFF)});
                enint_answered_with_ibi_request = true;
                return status;
            }
            log_ccc_fail_detail(static_cast<uint8_t>(_port), PartDirectedByte, base);
            I3C_MasterStop(base);
        }
        return status;
    };

    // MCTRL.IBIRESP is the only IBI policy the blocking transfer API honours;
    // xfer.ibiResponse above is applied by I3C_MasterTransferNonBlocking only.
    // Under AckMandatory the controller ACKs a mid-frame IBI and then waits for
    // software to take the mandatory data byte, which a blocking transfer never
    // does, so the frame parks until the completion poll expires. Refuse IBIs
    // for the duration of the frame and restore the steady-state policy on
    // exit. A target that loses the frame this way is not stranded: the caller
    // drains the collision through force_bus_idle() and re-sends the whole CCC.
    set_ibi_nack();
    const status_t result = send_frame();
    set_ibi_ack_mandatory();
    // Re-sending ENEC after the target answered ENINT with an IBI request would
    // meet that same request every time. Accept the frame instead: the request is
    // collected under the AckMandatory policy just restored, either by the IBI
    // interrupt or by winning arbitration against the next transfer.
    if (result == kStatus_Success || enint_answered_with_ibi_request) {
        return true;
    }

    const logger::EventData data = {
        static_cast<uint8_t>(_port),
        CccEnecDirect,
        static_cast<uint8_t>(result >> 0 & 0xFF),
        static_cast<uint8_t>(result >> 8 & 0xFF),
        static_cast<uint8_t>(result >> 16 & 0xFF),
        static_cast<uint8_t>(result >> 24 & 0xFF),
    };
    logger::info(logger::Event::I3CCccErrorV2, data);
    auto status  = to_driver_status(static_cast<uint32_t>(result));
    _last_status = status;
    if (status != Status::Success) {
        const auto& task = *static_cast<nv::i3c::Task*>(_task);
        task.record_error(static_cast<uint8_t>(status));
    }
    return false;
}

bool nv::i3c::Driver::register_ibi(uint8_t address)
{
    i3c_register_ibi_addr_t ibi_record{};
    ibi_record.address[0]    = address;
    ibi_record.ibiHasPayload = true;
    I3C_MasterRegisterIBI(_i3c_m_handle.base, &ibi_record);
    _ibi_address = address;
    return true;
}

namespace {

// SDK 25.09.00 ships I3C_MasterSetIBIResponse() but 25.09.02 dropped it, and this
// driver is built against both, so arm the policy directly. Unlike
// I3C_MasterEmitIBIResponse() this leaves REQUEST at None: it only changes how the
// controller will answer a future IBI and must not emit a decision here. Clearing
// REQUEST_MASK is required because the field is write-triggered, so a plain
// read-modify-write of MCTRL would re-issue whatever request it last carried.
void set_ibi_response(I3C_Type* base, i3c_ibi_response_t response)
{
    uint32_t ctrl  = base->MCTRL;
    ctrl          &= ~(I3C_MCTRL_IBIRESP_MASK | I3C_MCTRL_REQUEST_MASK);
    ctrl          |= I3C_MCTRL_IBIRESP(static_cast<uint32_t>(response))
          | I3C_MCTRL_REQUEST(kI3C_RequestNone);
    base->MCTRL = ctrl;
}

}  // namespace

void nv::i3c::Driver::set_ibi_nack()
{
    set_ibi_response(_i3c_m_handle.base, kI3C_IbiRespNack);
}

void nv::i3c::Driver::set_ibi_ack_mandatory()
{
    set_ibi_response(_i3c_m_handle.base, kI3C_IbiRespAckMandatory);
}

bool nv::i3c::Driver::set_mrl_mwl(uint8_t address)
{
    constexpr uint8_t CccSetMrlDirect = 0x8AU;
    constexpr uint8_t CccSetMwlDirect = 0x89U;
    constexpr uint8_t MrlMwlMsb       = 0x00U;
    constexpr uint8_t MrlMwlLsb       = 0x45U;
    constexpr uint8_t IbiPayloadSize  = 0x02U;
    _last_status                      = Status::Success;

    auto send_direct_ccc = [this, address](uint8_t ccc, std::span<uint8_t> data) {
        constexpr uint8_t BoradcastAddress = 0x7EU;

        i3c_master_transfer_t xfer{};
        xfer.slaveAddress   = BoradcastAddress;
        xfer.subaddress     = ccc;
        xfer.subaddressSize = 1U;
        xfer.direction      = kI3C_Write;
        xfer.busType        = kI3C_TypeI3CSdr;
        xfer.flags          = kI3C_TransferNoStopFlag;
        xfer.ibiResponse    = kI3C_IbiRespAckMandatory;
        log_ccc_sent(static_cast<uint8_t>(_port), ccc, address);
        auto result = I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer);
        if (result != kStatus_Success) {
            I3C_MasterEmitRequest(_i3c_m_handle.base, kI3C_RequestForceExit);
            const logger::EventData log_data = {
                static_cast<uint8_t>(_port),
                ccc,
                static_cast<uint8_t>(result >> 0 & 0xFF),
                static_cast<uint8_t>(result >> 8 & 0xFF),
                static_cast<uint8_t>(result >> 16 & 0xFF),
                static_cast<uint8_t>(result >> 24 & 0xFF),
            };
            logger::info(logger::Event::I3CCccErrorV2, log_data);
            auto status  = to_driver_status(static_cast<uint32_t>(result));
            _last_status = status;
            if (status != Status::Success) {
                const auto& task = *static_cast<nv::i3c::Task*>(_task);
                task.record_error(static_cast<uint8_t>(status));
            }
            return false;
        }

        memset(&xfer, 0, sizeof(xfer));
        xfer.slaveAddress = address;
        xfer.data         = data.data();
        xfer.dataSize     = data.size();
        xfer.direction    = kI3C_Write;
        xfer.busType      = kI3C_TypeI3CSdr;
        xfer.flags        = kI3C_TransferDefaultFlag;
        xfer.ibiResponse  = kI3C_IbiRespAckMandatory;
        result            = I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer);
        if (result != kStatus_Success) {
            I3C_MasterStop(_i3c_m_handle.base);
            const logger::EventData log_data = {
                static_cast<uint8_t>(_port),
                ccc,
                static_cast<uint8_t>(result >> 0 & 0xFF),
                static_cast<uint8_t>(result >> 8 & 0xFF),
                static_cast<uint8_t>(result >> 16 & 0xFF),
                static_cast<uint8_t>(result >> 24 & 0xFF),
            };
            logger::info(logger::Event::I3CCccErrorV2, log_data);
            auto status  = to_driver_status(static_cast<uint32_t>(result));
            _last_status = status;
            if (status != Status::Success) {
                const auto& task = *static_cast<nv::i3c::Task*>(_task);
                task.record_error(static_cast<uint8_t>(status));
            }
            return false;
        }
        return true;
    };

    I3cBuffer set_mrl_data{MrlMwlMsb, MrlMwlLsb, IbiPayloadSize};
    if (!send_direct_ccc(CccSetMrlDirect, std::span<uint8_t>(set_mrl_data.data(), 3))) {
        return false;
    }

    I3cBuffer set_mwl_data{MrlMwlMsb, MrlMwlLsb};
    return send_direct_ccc(CccSetMwlDirect, std::span<uint8_t>(set_mwl_data.data(), 2));
}

bool nv::i3c::Driver::get_status(uint8_t address, uint16_t& status)
{
    constexpr uint8_t  BroadcastAddress = 0x7EU;
    constexpr uint8_t  CccGetStatus     = 0x90U;
    constexpr uint16_t WordMask         = 0xFFFF;
    _last_status                        = Status::Success;

    const auto report_failure = [this](uint32_t result) {
        constexpr uint8_t       CccGetStatus = 0x90U;
        const logger::EventData data         = {
            static_cast<uint8_t>(_port),
            CccGetStatus,
            static_cast<uint8_t>(result >> 0 & 0xFF),
            static_cast<uint8_t>(result >> 8 & 0xFF),
            static_cast<uint8_t>(result >> 16 & 0xFF),
            static_cast<uint8_t>(result >> 24 & 0xFF),
        };
        logger::info(logger::Event::I3CCccErrorV2, data);
        const auto driver_status = to_driver_status(result);
        _last_status             = driver_status;
        if (driver_status != Status::Success) {
            const auto& task = *static_cast<nv::i3c::Task*>(_task);
            task.record_error(static_cast<uint8_t>(driver_status));
        }
        return false;
    };

    // GETSTATUS is a Direct CCC and must be framed as two chained bus segments:
    //   phase 1: START + 0x7E (broadcast) + CCC 0x90, held open (No Stop)
    //   phase 2: repeated START + target address + directed read + STOP
    // The blocking API sends the CCC byte on the 0x7E broadcast address, which
    // the target recognizes as GETSTATUS. A single combined EDMA transaction
    // (StartWithBroadcastAddr) instead emits the CCC as a directed write to the
    // target address, which this CPU NACKs, so keep the blocking two-phase form.
    I3cBuffer             buffer{};
    i3c_master_transfer_t xfer{};
    xfer.slaveAddress   = BroadcastAddress;
    xfer.subaddress     = CccGetStatus;
    xfer.subaddressSize = 1U;
    xfer.direction      = kI3C_Write;
    xfer.busType        = kI3C_TypeI3CSdr;
    xfer.flags          = kI3C_TransferNoStopFlag;
    xfer.ibiResponse    = kI3C_IbiRespAckMandatory;
    auto result         = I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer);
    if (result != kStatus_Success) {
        I3C_MasterEmitRequest(_i3c_m_handle.base, kI3C_RequestForceExit);
        return report_failure(static_cast<uint32_t>(result));
    }

    memset(&xfer, 0, sizeof(xfer));
    xfer.slaveAddress = address;
    xfer.data         = buffer.data();
    xfer.dataSize     = 2U;
    xfer.direction    = kI3C_Read;
    xfer.busType      = kI3C_TypeI3CSdr;
    xfer.flags        = kI3C_TransferDefaultFlag;
    xfer.ibiResponse  = kI3C_IbiRespAckMandatory;
    result            = I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer);
    if (result != kStatus_Success) {
        I3C_MasterStop(_i3c_m_handle.base);
        return report_failure(static_cast<uint32_t>(result));
    }

    status = (buffer[0] << 8 | buffer[1]) & WordMask;
    return true;
}

bool nv::i3c::Driver::process_daa(std::span<uint8_t> address_list)
{
    constexpr uint8_t CccEntdaa        = 0x07U;
    constexpr uint8_t BroadcastAddress = 0x7EU;
    _last_status                       = Status::Success;
    /// reset list of devices
    I3C_MasterClearDeviceCount(_i3c_m_handle.base);
    /// start DAA porcess
    if constexpr (nv::ipc::MctpBridgeConfig<>::UsI3cBridgeMode) {
        log_ccc_sent(static_cast<uint8_t>(_port), CccEntdaa, BroadcastAddress);
    }
    auto result = I3C_MasterProcessDAA(
        _i3c_m_handle.base, address_list.data(), address_list.size());
    if (result != kStatus_Success) {
        _last_status = to_driver_status(static_cast<uint32_t>(result));
        I3C_MasterStop(_i3c_m_handle.base);
        nv::info("fail to process daa %d\n", result);
        logger::info(logger::Event::I3CFailedToProcessDaaV2,
                     {static_cast<uint8_t>(_port),
                      static_cast<uint8_t>(result >> 0 & 0xFF),
                      static_cast<uint8_t>(result >> 8 & 0xFF),
                      static_cast<uint8_t>(result >> 16 & 0xFF),
                      static_cast<uint8_t>(result >> 24 & 0xFF)});
        return false;
    }
    /// list I3C devices
    i3c_device_info_t* list  = nullptr;
    uint8_t            count = 0;
    list                     = I3C_MasterGetDeviceListAfterDAA(_i3c_m_handle.base, &count);
    for (uint8_t index = 0; index < count; index++) {
        nv::info("I3C device [%d], vendor#: 0x%04x, part#: 0x%8x, addr: 0x%2x\n",
                 index,
                 list[index].vendorID,
                 list[index].partNumber,
                 list[index].dynamicAddr);
        nv::logger::info(
            nv::logger::Event::I3CSetAddrV2,
            {static_cast<uint8_t>(_port), static_cast<uint8_t>(list[index].dynamicAddr)});
    }
    if (count != address_list.size() && _is_gpu == false) {
        _last_status = Status::Error;
        logger::info(logger::Event::I3CDaaMismatchV2,
                     {static_cast<uint8_t>(_port), count, address_list.size()});
        return false;
    }
    _daa_done = true;
    return true;
}

bool nv::i3c::Driver::write(uint8_t address, std::span<uint8_t> buffer)
{
    uint8_t               length = 0;
    i3c_master_transfer_t xfer{};
    xfer.slaveAddress = address;
    xfer.data         = buffer.data();
    xfer.dataSize     = buffer.size();
    xfer.direction    = kI3C_Write;
    xfer.busType      = kI3C_TypeI3CSdr;
    xfer.flags        = kI3C_TransferDefaultFlag;
    // I3C_MasterTransferEDMA writes this field into MCTRL.IBIRESP, so it sets the
    // controller-wide IBI policy rather than just describing this frame.
    if constexpr (nv::ipc::MctpBridgeConfig<>::UsI3cBridgeMode) {
        // Preserve whatever policy is armed. The first post-recovery DiscoveryNotify
        // is transmitted while IBIs are NACKed; Task::transmit() switches to
        // AckMandatory only after this write succeeds.
        xfer.ibiResponse = static_cast<i3c_ibi_response_t>(
            (_i3c_m_handle.base->MCTRL & I3C_MCTRL_IBIRESP_MASK) >> I3C_MCTRL_IBIRESP_SHIFT);
    }
    else {
        // Nothing arms the policy on non-bridge products (register_ibi() and the
        // enec(address) NACK window are both bridge-only), so preserving it here
        // would leave the post-I3C_MasterInit() default of Ack-without-mandatory-byte.
        xfer.ibiResponse = kI3C_IbiRespAckMandatory;
    }
    auto        status = transfer(&xfer, length);
    const auto& task   = *static_cast<nv::i3c::Task*>(_task);
    if (status != Status::Success) {
        if (_error_log_count < ErrorLogThreshold) {
            _error_log_count++;
            const uint32_t              mstatus = _i3c_m_handle.base->MSTATUS;
            const nv::logger::EventData data    = {
                static_cast<uint8_t>(_port),
                static_cast<uint8_t>(status),
                static_cast<uint8_t>(_i3c_m_handle.state),
                static_cast<uint8_t>(mstatus >> 0 & 0xFF),
                static_cast<uint8_t>(mstatus >> 8 & 0xFF),
                static_cast<uint8_t>(mstatus >> 16 & 0xFF),
                static_cast<uint8_t>(mstatus >> 24 & 0xFF),
            };
            nv::logger::info(nv::logger::Event::I3CWriteFailV2, data);
        }

        task.record_error(static_cast<uint8_t>(status));
    }
    return status == Status::Success;
}

bool nv::i3c::Driver::read(uint8_t address, std::span<uint8_t> buffer, uint8_t& length)
{
    i3c_master_transfer_t xfer{};
    xfer.slaveAddress  = address;
    xfer.data          = buffer.data();
    xfer.dataSize      = buffer.size();
    xfer.direction     = kI3C_Read;
    xfer.busType       = kI3C_TypeI3CSdr;
    xfer.flags         = kI3C_TransferDefaultFlag;
    xfer.ibiResponse   = kI3C_IbiRespAckMandatory;
    auto        status = transfer(&xfer, length);
    const auto& task   = *static_cast<nv::i3c::Task*>(_task);
    if (status != Status::Success) {
        if (_error_log_count < ErrorLogThreshold) {
            _error_log_count++;
            const uint32_t              mstatus = _i3c_m_handle.base->MSTATUS;
            const nv::logger::EventData data    = {
                static_cast<uint8_t>(_port),
                static_cast<uint8_t>(status),
                static_cast<uint8_t>(_i3c_m_handle.state),
                static_cast<uint8_t>(mstatus >> 0 & 0xFF),
                static_cast<uint8_t>(mstatus >> 8 & 0xFF),
                static_cast<uint8_t>(mstatus >> 16 & 0xFF),
                static_cast<uint8_t>(mstatus >> 24 & 0xFF),
            };
            nv::logger::info(nv::logger::Event::I3CReadFailV2, data);
        }
        task.record_error(static_cast<uint8_t>(status));
    }
    return status == Status::Success;
}

nv::i3c::Driver::Status nv::i3c::Driver::transfer(void* args, uint8_t& length)
{
    using namespace std::chrono;
    const auto&    task   = *static_cast<nv::i3c::Task*>(_task);
    const uint8_t  Retry  = 3;
    const uint32_t Mask   = Event::Success | Event::Error | Event::Nack;
    Status         status = Status::Error;
    for (uint8_t i = Retry; i > 0; i--) {
        status_t result = kStatus_Success;
        // WAR SDK API cannot be interrupted
        taskENTER_CRITICAL();
        result = I3C_MasterTransferEDMA(
            _i3c_m_handle.base, &_i3c_m_handle, static_cast<i3c_master_transfer_t*>(args));
        taskEXIT_CRITICAL();
        if (result != kStatus_Success) {
            status = Status::Busy;
            if constexpr (nv::ipc::MctpBridgeConfig<>::UsI3cBridgeMode) {
                // Busy-at-entry: I3C_MasterTransferEDMA refused to start because the
                // controller was not Idle. This is either an IBI the CPU is pushing
                // right now, a stuck EDMA handle left non-idle by a prior aborted
                // transfer, or an intermittent CPU Slave-Start that held the
                // controller out of Idle at this instant.
                const auto state = I3C_MasterGetState(_i3c_m_handle.base);
                // An IBI in flight parks the controller in IbiAck/IbiRcv for
                // microseconds and completes on its own. Aborting the handle there
                // flushes the RX FIFO, drops handle->state back to Idle, and leaves
                // the frame with no STOP -- I3C_MasterStop() is guarded on NormAct
                // and silently no-ops from IbiRcv -- so force_bus_idle() is left with
                // nothing to recover from and falls through to a controller reset.
                // Leave the IBI alone; the retry below picks it up, and the
                // try_recover() at the end of this function is still the backstop.
                if (state != kI3C_MasterStateIbiAck && state != kI3C_MasterStateIbiRcv) {
                    // force_bus_idle() inspects the sticky SLVSTART flag and services
                    // any pending CPU Slave-Start (AutoIbi + ACK + drain), which
                    // sampling the live STATE here would miss. Only the CPU link
                    // raises Slave-Starts this way; elsewhere the retry below is left
                    // to try_recover(), whose MaxConsecutiveRecoveries cap
                    // force_bus_idle() does not honour.
                    I3C_MasterTransferAbortEDMA(_i3c_m_handle.base, &_i3c_m_handle);
                    (void)force_bus_idle();
                }
            }
            task.delay(10ms);
            continue;
        }
        auto events = _event.wait(Mask, true, false, 100ms);
        if (events.value() & Event::Success) {
            length = static_cast<uint8_t>(_i3c_m_handle.transferCount & UINT8_MAX);
            status = Status::Success;
            break;
        }
        if (events.value() & Event::Nack) {
            status = Status::Nack;
            task.delay(10ms);
            continue;
        }
        if (events.value() & Event::Error) {
            status = Status::Error;
            task.delay(10ms);
            continue;
        }
        if (!events.value()) {
            if constexpr (nv::ipc::EnableSmartDMA) {
                sys::smartdma::Driver::log_status(_i3c_m_handle);
            }
            status = Status::Timeout;
            task.delay(10ms);
        }
    }

    if (status == Status::Busy) {
        try_recover();
    }
    // Could recovery again if transfer success
    else if (status == Status::Success) {
        _consecutive_recoveries = 0;
    }
    return status;
}

void nv::i3c::Driver::try_recover()
{
    if (!_daa_done) {
        return;
    }
    const uint32_t              mstatus = _i3c_m_handle.base->MSTATUS;
    const nv::logger::EventData data    = {static_cast<uint8_t>(_port),
                                           static_cast<uint8_t>(_i3c_m_handle.state),
                                           static_cast<uint8_t>(mstatus >> 0 & 0xFF),
                                           static_cast<uint8_t>(mstatus >> 8 & 0xFF),
                                           static_cast<uint8_t>(mstatus >> 16 & 0xFF),
                                           static_cast<uint8_t>(mstatus >> 24 & 0xFF),
                                           static_cast<uint8_t>(_consecutive_recoveries & 0xFF),
                                           0};
    if (_consecutive_recoveries < MaxConsecutiveRecoveries) {
        nv::logger::info(nv::logger::Event::I3CBusRecovery, data);
        I3C_MasterTransferAbortEDMA(_i3c_m_handle.base, &_i3c_m_handle);
        deinit();
        init();
        _consecutive_recoveries++;
    }
    else if (_consecutive_recoveries == MaxConsecutiveRecoveries) {
        nv::logger::info(nv::logger::Event::I3CBusRecoveryGiveUp, data);
        _consecutive_recoveries++;
    }
}

void nv::i3c::Driver::on_ibi(void* args)
{
    auto& task   = *static_cast<nv::i3c::Task*>(_task);
    auto& handle = *static_cast<i3c_master_edma_handle_t*>(args);
    if constexpr (nv::ipc::EnableSmartDMA) {
        auto instance = I3C_GetInstance(handle.base);
        auto ibi_data = sys::smartdma::Driver::get_ibi_data(instance);
        task.on_ibi(
            ibi_data->address,
            std::span<volatile uint8_t>(static_cast<volatile uint8_t*>(&ibi_data->buf[0]),
                                        static_cast<size_t>(ibi_data->payload_size)));
    }
    else {
        task.on_ibi(handle.ibiAddress,
                    std::span<uint8_t>(handle.ibiBuff, handle.ibiPayloadSize));
    }
}

void nv::i3c::Driver::on_master_request_ibi(void* args)
{
    constexpr uint8_t PendingReadId = 0xAEU;

    auto&   task    = *static_cast<nv::i3c::Task*>(_task);
    auto&   handle  = *static_cast<i3c_master_edma_handle_t*>(args);
    uint8_t address = handle.ibiAddress;

    if constexpr (nv::ipc::EnableSmartDMA) {
        const auto instance = I3C_GetInstance(handle.base);
        address             = sys::smartdma::Driver::get_ibi_data(instance)->address;
    }

    volatile uint8_t pending_read = PendingReadId;
    task.on_ibi(address, std::span<volatile uint8_t>(&pending_read, 1));
}

void nv::i3c::Driver::set_event(Event event)
{
    (void)_event.set(event);
    // TODO error log?
}

nv::i2c::I2cStatus nv::i3c::Driver::i2c(uint8_t            address,
                                        std::span<uint8_t> write_buffer,
                                        std::span<uint8_t> read_buffer)
{
    auto result = nv::i2c::I2cStatus::Error;
    if (write_buffer.size() != 0 && read_buffer.size() == 0) {
        result = i2c_write(address, write_buffer);
    }
    else if (write_buffer.size() == 0 && read_buffer.size() != 0) {
        result = i2c_read(address, read_buffer);
    }
    else if (write_buffer.size() != 0 && read_buffer.size() != 0) {
        result = i2c_write_read(address, write_buffer, read_buffer);
    }
    else {
        /// TODO quick write/read
    }

    /// Arbitration lost: dump the first 16 bytes of the write buffer (what we
    /// were trying to send) as two 8-byte events. Throttled: nv::logger::info
    /// writes flash, so an arblost storm could brick the MCU.
    if (result == nv::i2c::I2cStatus::ArbLost && _i2c_arblost_log_count < ErrorLogThreshold) {
        _i2c_arblost_log_count++;
        nv::logger::info(nv::logger::Event::I3CI2CArbLostV2,
                         {write_buffer[0],
                          write_buffer[1],
                          write_buffer[2],
                          write_buffer[3],
                          write_buffer[4],
                          write_buffer[5],
                          write_buffer[6],
                          write_buffer[7]});
        /// bytes 8..15
        nv::logger::info(nv::logger::Event::I3CI2CArbLostV2,
                         {write_buffer[8],
                          write_buffer[9],
                          write_buffer[10],
                          write_buffer[11],
                          write_buffer[12],
                          write_buffer[13],
                          write_buffer[14],
                          write_buffer[15]});
    }

    return result;
}

nv::i2c::I2cStatus nv::i3c::Driver::i2c_write(uint8_t address, std::span<uint8_t> buffer)
{
    i3c_master_transfer_t xfer{
        .flags        = kI3C_TransferDefaultFlag,
        .slaveAddress = address,
        .direction    = kI3C_Write,
        .data         = buffer.data(),
        .dataSize     = buffer.size(),
        .busType      = kI3C_TypeI2C,
    };
    nv::i2c::I2cStatus result = nv::i2c::I2cStatus::Ok;
    taskENTER_CRITICAL();
    auto status = I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer);
    taskEXIT_CRITICAL();
    result = to_status(status);
    if (result != nv::i2c::I2cStatus::Ok) {
        i2c_stop();
        if (_i2c_error_log_count < ErrorLogThreshold) {
            _i2c_error_log_count++;
            const uint32_t              mstatus = _i3c_m_handle.base->MSTATUS;
            const nv::logger::EventData data    = {
                static_cast<uint8_t>(_port),
                static_cast<uint8_t>(result),
                address,
                static_cast<uint8_t>(_i3c_m_handle.state),
                static_cast<uint8_t>(mstatus >> 0 & 0xFF),
                static_cast<uint8_t>(mstatus >> 8 & 0xFF),
                static_cast<uint8_t>(mstatus >> 16 & 0xFF),
                static_cast<uint8_t>(mstatus >> 24 & 0xFF),
            };
            nv::logger::info(nv::logger::Event::I3CI2CWriteFailV2, data);
        }
        const auto& task = *static_cast<nv::i3c::Task*>(_task);
        task.record_error((static_cast<uint8_t>(to_driver_status(result))));
    }
    return result;
}

nv::i2c::I2cStatus nv::i3c::Driver::i2c_read(uint8_t address, std::span<uint8_t> buffer)
{
    i3c_master_transfer_t xfer{
        .flags        = kI3C_TransferDefaultFlag,
        .slaveAddress = address,
        .direction    = kI3C_Read,
        .data         = buffer.data(),
        .dataSize     = buffer.size(),
        .busType      = kI3C_TypeI2C,
    };
    nv::i2c::I2cStatus result = nv::i2c::I2cStatus::Ok;
    taskENTER_CRITICAL();
    result = to_status(I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer));
    taskEXIT_CRITICAL();
    if (result != nv::i2c::I2cStatus::Ok) {
        i2c_stop();
        if (_i2c_error_log_count < ErrorLogThreshold) {
            _i2c_error_log_count++;
            const uint32_t              mstatus = _i3c_m_handle.base->MSTATUS;
            const nv::logger::EventData data    = {
                static_cast<uint8_t>(_port),
                static_cast<uint8_t>(result),
                address,
                static_cast<uint8_t>(_i3c_m_handle.state),
                static_cast<uint8_t>(mstatus >> 0 & 0xFF),
                static_cast<uint8_t>(mstatus >> 8 & 0xFF),
                static_cast<uint8_t>(mstatus >> 16 & 0xFF),
                static_cast<uint8_t>(mstatus >> 24 & 0xFF),
            };
            nv::logger::info(nv::logger::Event::I3CI2CReadFailV2, data);
        }

        const auto& task = *static_cast<nv::i3c::Task*>(_task);
        task.record_error((static_cast<uint8_t>(to_driver_status(result))));
    }
    return result;
}

nv::i2c::I2cStatus nv::i3c::Driver::i2c_write_read(uint8_t            address,
                                                   std::span<uint8_t> write_buffer,
                                                   std::span<uint8_t> read_buffer)
{
    i3c_master_transfer_t xfer{
        .slaveAddress = address,
        .busType      = kI3C_TypeI2C,
    };
    /// write phase
    xfer.direction            = kI3C_Write;
    xfer.data                 = write_buffer.data();
    xfer.dataSize             = write_buffer.size();
    xfer.flags                = kI3C_TransferNoStopFlag;
    nv::i2c::I2cStatus result = nv::i2c::I2cStatus::Ok;
    taskENTER_CRITICAL();
    result = to_status(I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer));
    taskEXIT_CRITICAL();
    if (result != nv::i2c::I2cStatus::Ok) {
        i2c_stop();
        if (_i2c_error_log_count < ErrorLogThreshold) {
            _i2c_error_log_count++;
            const uint32_t              mstatus = _i3c_m_handle.base->MSTATUS;
            const nv::logger::EventData data    = {
                static_cast<uint8_t>(_port),
                static_cast<uint8_t>(result),
                address,
                static_cast<uint8_t>(_i3c_m_handle.state),
                static_cast<uint8_t>(mstatus >> 0 & 0xFF),
                static_cast<uint8_t>(mstatus >> 8 & 0xFF),
                static_cast<uint8_t>(mstatus >> 16 & 0xFF),
                static_cast<uint8_t>(mstatus >> 24 & 0xFF),
            };
            nv::logger::info(nv::logger::Event::I3CI2CWriteFailV2, data);
        }
        const auto& task = *static_cast<nv::i3c::Task*>(_task);
        task.record_error((static_cast<uint8_t>(to_driver_status(result))));
        return result;
    }
    /// read phase
    xfer.direction = kI3C_Read;
    xfer.data      = read_buffer.data();
    xfer.dataSize  = read_buffer.size();
    xfer.flags     = kI3C_TransferRepeatedStartFlag;
    taskENTER_CRITICAL();
    result = to_status(I3C_MasterTransferBlocking(_i3c_m_handle.base, &xfer));
    taskEXIT_CRITICAL();
    if (result != nv::i2c::I2cStatus::Ok) {
        i2c_stop();
        if (_i2c_error_log_count < ErrorLogThreshold) {
            _i2c_error_log_count++;
            const uint32_t              mstatus = _i3c_m_handle.base->MSTATUS;
            const nv::logger::EventData data    = {
                static_cast<uint8_t>(_port),
                static_cast<uint8_t>(result),
                address,
                static_cast<uint8_t>(_i3c_m_handle.state),
                static_cast<uint8_t>(mstatus >> 0 & 0xFF),
                static_cast<uint8_t>(mstatus >> 8 & 0xFF),
                static_cast<uint8_t>(mstatus >> 16 & 0xFF),
                static_cast<uint8_t>(mstatus >> 24 & 0xFF),
            };
            nv::logger::info(nv::logger::Event::I3CI2CReadFailV2, data);
        }
        const auto& task = *static_cast<nv::i3c::Task*>(_task);
        task.record_error((static_cast<uint8_t>(to_driver_status(result))));
        return result;
    }
    return nv::i2c::I2cStatus::Ok;
}

void sys::i3c::Driver::i2c_stop()
{
    /// WAR driver didn't send stop correctly when NACK
    auto mctrl_val             = _i3c_m_handle.base->MCTRL;
    mctrl_val                 &= ~I3C_MCTRL_TYPE_MASK;
    mctrl_val                 |= I3C_MCTRL_TYPE(kI3C_TypeI2C);
    _i3c_m_handle.base->MCTRL  = mctrl_val;
    I3C_MasterEmitRequest(_i3c_m_handle.base, kI3C_RequestEmitStop);
}

nv::i2c::I2cStatus sys::i3c::Driver::to_status(status_t status)
{
    switch (status) {
        case kStatus_Success   : return nv::i2c::I2cStatus::Ok;
        case kStatus_I3C_Busy  : return nv::i2c::I2cStatus::Busy;
        case kStatus_I3C_Nak   : return nv::i2c::I2cStatus::Nak;
        case kStatus_I3C_IBIWon: return nv::i2c::I2cStatus::ArbLost;
        default                : return nv::i2c::I2cStatus::Error;
    }
}

nv::i3c::Driver::Status nv::i3c::Driver::to_driver_status(nv::i2c::I2cStatus status)
{
    switch (status) {
        case nv::i2c::I2cStatus::Ok     : return nv::i3c::Driver::Status::Success;
        case nv::i2c::I2cStatus::Busy   : return nv::i3c::Driver::Status::Busy;
        case nv::i2c::I2cStatus::Nak    : return nv::i3c::Driver::Status::Nack;
        case nv::i2c::I2cStatus::Timeout: return nv::i3c::Driver::Status::Timeout;
        default                         : return nv::i3c::Driver::Status::Error;
    }
}

nv::i3c::Driver::Status nv::i3c::Driver::to_driver_status(uint32_t status)
{
    switch (status) {
        case kStatus_Success    : return nv::i3c::Driver::Status::Success;
        case kStatus_I3C_Busy   : return nv::i3c::Driver::Status::Busy;
        case kStatus_I3C_Nak    : return nv::i3c::Driver::Status::Nack;
        case kStatus_I3C_Timeout: return nv::i3c::Driver::Status::Timeout;
        default                 : return nv::i3c::Driver::Status::Error;
    }
}

nv::perf_mon::OobBus nv::i3c::Driver::i3c_port_to_oob_bus(Port port)
{
    switch (port) {
        case Port::Zero: return nv::perf_mon::OobBus::I3cPort0; break;
        case Port::One : return nv::perf_mon::OobBus::I3cPort1; break;
        default        : return nv::perf_mon::OobBus::End; break;
    }
}

bool nv::i3c::Driver::ocp_query_interface_mastering(uint8_t address, bool& enable)
{
    constexpr uint8_t  Command = 0x25;
    nv::i2c::I2cBuffer write_buffer{};
    nv::i2c::I2cBuffer read_buffer{};
    write_buffer[0] = Command;
    auto status     = i2c(address,
                      std::span<uint8_t>(write_buffer.data(), 1),
                      std::span<uint8_t>(read_buffer.data(), 4));
    if (status != nv::i2c::I2cStatus::Ok) {
        return false;
    }
    enable = read_buffer[3] == 1 ? true : false;
    return true;
}

bool nv::i3c::Driver::ocp_enable_interface_mastering(uint8_t address)
{
    constexpr uint8_t  Command = 0x25;
    constexpr uint8_t  Length  = 0x03;
    nv::i2c::I2cBuffer write_buffer{};
    nv::i2c::I2cBuffer read_buffer{};
    write_buffer[0] = Command;
    write_buffer[1] = Length;
    write_buffer[2] = 0x00;
    write_buffer[3] = 0x00;
    write_buffer[4] = 0x01;
    auto status     = i2c(address,
                      std::span<uint8_t>(write_buffer.data(), 5),
                      std::span<uint8_t>(read_buffer.data(), 0));
    return status == nv::i2c::I2cStatus::Ok ? true : false;
}

bool nv::i3c::Driver::gpu_query_i3c_mode(uint8_t address, bool& i3c)
{
    constexpr uint8_t  Register = 0x00;
    bool               result   = false;
    nv::i2c::I2cBuffer write_buffer{};
    nv::i2c::I2cBuffer read_buffer{};
    write_buffer[0] = Register;
    auto status     = i2c(address,
                      std::span<uint8_t>(write_buffer.data(), 1),
                      std::span<uint8_t>(read_buffer.data(), 1));
    switch (status) {
        case nv::i2c::I2cStatus::Ok:
            i3c    = false;
            result = true;
            break;
        case nv::i2c::I2cStatus::Nak:
            i3c    = true;
            result = true;
            break;
        default: result = false;
    }
    return result;
}

bool nv::i3c::Driver::gpu_configure_cms1(uint8_t address)
{
    constexpr uint8_t  Command = 0x29;
    nv::i2c::I2cBuffer write_buffer{};
    nv::i2c::I2cBuffer read_buffer{};
    write_buffer[0] = Command;
    write_buffer[1] = 0x06;
    write_buffer[2] = 0x01;
    write_buffer[3] = 0x00;
    write_buffer[4] = 0x00;
    write_buffer[5] = 0x00;
    write_buffer[6] = 0x00;
    write_buffer[7] = 0x00;
    auto status     = i2c(address,
                      std::span<uint8_t>(write_buffer.data(), 8),
                      std::span<uint8_t>(read_buffer.data(), 0));
    return status == nv::i2c::I2cStatus::Ok ? true : false;
}

bool nv::i3c::Driver::gpu_program_cms1(uint8_t address, std::span<uint8_t> buffer)
{
    constexpr uint8_t  Command = 0x2b;
    nv::i2c::I2cBuffer write_buffer{};
    nv::i2c::I2cBuffer read_buffer{};
    write_buffer[0] = Command;
    write_buffer[1] = static_cast<uint8_t>(buffer.size());
    std::memcpy(write_buffer.data() + 2, buffer.data(), buffer.size());
    auto status = i2c(address,
                      std::span<uint8_t>(write_buffer.data(), 2 + buffer.size()),
                      std::span<uint8_t>(read_buffer.data(), 0));
    return status == nv::i2c::I2cStatus::Ok ? true : false;
}

bool nv::i3c::Driver::gpu_read_cms1(uint8_t address, std::span<uint8_t> buffer)
{
    constexpr uint8_t  Command = 0x2b;
    nv::i2c::I2cBuffer write_buffer{};
    nv::i2c::I2cBuffer read_buffer{};
    write_buffer[0] = Command;
    auto status     = i2c(address,
                      std::span<uint8_t>(write_buffer.data(), 1),
                      std::span<uint8_t>(read_buffer.data(), buffer.size() + 1));
    if (status == nv::i2c::I2cStatus::Ok) {
        std::memcpy(buffer.data(), read_buffer.data() + 1, buffer.size());
        return true;
    }
    return false;
}

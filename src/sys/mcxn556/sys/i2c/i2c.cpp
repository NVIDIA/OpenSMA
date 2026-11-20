/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
#include "sys/i2c/i2c.h"

#include "fsl_clock.h"
#include <algorithm>
#include <cstdint>
#include <cstring>

#include "nv/i2c/mutex.h"
#include "nv/i2c/task.h"
#include "nv/i2c/vr_i2c_task.h"
#include "nv/i2c/smb_direct.h"
#include "nv/i2c/helper.h"
#include "nv/ipc/mutex.h"
#include "nv/i2c/i2c_forward.h"
#include "sys/i2c/i2c_forward_dest.h"
#include "nv/i2c/eeprom_cache.h"
#include "nv/logger/log.h"
#include "nv/logger/task.h"
#include "sys/i2c/utils.h"

#include "nv/nv.h"
#include "nv/ipc/supervisor.h"
#include "nv/ctimer/ctimer.h"

using namespace nv;
using namespace sys::i2c;

namespace sys::i2c {

namespace {
constexpr bool has_emulation_slave_function()
{
    for (const auto& entry : nv::ipc::SlaveFunctionTable) {
        if (entry.enabled && entry.function == nv::i2c::SlaveFunction::Emulation) {
            return true;
        }
    }
    return false;
}
}  // namespace

void download_log_in_isr(const uint8_t CurrentSession, I2cBuffer& log_buffer_data)
{
    nv::logger::Dlreq       res_log{};
    const nv::logger::Dlreq req_log{.session = CurrentSession};

    auto& task = nv::ipc::Supervisor::inst().task(nv::ipc::TaskId::Logger);
    // NOLINTNEXTLINE(*-reinterpret-cast)
    auto& logger_task = reinterpret_cast<nv::logger::Task&>(task);
    logger_task.get_logger_obj().process_download(req_log, res_log);

    log_buffer_data[0] = res_log.session;
    log_buffer_data[1] = res_log.size;
    memcpy(&log_buffer_data[2], std::bit_cast<uint8_t*>(res_log.data.data()), res_log.size);
}

static_assert(nv::ipc::SlaveFunctionTable.size() < InvalidSlaveFunctionIndex,
              "SlaveFunctionTable must not grow into InvalidSlaveFunctionIndex");

/// Index into nv::ipc::SlaveFunctionTable for (port, address);
/// InvalidSlaveFunctionIndex when the address is not claimed (callers
/// default to MCTP handling)
uint8_t get_slave_function(nv::i2c::Port port, uint8_t address)
{
    for (uint8_t i = 0; i < nv::ipc::SlaveFunctionTable.size(); i++) {
        const auto& entry = nv::ipc::SlaveFunctionTable.at(i);
        if (entry.enabled && entry.port == port && entry.address == address) {
            return i;
        }
    }
    return InvalidSlaveFunctionIndex;
}

/// Shared weak default: no I2cForward destination on this project. A project
/// that configures a forward provides a strong override in sys::i2c (see
/// i2c_forward_dest.h and e.g. e4188/core0/i2c_forward_dest.cpp).
// NOLINTNEXTLINE(cert-dcl37-c,cert-dcl51-cpp)
__attribute__((weak)) nv::ipc::TaskId forward_task_id(nv::i2c::Port /*source_port*/)
{
    return nv::ipc::TaskId::End;
}

/// EEPROM slave callback - handles all EEPROM cache operations
void Driver::target_callback_eeprom([[maybe_unused]] LPI2C_Type* base,
                                    lpi2c_slave_transfer_t*      transfer,
                                    void*                        user_data)
{
    using namespace nv::i2c;
    auto  context = static_cast<TargetContex*>(user_data);
    auto& task    = nv::ipc::Supervisor::inst().task(nv::ipc::EepromTaskId);
    // NOLINTNEXTLINE(*-reinterpret-cast)
    auto& eeprom_task = reinterpret_cast<nv::i2c::Task&>(task);
    auto& cache       = eeprom_task.eeprom_cache();

    const std::span<uint8_t> Buffer = context->buffer;

    switch (transfer->event) {
        case kLPI2C_SlaveAddressMatchEvent:
            transfer->data     = nullptr;
            transfer->dataSize = 0;
            if (transfer->receivedAddress & 0x01U) {
                if (cache.use_2byte_addr()) {
                    const uint16_t eeprom_addr = (static_cast<uint16_t>(Buffer[2]) << 8)
                                               | Buffer[3];
                    cache.set_addr_ptr(eeprom_addr);
                }
                else {
                    cache.set_addr_ptr(Buffer[2]);
                }
            }
            break;
        case kLPI2C_SlaveTransmitEvent: {
            context->transmit = true;
            Buffer[0]         = cache.read(cache.get_addr_ptr());
            cache.inc_addr_ptr();
            transfer->data     = Buffer.subspan(0, 1).data();
            transfer->dataSize = 1;
            break;
        }
        case kLPI2C_SlaveReceiveEvent:
            context->transmit  = false;
            transfer->data     = Buffer.subspan(2).data();
            transfer->dataSize = Buffer.subspan(2).size();
            break;
        case kLPI2C_SlaveCompletionEvent:
            if (!context->transmit && transfer->transferredCount > 0) {
                const uint8_t AddrBytes = cache.use_2byte_addr() ? 2 : 1;
                if (transfer->transferredCount >= AddrBytes) {
                    uint16_t eeprom_addr = 0;
                    if (cache.use_2byte_addr()) {
                        eeprom_addr = (static_cast<uint16_t>(Buffer[2]) << 8) | Buffer[3];
                    }
                    else {
                        eeprom_addr = Buffer[2];
                    }
                    cache.set_addr_ptr(eeprom_addr);
                    for (size_t i = AddrBytes; i < transfer->transferredCount; i++) {
                        cache.write(cache.get_addr_ptr(), Buffer[2 + i]);
                        cache.inc_addr_ptr();
                    }
                }
            }
            break;
        default: break;
    }
}

void Driver::target_callback_lookup(LPI2C_Type*             base,
                                    lpi2c_slave_transfer_t* transfer,
                                    void*                   user_data)
{
    auto context = static_cast<TargetContex*>(user_data);

    // Determine slave function on address match
    if (transfer->event == kLPI2C_SlaveAddressMatchEvent) {
        const auto address  = static_cast<uint8_t>(transfer->receivedAddress & 0xFF) >> 1U;
        const auto instance = LPI2C_GetInstance(base);
        NV_ASSERT(instance < nv::common::to_underlying(nv::i2c::Port::End));
        const auto port      = static_cast<nv::i2c::Port>(instance);
        const auto index     = get_slave_function(port, address);
        context->target_addr = address;
        // defaults for unclaimed addresses: MCTP/SMBus handling, no forward
        context->function    = SlaveFunction::Mctp;
        context->forward_dst = nv::ipchandler::Id::Unuse;
        if (index != InvalidSlaveFunctionIndex) {
            const auto& entry = nv::ipc::SlaveFunctionTable.at(index);
            context->function = entry.function;
            if (entry.function == SlaveFunction::I2cForward) {
                const auto dest = forward_task_id(port);
                if (dest != nv::ipc::TaskId::End) {
                    context->forward_dst = entry.forward_dst;
                    // NOLINTNEXTLINE(*-reinterpret-cast) same idiom as the EEPROM bridge
                    nv::i2c::forward::set_window(
                        reinterpret_cast<nv::i2c::Task&>(nv::ipc::Supervisor::inst().task(dest))
                            .forward_state(),
                        entry.shadow_base,
                        entry.shadow_len);
                }
            }
        }
    }

    // Dispatch to appropriate slave callback
    switch (context->function) {
        case SlaveFunction::Eeprom: target_callback_eeprom(base, transfer, user_data); break;
        case SlaveFunction::Erot  : target_callback_erot(base, transfer, user_data); break;
        case SlaveFunction::I2cForward:
            target_callback_i2c_forward(base, transfer, user_data);
            break;
        case SlaveFunction::Emulation:
            target_callback_emulation(base, transfer, user_data);
            break;
        case SlaveFunction::Mctp:
        default                 : target_callback(base, transfer, user_data); break;
    }
}
/// EROT slave callback - handles OCP Recovery 2-byte writes (reg, value) from
/// a downstream EROT acting as I2C master after its boot completes.
void Driver::target_callback_erot([[maybe_unused]] LPI2C_Type* base,
                                  lpi2c_slave_transfer_t*      transfer,
                                  void*                        user_data)
{
    using namespace nv::i2c;
    auto                     context = static_cast<TargetContex*>(user_data);
    auto                     task    = static_cast<Task*>(context->task);
    const std::span<uint8_t> Buffer  = context->buffer;

    switch (transfer->event) {
        case kLPI2C_SlaveAddressMatchEvent:
            transfer->data     = nullptr;
            transfer->dataSize = 0;
            break;
        case kLPI2C_SlaveReceiveEvent:
            context->transmit  = false;
            transfer->data     = Buffer.subspan(2).data();
            transfer->dataSize = Buffer.subspan(2).size();
            break;
        case kLPI2C_SlaveCompletionEvent:
            if (!context->transmit && transfer->transferredCount == 2) {
                task->handle_erot_recovery(/*reg=*/context->buffer[2],
                                           /*value=*/context->buffer[3]);
            }
            break;
        default: break;
    }
}

/// DAC forward slave callback - target writes are queued verbatim to the I2C
/// task owning the real DAC bus; reads are served from the local shadow
/// (see nv/i2c/i2c_forward.h, bug 6400342).
void Driver::target_callback_i2c_forward(LPI2C_Type*             base,
                                         lpi2c_slave_transfer_t* transfer,
                                         void*                   user_data)
{
    using namespace nv::i2c;
    const auto dest = forward_task_id(static_cast<nv::i2c::Port>(LPI2C_GetInstance(base)));
    if (dest == nv::ipc::TaskId::End) {
        return;  // no forward destination on this project; entries cannot match
    }
    auto                     context = static_cast<TargetContex*>(user_data);
    const std::span<uint8_t> Buffer  = context->buffer;
    /// shadow home: the destination task's private MPU block, resolved on
    /// demand from the per-project weak forward_task_id() - the same idiom the
    /// EEPROM bridge uses for its cache above
    // NOLINTNEXTLINE(*-reinterpret-cast) same idiom as the EEPROM bridge
    auto& fwd = reinterpret_cast<nv::i2c::Task&>(nv::ipc::Supervisor::inst().task(dest))
                    .forward_state();

    switch (transfer->event) {
        case kLPI2C_SlaveAddressMatchEvent:
            transfer->data     = nullptr;
            transfer->dataSize = 0;
            if ((transfer->receivedAddress & 0x01U) != 0U) {
                /// combined write -> repeated-start -> read: the register
                /// selection from the write phase is already in the buffer
                /// but the completion event only fires after the read, so
                /// grab it now (same approach as the EEPROM bridge above).
                /// Gated on rx_seen: on a standalone read buffer[2] is stale,
                /// possibly another slave function's traffic on the shared
                /// per-port buffer, and the selection must stay where the
                /// last write left it.
                if (context->rx_seen) {
                    forward::select_reg(fwd, context->buffer[2]);
                    context->rx_seen = false;
                }
            }
            else {
                context->rx_seen = false;  // new write transaction starts clean
            }
            break;
        case kLPI2C_SlaveReceiveEvent:
            context->transmit  = false;
            context->rx_seen   = true;
            transfer->data     = Buffer.subspan(2).data();
            transfer->dataSize = Buffer.subspan(2).size();
            break;
        case kLPI2C_SlaveTransmitEvent:
            context->transmit  = true;
            Buffer[0]          = forward::shadow_read(fwd, forward::selected_reg(fwd));
            transfer->data     = Buffer.subspan(0, 1).data();
            transfer->dataSize = 1;
            break;
        case kLPI2C_SlaveCompletionEvent:
            /// forward only intact writes: a transaction that ended with a
            /// bus error would hand truncated bytes to the real device (the
            /// MCTP callback guards completionStatus the same way)
            if (!context->transmit && transfer->transferredCount > 0
                && transfer->completionStatus == kStatus_Success) {
                (void)forward::on_target_write(fwd,
                                               context->forward_dst,
                                               context->target_addr,
                                               Buffer.subspan(2, transfer->transferredCount));
            }
            context->rx_seen = false;
            break;
        default: break;
    }
}

/// Dispatch a physical LPI2C target transaction to the VrI2cTask associated with this port.
/// Read sizing and register semantics are owned by the selected emulated device.
void Driver::target_callback_emulation(LPI2C_Type*             base,
                                       lpi2c_slave_transfer_t* transfer,
                                       void*                   user_data)
{
    auto* context = static_cast<TargetContex*>(user_data);
    NV_ASSERT(context->emul_task != nullptr);
    auto&                    emul_task = *static_cast<nv::i2c::VrI2cTask*>(context->emul_task);
    const std::span<uint8_t> buffer    = context->buffer;

    switch (transfer->event) {
        case kLPI2C_SlaveAddressMatchEvent: {
            // Nothing is staged until the transmit event, so discard any stale TX byte.
            base->SCR          |= LPI2C_SCR_RTF_MASK;
            transfer->data      = nullptr;
            transfer->dataSize  = 0U;
            context->transmit   = false;

            if ((transfer->receivedAddress & 0x01U) != 0U) {
                // The SDK reports a repeated start as another address match. Complete the
                // receive phase before asking the emulator for the first read byte.
                if (context->rx_seen && transfer->transferredCount > 0U) {
                    emul_task.i2c_write(context->target_addr,
                                        buffer.subspan(2, transfer->transferredCount));
                    context->rx_seen = false;
                }
            }
            else {
                context->rx_seen = false;
            }
            context->new_transaction = true;
            break;
        }
        case kLPI2C_SlaveTransmitEvent: {
            const bool NewTransaction = context->new_transaction;
            context->transmit         = true;
            context->new_transaction  = false;
            transfer->data            = context->buffer.data();
            transfer->dataSize        = emul_task.i2c_read(
                context->target_addr, buffer.first<I2cSlaveBufferSize>(), NewTransaction);
            break;
        }
        case kLPI2C_SlaveReceiveEvent:
            context->transmit  = false;
            context->rx_seen   = true;
            transfer->data     = context->buffer.data() + 2;
            transfer->dataSize = I2cSlaveBufferSize;
            break;
        case kLPI2C_SlaveCompletionEvent:
            if (!context->transmit && context->rx_seen
                && transfer->completionStatus == kStatus_Success
                && transfer->transferredCount > 0U) {
                emul_task.i2c_write(context->target_addr,
                                    buffer.subspan(2, transfer->transferredCount));
            }
            context->rx_seen = false;
            break;
        default: break;
    }
}

}  // namespace sys::i2c

LPI2C_Type* Driver::get_base(nv::i2c::Port port)
{
    constexpr uint8_t Size = nv::common::to_underlying(nv::i2c::Port::End);
    // NOLINTNEXTLINE: SDK definition
    static const std::array<LPI2C_Type*, Size> bases LPI2C_BASE_PTRS;
    return bases.at(nv::common::to_underlying(port));
}

IRQn_Type Driver::get_irq(nv::i2c::Port port)
{
    constexpr uint8_t Size = nv::common::to_underlying(nv::i2c::Port::End);
    // NOLINTNEXTLINE: SDK definition
    std::array<IRQn_Type, Size> irqs LPI2C_IRQS;
    return irqs.at(nv::common::to_underlying(port));
}

void Driver::irq_handler(uint32_t instance, void* handle)
{
    NV_ASSERT(instance < nv::common::to_underlying(nv::i2c::Port::End));
    NV_ASSERT(handle != nullptr);
    auto base         = get_base(static_cast<nv::i2c::Port>(instance));
    auto lpi2c_handle = static_cast<Lpi2cHandle*>(handle);

    if constexpr (has_emulation_slave_function()) {
        // Re-ordering to and SSR/MSR gate required to stabilize concurrent master/target access
        // on dual-I2C FLEXCOMM (RTC as slave, LSTP as master)
        if ((base->SCR & LPI2C_SCR_SEN_MASK) != 0U && (base->SSR & base->SIER) != 0U) {
            LPI2C_SlaveTransferHandleIRQ(instance, &lpi2c_handle->target);
        }
        if ((base->MCR & LPI2C_MCR_MEN_MASK) != 0U && (base->MSR & base->MIER) != 0U) {
            LPI2C_MasterTransferHandleIRQ(instance, &lpi2c_handle->controller);
        }
    }
    else {
        // For projects that don't use dual-I2C FLEXCOMM, fall back to old logic to reduce
        // regression risk
        if ((base->MCR & LPI2C_MCR_MEN_MASK) != 0U) {
            LPI2C_MasterTransferHandleIRQ(instance, &lpi2c_handle->controller);
        }
        if ((base->SCR & LPI2C_SCR_SEN_MASK) != 0U) {
            LPI2C_SlaveTransferHandleIRQ(instance, &lpi2c_handle->target);
        }
    }
}

void Driver::controller_callback([[maybe_unused]] LPI2C_Type*            base,
                                 [[maybe_unused]] lpi2c_master_handle_t* handle,
                                 status_t                                completion_status,
                                 void*                                   user_data)
{
    using namespace nv::i2c;
    auto context = static_cast<ControllerContex*>(user_data);
    auto task    = static_cast<Task*>(context->task);
    switch (completion_status) {
        case kStatus_Success              : task->set_event(Task::Event::CtrlDone); break;
        case kStatus_LPI2C_Nak            : task->set_event(Task::Event::CtrlNak); break;
        case kStatus_LPI2C_ArbitrationLost: task->set_event(Task::Event::CtrlArbLost); break;
        case kStatus_LPI2C_FifoError      : task->set_event(Task::Event::CtrlFifoError); break;
        case kStatus_LPI2C_BitError       : task->set_event(Task::Event::CtrlBitError); break;
        case kStatus_LPI2C_PinLowTimeout:
            task->set_event(Task::Event::CtrlPinLowTimeout);
            break;
        case kStatus_LPI2C_DmaRequestFail: task->set_event(Task::Event::CtrlDmaError); break;
        case kStatus_LPI2C_Timeout       : task->set_event(Task::Event::CtrlTimeout); break;
        case kStatus_LPI2C_NoTransferInProgress:
            task->set_event(Task::Event::CtrlNoTransfer);
            break;
        default:
            /// TODO should we enable pin low detect?
            task->set_event(Task::Event::CtrlError);
            break;
    }
}

void Driver::update_target_state_time_stamp(lpi2c_slave_transfer_event_t event)
{
    switch (event) {
        case kLPI2C_SlaveAddressMatchEvent:
        case kLPI2C_SlaveTransmitEvent:
        case kLPI2C_SlaveReceiveEvent:
            _target_state_time_stamp.store(nv::ctimer::Driver::read_ticks(),
                                           std::memory_order_relaxed);
            break;
        case kLPI2C_SlaveCompletionEvent:
            _target_state_time_stamp.store(0U, std::memory_order_relaxed);
            break;
        default: break;
    }
}

void Driver::check_target_timeout([[maybe_unused]] bool enable_target)
{
    if constexpr (nv::i2c::EnableI2cPeripheralRecovery) {
        if constexpr (nv::i2c::I2CTargetTimeoutCheckIntervalMs > 0) {
            const auto ts = _target_state_time_stamp.load(std::memory_order_relaxed);
            if (ts == 0U) {
                return;
            }
            const auto now        = nv::ctimer::Driver::read_ticks();
            const auto elapsed_us = sys::ctimer::Driver::get_counter_difference(ts, now);
            if (elapsed_us > nv::i2c::I2CTargetTimeoutUs) {
                peripheral_recovery(enable_target);
                _target_state_time_stamp.store(0U, std::memory_order_relaxed);
            }
        }
    }
}

void Driver::target_callback(LPI2C_Type*             base,
                             lpi2c_slave_transfer_t* transfer,
                             void*                   user_data)
{
    using namespace nv::i2c;
    constexpr uint8_t DefaultData   = 0xFF;
    constexpr uint8_t MinPacketSize = 4;
    auto              context       = static_cast<TargetContex*>(user_data);
    auto              task          = static_cast<Task*>(context->task);
    auto*             drv           = context->owner;
    NV_ASSERT(drv != nullptr);
    drv->update_target_state_time_stamp(transfer->event);
    const std::span<uint8_t> Buffer = context->buffer;
    switch (transfer->event) {
        case kLPI2C_SlaveAddressMatchEvent:
            transfer->data     = nullptr;
            transfer->dataSize = 0;
            break;
        case kLPI2C_SlaveTransmitEvent: {
            context->transmit  = true;
            transfer->data     = context->buffer.data();
            transfer->dataSize = context->buffer.size();

            auto status = false;
            if constexpr (nv::ipc::EnableSmbDirect) {
                status = nv::i2c::on_smbus_direct(context->buffer[2], context->buffer);
            }
            else {
                status = nv::i2c::vr_on_smbus_direct(context->buffer[2],
                                                     context->buffer,
                                                     nv::smb_telemetry::SmbDirect::get_cache());
            }

            if (!status) {
                const auto CmdCode = context->buffer[2];
                if (CmdCode == std::to_underlying(I2cCustomizeCommand::DumpLog)) {
                    const uint8_t RequestSession = context->buffer[3];
                    context->buffer.fill(0x0);
                    download_log_in_isr(RequestSession, context->buffer);
                }
                else {
                    context->buffer[2] = 0x0;
                    context->buffer[0] = DefaultData;
                }
            }
        } break;

        case kLPI2C_SlaveReceiveEvent:
            context->transmit  = false;
            transfer->data     = Buffer.subspan(2).data();
            transfer->dataSize = Buffer.subspan(2).size();
            break;
        case kLPI2C_SlaveCompletionEvent:
            if (transfer->completionStatus != kStatus_Success) {
                task->rx_error();
                break;
            }
            // do not process small transfers and SMBus reads
            if (!context->transmit && transfer->data != nullptr
                && transfer->transferredCount >= MinPacketSize) {
                // [0] = payload_size, [1] = 8 bit target address, [2:] payload
                context->buffer[0] = transfer->transferredCount + 1;
                context->buffer[1] = static_cast<uint8_t>(base->SAMR) | 0U;
                task->rx(context->buffer);
            }
            break;
        case kLPI2C_SlaveRepeatedStartEvent:
            // TODO the driver call AddressMatchEvent instead of RepeatedStartEvent. Is it a
            // BUG?
            break;
        default: nv::error("unknown event %d\n", transfer->event); break;
    }
}

void Driver::bind(nv::i2c::Port port, void* task, void* emul_task)
{
    NV_ASSERT(task != nullptr);
    auto i2c_task = static_cast<nv::i2c::Task*>(task);
    nv::info("task %s binds to I2C port %d\n", i2c_task->name().data(), port);
    logger::info(logger::Event::I2CBind,
                 {static_cast<uint8_t>(i2c_task->id()), static_cast<uint8_t>(port)});
    _port                     = port;
    _base                     = get_base(port);
    _controller_context.task  = task;
    _target_context.task      = task;
    _target_context.emul_task = emul_task;
    _target_context.owner     = this;

    _peripheral_clk_hz = CLOCK_GetLPFlexCommClkFreq(LPI2C_GetInstance(_base));
    if (_peripheral_clk_hz == 0) {
        nv::error("fail to get clock frequency\n");
        _peripheral_clk_hz = 25000000UL;
    }
}

void Driver::init()
{
    LPI2C_MasterTransferCreateHandle(
        _base, &_lpi2c_handle.controller, controller_callback, &_controller_context);
    if (nv::ipc::SlaveFunctionTableSize > 0) {
        LPI2C_SlaveTransferCreateHandle(
            _base, &_lpi2c_handle.target, target_callback_lookup, &_target_context);
    }
    else {
        LPI2C_SlaveTransferCreateHandle(
            _base, &_lpi2c_handle.target, target_callback, &_target_context);
    }

    LP_FLEXCOMM_SetIRQHandler(
        LPI2C_GetInstance(_base), irq_handler, &_lpi2c_handle, LP_FLEXCOMM_PERIPH_LPI2C);
}

void Driver::start(bool enable_target)
{
    LPI2C_MasterEnable(_base, true);
    if (enable_target) {
        LPI2C_SlaveEnable(_base, true);
        constexpr uint32_t Mask   = kLPI2C_SlaveAddressMatchEvent | kLPI2C_SlaveCompletionEvent;
        const status_t     Status = LPI2C_SlaveTransferNonBlocking(
            _base, &_lpi2c_handle.target, Mask);
        if (Status != kStatus_Success) {
            nv::error("fail to start target\n");
        }
    }
}

void Driver::peripheral_recovery(bool enable_target)
{
    // This routine only recovers the I2C slave/target. Controller-only ports
    // have no slave, so do not touch any slave registers/APIs, take the port
    // mutex, or emit recovery logs.
    if (!enable_target) {
        return;
    }
    auto& mutex = nv::ipc::Mutex::make(nv::i2c::port_to_mutex_id(_port));
    if (mutex.lock() != nv::ipc::Mutex::Status::Ok) {
        return;
    }

    constexpr uint8_t I2CRecoveryLogReport = 0;
    constexpr uint8_t I2CRecoveryLogResult = 1;
    nv::logger::info(
        nv::logger::Event::I2CSlaveRecovery,
        {I2CRecoveryLogReport,
         static_cast<uint8_t>(nv::common::to_underlying(_port) & 0xFFU),
         static_cast<uint8_t>(_lpi2c_handle.target.isBusy ? 1 : 0),
         static_cast<uint8_t>(_lpi2c_handle.target.transferredCount & 0xFFU),
         static_cast<uint8_t>(_lpi2c_handle.target.transfer.event & 0xFFU),
         static_cast<uint8_t>(_lpi2c_handle.target.transfer.receivedAddress & 0xFFU),
         static_cast<uint8_t>(_lpi2c_handle.target.transfer.completionStatus & 0xFFU)});

    // Abort current transfer and disable slave
    LPI2C_SlaveTransferAbort(_base, &_lpi2c_handle.target);
    LPI2C_SlaveEnable(_base, false);

    // Retrieve slave configuration from registers, reset, and re-init slave
    lpi2c_slave_config_t slave_config{};
    slave_config_fill_from_registers(_base, _peripheral_clk_hz, slave_config);
    LPI2C_SlaveInit(_base, &slave_config, _peripheral_clk_hz);

    // Enable slave and interrupts and arm for next transfer
    LPI2C_SlaveEnable(_base, true);
    constexpr uint32_t Mask = kLPI2C_SlaveAddressMatchEvent | kLPI2C_SlaveCompletionEvent;
    const status_t Status = LPI2C_SlaveTransferNonBlocking(_base, &_lpi2c_handle.target, Mask);
    nv::logger::info(
        nv::logger::Event::I2CSlaveRecovery,
        {I2CRecoveryLogResult,
         static_cast<uint8_t>(nv::common::to_underlying(_port) & 0xFFU),
         static_cast<uint8_t>(_lpi2c_handle.target.isBusy ? 1 : 0),
         static_cast<uint8_t>(_lpi2c_handle.target.transferredCount & 0xFFU),
         static_cast<uint8_t>(_lpi2c_handle.target.transfer.event & 0xFFU),
         static_cast<uint8_t>(_lpi2c_handle.target.transfer.receivedAddress & 0xFFU),
         static_cast<uint8_t>(static_cast<uint32_t>(Status) & 0xFFU),
         static_cast<uint8_t>(static_cast<uint32_t>(Status) >> 8U & 0xFFU)});

    mutex.unlock();
}

bool Driver::write(std::span<uint8_t> data)
{
    std::copy(data.begin(), data.end(), _controller_context.buffer.begin());
    lpi2c_master_transfer_t transfer = {
        .flags          = kLPI2C_TransferDefaultFlag,
        .slaveAddress   = static_cast<uint8_t>(_controller_context.buffer[0] >> 1U),
        .direction      = kLPI2C_Write,
        .subaddress     = 0,
        .subaddressSize = 0,
        .data           = &_controller_context.buffer.data()[1],
        .dataSize       = data.size() - 1,
    };

    auto& mutex = nv::ipc::Mutex::make(nv::i2c::port_to_mutex_id(_port));
    // Acquire mutex
    auto mutex_status = mutex.lock();
    if (mutex_status != nv::ipc::Mutex::Status::Ok) {
        return false;
    }
    const status_t Status = LPI2C_MasterTransferNonBlocking(
        _base, &_lpi2c_handle.controller, &transfer);
    mutex.unlock();
    return Status == kStatus_Success;
}

bool Driver::get_status(uint8_t address)
{
    constexpr size_t  PacketSize     = 11;
    constexpr uint8_t SmbusMctpCmd   = 0x0F;
    constexpr uint8_t MctpByteCount  = 0x08;
    constexpr uint8_t MctpHdrVersion = 0x01;
    constexpr uint8_t MctpNullEid    = 0x00;
    // SOM=1, EOM=1, pkt_seq=0, TO=1, msg_tag=0
    constexpr uint8_t MctpPktFlags    = 0xC8;
    constexpr uint8_t MctpMsgTypeCtrl = 0x00;
    // rq=1, d=0, instance_id=0
    constexpr uint8_t MctpCtrlRqByte = 0x80;
    constexpr uint8_t MctpCmdGetEpId = 0x02;
    constexpr uint8_t PecPlaceholder = 0x00;
    using namespace std::chrono;
    constexpr uint8_t Retry    = 5;
    auto              i2c_task = static_cast<nv::i2c::Task*>(_controller_context.task);

    std::array<uint8_t, PacketSize> packet{SmbusMctpCmd,
                                           MctpByteCount,
                                           static_cast<uint8_t>(this->address() << 1U),
                                           MctpHdrVersion,
                                           MctpNullEid,
                                           MctpNullEid,
                                           MctpPktFlags,
                                           MctpMsgTypeCtrl,
                                           MctpCtrlRqByte,
                                           MctpCmdGetEpId,
                                           PecPlaceholder};

    std::array<uint8_t, PacketSize> pec_input{};
    pec_input[0] = static_cast<uint8_t>(address << 1U);
    std::copy(packet.begin(), packet.end() - 1, pec_input.begin() + 1);
    packet.back() = nv::i2c::crc8(pec_input);

    lpi2c_master_transfer_t transfer = {
        .flags          = kLPI2C_TransferDefaultFlag,
        .slaveAddress   = address,
        .direction      = kLPI2C_Write,
        .subaddress     = 0,
        .subaddressSize = 0,
        .data           = packet.data(),
        .dataSize       = packet.size(),
    };

    auto& mutex = nv::ipc::Mutex::make(nv::i2c::port_to_mutex_id(_port));
    if (mutex.lock() != nv::ipc::Mutex::Status::Ok) {
        return false;
    }
    for (uint8_t i = 0; i < Retry; i++) {
        const status_t Status = LPI2C_MasterTransferBlocking(_base, &transfer);
        if (Status == kStatus_Success) {
            mutex.unlock();
            return true;
        }
        if (i + 1U < Retry) {
            mutex.unlock();
            i2c_task->delay(10ms);
            if (mutex.lock() != nv::ipc::Mutex::Status::Ok) {
                return false;
            }
        }
    }
    mutex.unlock();
    return false;
}

bool Driver::ping(uint8_t address)
{
    // Zero-length write: send START + address + STOP and report the address-phase ACK/NAK.
    // No payload is transmitted, so this never feeds MCTP (or any) data to the target.
    lpi2c_master_transfer_t transfer = {
        .flags          = kLPI2C_TransferDefaultFlag,
        .slaveAddress   = address,
        .direction      = kLPI2C_Write,
        .subaddress     = 0,
        .subaddressSize = 0,
        .data           = nullptr,
        .dataSize       = 0,
    };

    auto& mutex = nv::ipc::Mutex::make(nv::i2c::port_to_mutex_id(_port));
    if (mutex.lock() != nv::ipc::Mutex::Status::Ok) {
        return false;
    }
    const status_t Status = LPI2C_MasterTransferBlocking(_base, &transfer);
    mutex.unlock();
    return Status == kStatus_Success;
}

uint8_t Driver::address()
{
    return static_cast<uint8_t>(_base->SAMR) >> 1U;
}

void Driver::set_address(nv::i2c::Port port, uint8_t address, uint8_t sec_addr)
{
    auto base  = get_base(port);
    base->SAMR = LPI2C_SAMR_ADDR0(static_cast<uint32_t>(address))
               | LPI2C_SAMR_ADDR1(static_cast<uint32_t>(sec_addr));
    base->SCFGR1 &= ~LPI2C_SCFGR1_ADDRCFG_MASK;
    if (sec_addr != 0) {
        base->SCFGR1 |= LPI2C_SCFGR1_ADDRCFG(kLPI2C_MatchAddress0OrAddress1);
    }
    logger::info(logger::Event::I2CAddressUpdate,
                 {static_cast<uint8_t>(port), static_cast<uint8_t>(address)});
}

void Driver::set_master_duty_cycle(nv::i2c::Port port, uint8_t duty_cycle)
{
    constexpr uint32_t ClkLowShift  = 0;
    constexpr uint32_t ClkHighShift = 8;
    constexpr uint32_t ClkMask      = 0x3FU;

    auto enabled = is_master_enabled(port);
    auto base    = get_base(port);
    auto mccr0   = base->MCCR0;

    uint32_t clkHi  = (mccr0 >> ClkHighShift) & ClkMask;
    uint32_t clkLo  = (mccr0 >> ClkLowShift) & ClkMask;
    uint32_t period = clkHi + clkLo;
    if (duty_cycle > 100 || period == 0) {
        return;
    }
    clkHi = static_cast<uint32_t>((static_cast<float>(period) * static_cast<float>(duty_cycle))
                                  / 100.0);
    clkLo = period - clkHi;

    logger::info(logger::Event::I2cClockPeriods,
                 {static_cast<uint8_t>(clkHi), static_cast<uint8_t>(clkLo)});
    mccr0 &= ~((ClkMask << ClkHighShift) | (ClkMask << ClkLowShift));
    mccr0 |= (clkHi << ClkHighShift) | (clkLo << ClkLowShift);

    if (enabled) {
        LPI2C_MasterEnable(base, false);
    }

    base->MCCR0 = mccr0;

    if (enabled) {
        LPI2C_MasterEnable(base, true);
    }
}

i2c::I2cStatus
Driver::i2c_read(uint8_t address, std::span<uint8_t> buffer, nv::i2c::I2cFlags flags)
{
    uint32_t lpi2c_flags = kLPI2C_TransferDefaultFlag;

    if (flags & nv::i2c::I2cFlags::RecvLen) {
        lpi2c_flags |= kLPI2C_TransferNoStartFlag;
    }

    if (flags & nv::i2c::I2cFlags::NoStop) {
        lpi2c_flags |= kLPI2C_TransferNoStopFlag;
    }

    lpi2c_master_transfer_t xfer{
        .flags        = lpi2c_flags,
        .slaveAddress = address,
        .direction    = kLPI2C_Read,
        .data         = buffer.data(),
        .dataSize     = (flags & nv::i2c::I2cFlags::QuickRead) ? 0 : buffer.size(),
    };

    const status_t status = LPI2C_MasterTransferNonBlocking(
        _base, &_lpi2c_handle.controller, &xfer);
    return sys::i2c::get_status(status);
}

i2c::I2cStatus
Driver::i2c_write(uint8_t address, std::span<uint8_t> buffer, nv::i2c::I2cFlags flags)
{
    lpi2c_master_transfer_t xfer{
        .flags        = (flags & nv::i2c::I2cFlags::NoStop) ? kLPI2C_TransferNoStopFlag
                                                            : kLPI2C_TransferDefaultFlag,
        .slaveAddress = address,
        .direction    = kLPI2C_Write,
        .data         = buffer.data(),
        .dataSize     = (flags & nv::i2c::I2cFlags::QuickWrite) ? 0 : buffer.size(),
    };

    const status_t status = LPI2C_MasterTransferNonBlocking(
        _base, &_lpi2c_handle.controller, &xfer);
    return sys::i2c::get_status(status);
}

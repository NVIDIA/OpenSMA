#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstring>

#include "bridge.h"

#include "fsl_clock.h"
#include "fsl_port.h"
#include "fsl_str.h"

#include "nv/nv.h"
#include "nv/common/debug.h"
#include "nv/common/utils.h"
#include "nv/ctimer/ctimer.h"
#include "nv/common/preproc.h"

#include "nv/vruart/bridge.h"

/** @todo: for convenience, to-be-moved to gpio driver */
extern "C" clock_ip_name_t get_port_clock(uint8_t port)
{
    switch (port) {
        case 0 : return kCLOCK_Port0;
        case 1 : return kCLOCK_Port1;
        case 2 : return kCLOCK_Port2;
        case 3 : return kCLOCK_Port3;
        case 4 : return kCLOCK_Port4;
        default: return kCLOCK_IpInvalid;
    }
}

/** @todo: for convenience, to-be-moved to gpio driver */
extern "C" PORT_Type* get_port_base(uint8_t port)
{
    switch (port) {
        case 0 : return PORT0;
        case 1 : return PORT1;
        case 2 : return PORT2;
        case 3 : return PORT3;
        case 4 : return PORT4;
        case 5 : return PORT5;
        default: break;
    }
    return nullptr;
}

namespace sys::uart {

sys::uart::Type* Bridge::get_reg_base(nv::vruart::Instance instance)
{
    constexpr uint8_t Size = nv::common::to_underlying(sys::uart::Instance::End);
    std::array<sys::uart::Type*, Size> bases LPUART_BASE_PTRS;
    return bases.at(nv::common::to_underlying(instance));
}

Status Bridge::init_signal(const nv::vruart::Signal& tx, const nv::vruart::Signal& rx)
{
    const port_pin_config_t config = {/* Internal pull-up/down resistor is disabled */
                                      kPORT_PullDisable,
                                      /* Low internal pull resistor value is selected. */
                                      kPORT_LowPullResistor,
                                      /* Fast slew rate is configured */
                                      kPORT_FastSlewRate,
                                      /* Passive input filter is disabled */
                                      kPORT_PassiveFilterDisable,
                                      /* Open drain output is disabled */
                                      kPORT_OpenDrainDisable,
                                      /* Low drive strength is configured */
                                      kPORT_LowDriveStrength,
                                      /* Pin is configured as FC4_P1 */
                                      kPORT_MuxAlt2,
                                      /* Digital input enabled */
                                      kPORT_InputBufferEnable,
                                      /* Digital input is not inverted */
                                      kPORT_InputNormal,
                                      /* Pin Control Register fields [15:0] are not locked */
                                      kPORT_UnlockRegister};

    CLOCK_EnableClock(get_port_clock(tx.port));
    CLOCK_EnableClock(get_port_clock(rx.port));

    PORT_SetPinConfig(get_port_base(tx.port), tx.pin, &config);
    PORT_SetPinConfig(get_port_base(rx.port), rx.pin, &config);

    this->uartTx = tx;
    this->uartRx = rx;
    return Status::Ok;
}

Status Bridge::init_edma(nv::vruart::EdmaInst edmaInstance,
                         nv::vruart::EdmaChn  edmaTxChn,
                         nv::vruart::EdmaChn  edmaRxChn)
{
    // Note: eDMA peripheral initialization is done in peripheral.cpp

    this->edmaInstance = edmaInstance;
    this->edmaTxChn    = edmaTxChn;
    this->edmaRxChn    = edmaRxChn;
    this->edmaRegbase  = (edmaInstance == nv::vruart::EdmaInst::_0) ? DMA0 : DMA1;

    auto txChn = static_cast<uint8_t>(nv::common::to_underlying(edmaTxChn));
    auto rxChn = static_cast<uint8_t>(nv::common::to_underlying(edmaRxChn));

    EDMA_CreateHandle(&this->edmaTxHandle, this->edmaRegbase, txChn);
    EDMA_CreateHandle(&this->edmaRxHandle, this->edmaRegbase, rxChn);

    auto base = static_cast<uint8_t>(kDma0RequestMuxLpFlexcomm0Rx)
              + nv::common::to_underlying(this->uartInstance) * 2;
    this->edmaRxChnReqSrc = static_cast<dma_request_source_t>(base + 0);
    this->edmaTxChnReqSrc = static_cast<dma_request_source_t>(base + 1);

    EDMA_SetChannelMux(this->edmaRegbase, txChn, this->edmaTxChnReqSrc);
    EDMA_SetChannelMux(this->edmaRegbase, rxChn, this->edmaRxChnReqSrc);

    return Status::Ok;
}

Status Bridge::init_lpuart(nv::vruart::Baudrate baudrate)
{
    /**
     * default config
     *   baudRate_Bps = 115200U;
     *   parityMode = kLPUART_ParityDisabled;
     *   dataBitsCount = kLPUART_EightDataBits;
     *   isMsb = false;
     *   stopBitCount = kLPUART_OneStopBit;
     *   txFifoWatermark = 0;
     *   rxFifoWatermark = 0;
     *   enableRxRTS = false;
     *   enableTxCTS = false;
     *   txCtsConfig = kLPUART_CtsSampleAtStart;
     *   txCtsSource = kLPUART_CtsSourcePin;
     *   rxIdleType = kLPUART_IdleTypeStartBit;
     *   rxIdleConfig = kLPUART_IdleCharacter1;
     *   enableTx = false;
     *   enableRx = false;
     */
    lpuart_config_t config;
    LPUART_GetDefaultConfig(&config);
    config.baudRate_Bps = baudrate;
    config.enableTx     = true;
    config.enableRx     = true;
    LPUART_Init(uartRegbase, &config, 12000000U);
    return Status::Ok;
}

Status Bridge::init_clock(nv::vruart::Instance instance)
{
    switch (instance) {
        case nv::vruart::Instance::_0:
            CLOCK_SetClkDiv(kCLOCK_DivFlexcom0Clk, 1u);
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM0);
            break;
        case nv::vruart::Instance::_1:
            CLOCK_SetClkDiv(kCLOCK_DivFlexcom1Clk, 1u);
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM1);
            break;
        case nv::vruart::Instance::_2:
            CLOCK_SetClkDiv(kCLOCK_DivFlexcom2Clk, 1u);
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM2);
            break;
        case nv::vruart::Instance::_3:
            CLOCK_SetClkDiv(kCLOCK_DivFlexcom3Clk, 1u);
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM3);
            break;
        case nv::vruart::Instance::_4:
            CLOCK_SetClkDiv(kCLOCK_DivFlexcom4Clk, 1u);
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM4);
            break;
        case nv::vruart::Instance::_5:
            CLOCK_SetClkDiv(kCLOCK_DivFlexcom5Clk, 1u);
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM5);
            break;
        case nv::vruart::Instance::_6:
            CLOCK_SetClkDiv(kCLOCK_DivFlexcom6Clk, 1u);
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM6);
            break;
        case nv::vruart::Instance::_7:
            CLOCK_SetClkDiv(kCLOCK_DivFlexcom7Clk, 1u);
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM7);
            break;
        case nv::vruart::Instance::_8:
            CLOCK_SetClkDiv(kCLOCK_DivFlexcom8Clk, 1u);
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM8);
            break;
        case nv::vruart::Instance::_9:
            CLOCK_SetClkDiv(kCLOCK_DivFlexcom9Clk, 1u);
            CLOCK_AttachClk(kFRO12M_to_FLEXCOMM9);
            break;
        default:
            nv::info("Bridge: unknown nv::vruart::Instance=%d\n",
                     nv::common::to_underlying(instance));
            return Status::InvalidInstance;
    }
    return Status::Ok;
}

extern "C" void LPUART_UserCallback(LPUART_Type*          base,
                                    lpuart_edma_handle_t* handle,
                                    status_t              status,
                                    void*                 userData)
{
    auto uart = static_cast<sys::uart::Bridge*>(userData);
    if (kStatus_LPUART_TxIdle == status) {
        uart->set_txongoing(false);
        // Notify task that UART TX is complete
        nv::vruart::Bridge::set_uart_tx_done_event();
    }

    // handle RX buffer full (EDMA transfer complete)
    if (kStatus_LPUART_RxIdle == status) {
        uart->set_rxongoing(false);

        // Order matters: enqueue first (copy data), then restart eDMA (may overwrite buffer)
        (void)nv::vruart::Bridge::enqueue(uart->get_rx_buffer().data(),
                                          sys::uart::edmaXferBufSize);
        (void)uart->start_edma_rx(sys::uart::edmaXferBufSize);
    }

    // check if idle line interrupt is triggered (partial data received)
    if (kStatus_LPUART_IdleLineDetected == status) {
        // get received bytes count
        uint32_t receivedCount;
        LPUART_TransferGetReceiveCountEDMA(base, handle, &receivedCount);

        if (receivedCount > 0) {
            // stop current edma transfer first
            LPUART_TransferAbortReceiveEDMA(base, handle);

            // Order matters: enqueue first (copy data), then restart eDMA (may overwrite
            // buffer)
            (void)nv::vruart::Bridge::enqueue(uart->get_rx_buffer().data(), receivedCount);
            (void)uart->start_edma_rx(sys::uart::edmaXferBufSize);
        }
    }
}

extern "C" void Custom_LPUART_IRQHandler(uint32_t instance, void* lpuartEdmaHandle)
{
    sys::uart::Type* base = sys::uart::Bridge::get_reg_base(
        static_cast<nv::vruart::Instance>(instance));
    lpuart_edma_handle_t* handle = static_cast<lpuart_edma_handle_t*>(lpuartEdmaHandle);

    // check if idle line interrupt is triggered
    if (LPUART_GetStatusFlags(base) & kLPUART_IdleLineFlag) {
        // clear idle line interrupt flag
        LPUART_ClearStatusFlags(base, kLPUART_IdleLineFlag);

        // call user callback, pass idle line status
        if (handle->callback != NULL) {
            handle->callback(base, handle, kStatus_LPUART_IdleLineDetected, handle->userData);
        }
    }

    // call SDK original handler (handle TX Complete)
    LPUART_TransferEdmaHandleIRQ(instance, lpuartEdmaHandle);
}

Status Bridge::init(nv::vruart::Instance      instance,
                    const nv::vruart::Signal& tx,
                    const nv::vruart::Signal& rx,
                    nv::vruart::Baudrate      baudrate,
                    nv::vruart::EdmaInst      edmaInstance,
                    nv::vruart::EdmaChn       edmaTxChn,
                    nv::vruart::EdmaChn       edmaRxChn)
{
    if (uartState == State::Running) {
        return Status::Ok;
    }

    // Set UART instance and register base
    uartInstance = instance;
    uartRegbase  = get_reg_base(instance);

    /** init lpuart clock */
    init_clock(uartInstance);

    /** init lpuart signal
     *
     * Projects that have already routed the LPUART pins via BOARD_InitPins
     * (MCUXpresso tool-generated pin_mux.c) can define
     * NV_UART_BRIDGE_BYPASS_PIN_INIT in their config.h to skip this step.
     *
     * init_signal hardcodes kPORT_MuxAlt2, which is correct for the
     * FRDM-MCXN947 FC4 pins on PIO1_8/PIO1_9 but wrong for boards using
     * a different alt assignment (e.g. vel_mer_sw uses PIO1_20/PIO1_21
     * where FC4 lives on ALT3).
     */
#if defined(NV_UART_BRIDGE_BYPASS_PIN_INIT) && (NV_UART_BRIDGE_BYPASS_PIN_INIT > 0)
    (void)tx;
    (void)rx;
#else
    init_signal(tx, rx);
#endif

    /** init lpuart */
    init_lpuart(baudrate);

    /** init lpuart edma */
    init_edma(edmaInstance, edmaTxChn, edmaRxChn);

    /** init lpuart edma handle */
    LPUART_TransferCreateHandleEDMA(uartRegbase,
                                    &edmaLpuartHandle,
                                    LPUART_UserCallback,
                                    static_cast<void*>(this),
                                    &edmaTxHandle,
                                    &edmaRxHandle);

    /** register new lpuart edma handle irq handler */
    LP_FLEXCOMM_SetIRQHandler(nv::common::to_underlying(uartInstance),
                              Custom_LPUART_IRQHandler,
                              &edmaLpuartHandle,
                              LP_FLEXCOMM_PERIPH_LPUART);

    /** enable lpuart idle line interrupt */
    LPUART_EnableInterrupts(uartRegbase, kLPUART_IdleLineInterruptEnable);

    /** start receive */
    LPUART_ReceiveEDMA(uartRegbase, &edmaLpuartHandle, &edmaLpuartXferRx);

    uartState = State::Running;
    return Status::Ok;
}

Status Bridge::tx(std::span<uint8_t> data)
{
    if (!ready()) {
        return Status::NotInited;
    }
    if (txongoing()) {
        return Status::LpuartEdmaTxBusy;
    }

    // Non-blocking: start eDMA TX, will get callback when complete
    set_txongoing(true);

    // Copy data to tx buffer
    std::copy(data.begin(), data.end(), edmaXferTxBuffer.data());

    // Start eDMA transfer (non-blocking)
    start_edma_tx(data.size());

    return Status::Ok;
}

}  // namespace sys::uart
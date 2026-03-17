#pragma once

#include <span>
#include <array>
#include <cstdint>

#include "nv/gpio/common.h"
#include "fsl_lpuart_edma.h"
#include "fsl_lpuart.h"
#include "fsl_edma.h"

#include "sys/uart/common.h"
#include "nv/vruart/common.h"

namespace sys::uart {

using Type                              = LPUART_Type;
constexpr static size_t edmaXferBufSize = 512U;

class Bridge
{
private:
    nv::vruart::Signal   uartTx{};
    nv::vruart::Signal   uartRx{};
    nv::vruart::Baudrate uartBaudrate{};
    nv::vruart::Instance uartInstance{};
    sys::uart::Type*     uartRegbase{nullptr};
    sys::uart::State     uartState{};

    nv::vruart::EdmaInst edmaInstance{};
    nv::vruart::EdmaChn  edmaTxChn{};
    nv::vruart::EdmaChn  edmaRxChn{};
    DMA_Type*            edmaRegbase{nullptr};
    edma_handle_t        edmaTxHandle{};
    edma_handle_t        edmaRxHandle{};
    dma_request_source_t edmaTxChnReqSrc{};
    dma_request_source_t edmaRxChnReqSrc{};

    std::array<uint8_t, edmaXferBufSize> edmaXferTxBuffer{};
    std::array<uint8_t, edmaXferBufSize> edmaXferRxBuffer{};

    lpuart_edma_handle_t edmaLpuartHandle{};
    lpuart_transfer_t    edmaLpuartXferTx{.data     = edmaXferTxBuffer.data(),
                                          .dataSize = edmaXferTxBuffer.size()};
    lpuart_transfer_t    edmaLpuartXferRx{.data     = edmaXferRxBuffer.data(),
                                          .dataSize = edmaXferRxBuffer.size()};

    volatile bool edmaTxOngoing{false};
    volatile bool edmaRxOngoing{false};

protected:
    Status init_clock(nv::vruart::Instance instance);
    Status init_lpuart(nv::vruart::Baudrate baudrate);
    Status init_signal(const nv::vruart::Signal& tx, const nv::vruart::Signal& rx);
    Status init_edma(nv::vruart::EdmaInst edmaInstance,
                     nv::vruart::EdmaChn  edmaTxChn,
                     nv::vruart::EdmaChn  edmaRxChn);

public:
    Bridge() = default;

    Status init(nv::vruart::Instance      uartInstance,
                const nv::vruart::Signal& tx,
                const nv::vruart::Signal& rx,
                nv::vruart::Baudrate      baudrate,
                nv::vruart::EdmaInst      edmaInstance,
                nv::vruart::EdmaChn       edmaTxChn,
                nv::vruart::EdmaChn       edmaRxChn);
    Status tx(std::span<uint8_t> data);

    bool ready() const { return uartState == State::Running; }
    bool txongoing() const
    {
        __DMB();
        return edmaTxOngoing;
    }
    bool rxongoing() const
    {
        __DMB();
        return edmaRxOngoing;
    }
    void set_txongoing(bool ongoing)
    {
        edmaTxOngoing = ongoing;
        __DMB();
    }
    void set_rxongoing(bool ongoing)
    {
        edmaRxOngoing = ongoing;
        __DMB();
    }
    status_t start_edma_tx(size_t dataSize)
    {
        edmaLpuartXferTx.dataSize = dataSize;
        return LPUART_SendEDMA(uartRegbase, &edmaLpuartHandle, &edmaLpuartXferTx);
    }
    status_t start_edma_rx(size_t dataSize)
    {
        edmaLpuartXferRx.dataSize = dataSize;
        return LPUART_ReceiveEDMA(uartRegbase, &edmaLpuartHandle, &edmaLpuartXferRx);
    }

    static sys::uart::Type* get_reg_base(nv::vruart::Instance instance);

    const std::array<uint8_t, edmaXferBufSize>& get_tx_buffer() const
    {
        return this->edmaXferTxBuffer;
    }
    const std::array<uint8_t, edmaXferBufSize>& get_rx_buffer() const
    {
        return this->edmaXferRxBuffer;
    }
};

}  // namespace sys::uart

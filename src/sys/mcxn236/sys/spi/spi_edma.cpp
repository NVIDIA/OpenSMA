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
#include <array>
#include <cstring>
#include "nv/nv.h"
#include "fsl_gpio.h"
#include "nv/logger/log.h"
#include "nv/ctimer/ctimer.h"
#include "sys/common/utils.h"
#include "fsl_lpspi_edma.h"
#include "fsl_edma.h"
#include "sys/spi/spi_edma.h"
using namespace nv;
using namespace sys::spi;

namespace {
// Timeout for SPI transfer operations in milliseconds
constexpr uint32_t SPI_TRANSFER_TIMEOUT_MS = 1000;
}  // namespace
void EdmaDriver::get_dma_channels(nv::spi::Flexcomm flexcomm,
                                  uint32_t&         tx_channel,
                                  uint32_t&         rx_channel)
{
    switch (flexcomm) {
        case nv::spi::Flexcomm::Zero:
            tx_channel = kDma0RequestMuxLpFlexcomm0Tx;
            rx_channel = kDma0RequestMuxLpFlexcomm0Rx;
            break;
        case nv::spi::Flexcomm::One:
            tx_channel = kDma0RequestMuxLpFlexcomm1Tx;
            rx_channel = kDma0RequestMuxLpFlexcomm1Rx;
            break;
        case nv::spi::Flexcomm::Two:
            tx_channel = kDma0RequestMuxLpFlexcomm2Tx;
            rx_channel = kDma0RequestMuxLpFlexcomm2Rx;
            break;
        case nv::spi::Flexcomm::Three:
            tx_channel = kDma0RequestMuxLpFlexcomm3Tx;
            rx_channel = kDma0RequestMuxLpFlexcomm3Rx;
            break;
        case nv::spi::Flexcomm::Four:
            tx_channel = kDma0RequestMuxLpFlexcomm4Tx;
            rx_channel = kDma0RequestMuxLpFlexcomm4Rx;
            break;
        case nv::spi::Flexcomm::Five:
            tx_channel = kDma0RequestMuxLpFlexcomm5Tx;
            rx_channel = kDma0RequestMuxLpFlexcomm5Rx;
            break;
        case nv::spi::Flexcomm::Six:
            tx_channel = kDma0RequestMuxLpFlexcomm6Tx;
            rx_channel = kDma0RequestMuxLpFlexcomm6Rx;
            break;
        case nv::spi::Flexcomm::Seven:
            tx_channel = kDma0RequestMuxLpFlexcomm7Tx;
            rx_channel = kDma0RequestMuxLpFlexcomm7Rx;
            break;
        default:
            tx_channel = 0;
            rx_channel = 0;
            break;
    }
}

LPSPI_Type* EdmaDriver::get_base(nv::spi::Flexcomm flexcomm)
{
    constexpr uint8_t Size = nv::common::to_underlying(nv::spi::Flexcomm::End);
    // NOLINTNEXTLINE: SDK definition
    std::array<LPSPI_Type*, Size> bases LPSPI_BASE_PTRS;
    return bases.at(nv::common::to_underlying(flexcomm));
}

void EdmaDriver::bind(nv::spi::Flexcomm flexcomm, nv::ipc::EventId event_id)
{
    _base                     = get_base(flexcomm);
    _flexcomm                 = flexcomm;
    _event                    = &nv::ipc::Event::make(event_id);
    _master_xfer_ctx.driver   = this;
    _master_xfer_ctx.xferDone = false;
}

void EdmaDriver::init()
{
    // Initialize eDMA
    static bool edma_initialized = false;
    if (!edma_initialized) {
        edma_config_t edmaConfig = {false};
        EDMA_GetDefaultConfig(&edmaConfig);
        EDMA_Init(DMA0, &edmaConfig);
        edma_initialized = true;
    }

    uint32_t tx_channel = 0;
    uint32_t rx_channel = 0;
    get_dma_channels(_flexcomm, tx_channel, rx_channel);

    // Create eDMA handles - need appropriate DMA channels
    // For flexcomm, typically use channels 0-15
    const auto dma_rx_channel = static_cast<uint32_t>(_flexcomm);      // Even channel for RX
    const auto dma_tx_channel = static_cast<uint32_t>(_flexcomm) + 1;  // Odd channel for TX

    EDMA_CreateHandle(&_lpspiEdmaMasterRxRegToRxDataHandle, DMA0, dma_rx_channel);
    EDMA_CreateHandle(&_lpspiEdmaMasterTxDataToTxRegHandle, DMA0, dma_tx_channel);

#if defined(FSL_FEATURE_EDMA_HAS_CHANNEL_MUX) && FSL_FEATURE_EDMA_HAS_CHANNEL_MUX
    EDMA_SetChannelMux(DMA0, dma_tx_channel, tx_channel);
    EDMA_SetChannelMux(DMA0, dma_rx_channel, rx_channel);
#endif

    // Create LPSPI eDMA handle
    LPSPI_MasterTransferCreateHandleEDMA(_base,
                                         &_edma_handle,
                                         master_edma_callback,
                                         &_master_xfer_ctx,
                                         &_lpspiEdmaMasterRxRegToRxDataHandle,
                                         &_lpspiEdmaMasterTxDataToTxRegHandle);
}

sys::spi::Status EdmaDriver::sendRecv(uint32_t                 send_len,
                                      const std::span<uint8_t> sbuf,
                                      uint32_t /*recv_len*/,
                                      std::span<uint8_t> rbuf)
{
    lpspi_transfer_t masterXfer;

    _master_xfer_ctx.xferDone = false;

    masterXfer.txData      = sbuf.data();
    masterXfer.rxData      = rbuf.data();
    masterXfer.dataSize    = send_len;
    masterXfer.configFlags = kLPSPI_MasterPcs0 | kLPSPI_MasterByteSwap;

    // Start master transfer using eDMA
    const status_t Status = LPSPI_MasterTransferEDMA(_base, &_edma_handle, &masterXfer);
    if (Status != kStatus_Success) {
        nv::info("sendRecv: LPSPI_MasterTransferEDMA return (%d)\n", Status);
        return sys::spi::Status::EventXferFail;
    }

    // wait complete
    auto wait         = sys::spi::Event::XferDone | sys::spi::Event::XferError;
    auto event_result = _event->wait(
        wait, true, false, std::chrono::milliseconds(SPI_TRANSFER_TIMEOUT_MS));

    if (!event_result) {
        nv::info("sendRecv: wait_event failed with status %d\n",
                 static_cast<int>(event_result.error()));
        return sys::spi::Status::EventTimeout;
    }

    if (!(event_result.value() & sys::spi::Event::XferDone)) {
        nv::info("sendRecv: transfer failed or timeout\n");
        return sys::spi::Status::EventXferFail;
    }

    return sys::spi::Status::Ok;
}

void EdmaDriver::master_edma_callback(LPSPI_Type* /*base*/,
                                      lpspi_master_edma_handle_t* /*handle*/,
                                      status_t status,
                                      void*    userData)
{
    auto ctx    = static_cast<MasterTransferContext*>(userData);
    auto driver = static_cast<EdmaDriver*>(ctx->driver);

    if (status == kStatus_Success) {
        (void)driver->_event->set(sys::spi::Event::XferDone);
    }
    else {
        (void)driver->_event->set(sys::spi::Event::XferError);
    }
    ctx->xferDone = true;
}
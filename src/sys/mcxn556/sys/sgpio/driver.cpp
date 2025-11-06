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
#include "nv/sgpio/driver.h"
#include "nv/common/preproc.h"
#include "nv/logger/log.h"

using namespace sys::sgpio;

namespace {

static void sgpio_slave_callback(FLEXIO_SPI_Type*                base,
                                 flexio_spi_slave_edma_handle_t* handle,
                                 status_t                        status,
                                 void*                           userData)
{
    (void)base;
    (void)handle;
    (void)userData;
    // coverity[cert_int31_c_violation] must be valid
    nv::logger::Logger::add_from_isr(nv::logger::Event::SgpioCallback.unique_id,
                                     nv::logger::Level::Info,
                                     nv::logger::data_from_u32(static_cast<uint32_t>(status)));
}

nv::sgpio::Driver::SgpioStatus to_status(status_t status)
{
    switch (status) {
        case kStatus_Success        : return nv::sgpio::Driver::SgpioStatus::Success;
        case kStatus_FLEXIO_SPI_Busy: return nv::sgpio::Driver::SgpioStatus::Busy;
        default                     : return nv::sgpio::Driver::SgpioStatus::Error;
    }
}

}  // namespace

nv::sgpio::Driver::Driver(SgpioConfig config) : sys::sgpio::Driver(), _config(config) {}

bool nv::sgpio::Driver::init()
{
    dma_request_source_t dma_request_source_tx = kDma0RequestMuxFlexIO0ShiftRegister2Request;
    // channel 0 and 1 used by I3C
    uint32_t dma_tx_channel = 2;
    _spi_dev.flexioBase     = FLEXIO0;
    _spi_dev.SDOPinIndex    = _config.sdo_pin;
    _spi_dev.SDIPinIndex    = _config.sdi_pin;
    _spi_dev.SCKPinIndex    = _config.sck_pin;
    _spi_dev.CSnPinIndex    = _config.csn_pin;
    _spi_dev.device_config  = kFLEXIO_SGPIO_TX_ONLY;
    switch (_config.port) {
        case SgpioPort::Port0:
            _spi_dev.shifterIndex[0] = 0;
            _spi_dev.shifterIndex[1] = 1;
            _spi_dev.shifterIndex[2] = 2;
            _spi_dev.shifterIndex[3] = 3;
            _spi_dev.timerIndex[0]   = 0;
            dma_request_source_tx    = kDma0RequestMuxFlexIO0ShiftRegister2Request;
            dma_tx_channel           = 2;
            break;
        case SgpioPort::Port1:
            _spi_dev.shifterIndex[0] = 4;
            _spi_dev.shifterIndex[1] = 5;
            _spi_dev.shifterIndex[2] = 6;
            _spi_dev.shifterIndex[3] = 7;
            _spi_dev.timerIndex[0]   = 1;
            dma_request_source_tx    = kDma0RequestMuxFlexIO0ShiftRegister6Request;
            dma_tx_channel           = 3;
            break;
        default: return false;
    }
    // EDMA setup
    DMA_Type* dma_base = DMA0;
    EDMA_CreateHandle(&_tx_handle, dma_base, dma_tx_channel);
    EDMA_SetChannelMux(dma_base, dma_tx_channel, dma_request_source_tx);
    // flexio setup
    flexio_spi_slave_config_t user_config;
    FLEXIO_SPI_SlaveGetDefaultConfig(&user_config);
    user_config.dataMode = kFLEXIO_SPI_128BitMode;
    FLEXIO_SPI_SlaveInit(&_spi_dev, &user_config);
    auto status = FLEXIO_SPI_SlaveTransferCreateHandleEDMA(
        &_spi_dev, &_spi_handle, sgpio_slave_callback, nullptr, &_tx_handle, nullptr);
    return status == kStatus_Success;
}

nv::sgpio::Driver::SgpioStatus
nv::sgpio::Driver::start(std::span<uint8_t>                  tx_data,
                         std::span<uint8_t>                  tx_buffer,
                         [[maybe_unused]] std::span<uint8_t> rx_buffer)
{
    constexpr uint32_t port0_error_mask   = 0x0f;
    constexpr uint32_t port1_error_mask   = 0xf0;
    constexpr uint32_t port0_error_enable = 0x01;
    constexpr uint32_t port1_error_enable = 0x10;
    if (tx_data.size() > tx_buffer.size()) {
        return SgpioStatus::Error;
    }
    memcpy(static_cast<uint8_t*>(tx_buffer.data()), tx_data.data(), tx_data.size());
    switch (_config.port) {
        case SgpioPort::Port0:
            FLEXIO_ClearShifterErrorFlags(_spi_dev.flexioBase, port0_error_mask);
            FLEXIO_EnableShifterErrorInterrupts(_spi_dev.flexioBase, port0_error_enable);
            break;
        case SgpioPort::Port1:
            FLEXIO_ClearShifterErrorFlags(_spi_dev.flexioBase, port1_error_mask);
            FLEXIO_EnableShifterErrorInterrupts(_spi_dev.flexioBase, port1_error_enable);
            break;
        default: return SgpioStatus::Error;
    }
    flexio_spi_transfer_t xfer   = {.txData   = static_cast<uint8_t*>(tx_buffer.data()),
                                    .rxData   = nullptr,
                                    .dataSize = tx_buffer.size(),
                                    .flags    = kFLEXIO_SPI_SGPIO};
    auto                  status = FLEXIO_SPI_SlaveTransferEDMA(&_spi_dev, &_spi_handle, &xfer);
    return to_status(status);
}

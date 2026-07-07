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

#include NV_IPC_CONFIG_H
#include "sys/spi_bm/spi_bm.h"
#include "nv/common/utils.h"
#include "nv/gpio/driver_bm.h"

namespace sys::spi::bm {

nv::spi::bm::SpiStatus Driver::get_status(status_t status)
{
    switch (status) {
        case kStatus_Success         : return nv::spi::bm::SpiStatus::Ok;
        case kStatus_LPSPI_Busy      : return nv::spi::bm::SpiStatus::Ok;
        case kStatus_LPSPI_Error     : return nv::spi::bm::SpiStatus::Error;
        case kStatus_LPSPI_Idle      : return nv::spi::bm::SpiStatus::Ok;
        case kStatus_LPSPI_OutOfRange: return nv::spi::bm::SpiStatus::Error;
        case kStatus_LPSPI_Timeout   : return nv::spi::bm::SpiStatus::EventTimeout;
    }
    return nv::spi::bm::SpiStatus::Error;
}

LPSPI_Type* Driver::get_base(nv::spi::Port port)
{
    constexpr uint8_t Size = nv::common::to_underlying(nv::spi::Port::End);
    // NOLINTNEXTLINE: SDK definition
    std::array<LPSPI_Type*, Size> bases LPSPI_BASE_PTRS;
    return bases.at(nv::common::to_underlying(port));
}

void Driver::bind(nv::spi::Port                                         port,
                  volatile uint32_t*                                    event,
                  nv::ecm_bm::AppEvent                                  event_bit,
                  std::array<nv::ipc::Gpios, nv::spi::bm::SpiMaxCsPins> cs_pins)
{
    _port       = port;
    _event      = event;
    _event_bit  = event_bit;
    _cs_gpios   = cs_pins;
    _current_cs = nv::spi::bm::CsPins::Cs0;
    _is_busy    = false;

    _master_xfer_ctx.driver = this;
    _base                   = get_base(port);

    deassert_all_cs();

    LPSPI_MasterTransferCreateHandle(_base, &_handle, master_callback, &_master_xfer_ctx);
}

nv::spi::bm::SpiStatus Driver::transfer(nv::spi::bm::CsPins      cs,
                                        uint32_t                 read_length,
                                        const std::span<uint8_t> send_buffer,
                                        nv::spi::bm::SpiFlags    flags)
{
    if (_is_busy) {
        return nv::spi::bm::SpiStatus::Busy;
    }
    if (read_length > nv::spi::bm::BufferSize || send_buffer.size() > nv::spi::bm::BufferSize) {
        return nv::spi::bm::SpiStatus::Error;
    }
    // Asymmetric write-then-read is not supported in a single transfer; caller
    // must split it into separate write and read transfers.
    if (read_length > 0 && send_buffer.size() > 0 && read_length != send_buffer.size()) {
        return nv::spi::bm::SpiStatus::Error;
    }

    const auto& [cs_port, cs_pin] = _cs_gpios.at(static_cast<size_t>(cs));
    if (cs_port == nv::gpio::InvalidGpioPort || cs_pin == nv::gpio::InvalidGpioPin) {
        return nv::spi::bm::SpiStatus::Error;
    }

    _current_cs = cs;

    _is_busy            = true;
    _result.flags       = flags;
    _result.read_length = read_length;

    const uint32_t data_size = std::max<uint32_t>(read_length,
                                                  static_cast<uint32_t>(send_buffer.size()));

    lpspi_transfer_t masterXfer;
    if (send_buffer.size() > 0) {
        memcpy(_tx_buffer.data(), send_buffer.data(), send_buffer.size());
        masterXfer.txData = _tx_buffer.data();
    }
    else {
        masterXfer.txData = nullptr;
    }
    masterXfer.rxData      = (read_length > 0) ? _result.buffer.data() : nullptr;
    masterXfer.dataSize    = data_size;
    masterXfer.configFlags = kLPSPI_MasterPcs0 | kLPSPI_MasterPcsContinuous
                           | kLPSPI_MasterByteSwap;

    status_t Status = kStatus_Success;
    if constexpr (nv::spi::WaitGpioSpiWar) {
        _is_wait_gpio_transaction = read_length > 0;
        _wait_gpio_flags.store((_is_wait_gpio_transaction && send_buffer.size() == 0)
                                   ? WaitGpioEvents::WriteDone
                                   : 0,
                               std::memory_order_release);

        // Avoid double read if we do read-only with wait for GPIO
        if (!_is_wait_gpio_transaction || (send_buffer.size() > 0)) {
            if (flags & nv::spi::bm::SpiFlags::CsAssert) {
                deassert_all_cs();
                assert_cs(_current_cs);
            }
            Status = LPSPI_MasterTransferNonBlocking(_base, &_handle, &masterXfer);
            if (Status != kStatus_Success) {
                _is_busy = false;
                if (flags & nv::spi::bm::SpiFlags::CsDeassert) {
                    deassert_cs(_current_cs);
                }
                return nv::spi::bm::SpiStatus::EventXferFail;
            }
            // Need to deassert CS during downtime between write and read to avoid bus
            // contention
            deassert_all_cs();
        }
    }
    else {
        if (flags & nv::spi::bm::SpiFlags::CsAssert) {
            deassert_all_cs();
            assert_cs(_current_cs);
        }

        Status = LPSPI_MasterTransferNonBlocking(_base, &_handle, &masterXfer);
        if (Status != kStatus_Success) {
            _is_busy = false;
            if (flags & nv::spi::bm::SpiFlags::CsDeassert) {
                deassert_cs(_current_cs);
            }
            return nv::spi::bm::SpiStatus::EventXferFail;
        }
    }

    return nv::spi::bm::SpiStatus::Ok;
}

nv::spi::bm::SpiResult& Driver::get_result()
{
    _is_busy = false;
    return _result;
}

void Driver::callback(status_t sdk_status)
{
    _result.status = get_status(sdk_status);

    if constexpr (nv::spi::WaitGpioSpiWar) {
        if (_is_wait_gpio_transaction) {
            if ((_wait_gpio_flags.load(std::memory_order_acquire) & WaitGpioEvents::ReadDone)
                != WaitGpioEvents::ReadDone) {
                deassert_cs(_current_cs);
                _wait_gpio_flags.fetch_or(WaitGpioEvents::WriteDone, std::memory_order_acq_rel);
                try_read_after_wait();
                return;
            }
            _is_wait_gpio_transaction = false;
        }
    }

    if (_result.flags & nv::spi::bm::SpiFlags::CsDeassert) {
        deassert_cs(_current_cs);
    }

    APP_EVENT_SET(*_event, _event_bit);
}

void Driver::master_callback([[maybe_unused]] LPSPI_Type*            base,
                             [[maybe_unused]] lpspi_master_handle_t* handle,
                             status_t                                status,
                             void*                                   userData)
{
    auto ctx = static_cast<MasterTransferContext*>(userData);
    static_cast<Driver*>(ctx->driver)->callback(status);
}

void Driver::spi_gpio_wait_complete()
{
    if constexpr (nv::spi::WaitGpioSpiWar) {
        if (_is_wait_gpio_transaction) {
            _wait_gpio_flags.fetch_or(WaitGpioEvents::GpioWaitDone, std::memory_order_acq_rel);
            try_read_after_wait();
        }
    }
}

bool Driver::is_busy()
{
    return _is_busy;
}

void Driver::try_read_after_wait()
{
    if constexpr (nv::spi::WaitGpioSpiWar) {
        uint8_t expected = WaitGpioEvents::Ready;
        if (_wait_gpio_flags.compare_exchange_strong(
                expected, WaitGpioEvents::ReadDone, std::memory_order_acq_rel)) {
            deassert_all_cs();
            assert_cs(_current_cs);

            lpspi_transfer_t masterXfer;
            masterXfer.txData      = nullptr;
            masterXfer.rxData      = _result.buffer.data();
            masterXfer.dataSize    = _result.read_length;
            masterXfer.configFlags = kLPSPI_MasterPcs0 | kLPSPI_MasterPcsContinuous
                                   | kLPSPI_MasterByteSwap;

            const status_t Status = LPSPI_MasterTransferNonBlocking(
                _base, &_handle, &masterXfer);
            if (Status != kStatus_Success) {
                callback(Status);
            }
        }
    }
}

void Driver::deassert_all_cs()
{
    for (size_t i = 0; i < nv::spi::bm::SpiMaxCsPins; ++i) {
        deassert_cs(static_cast<nv::spi::bm::CsPins>(i));
    }
}

void Driver::assert_cs(nv::spi::bm::CsPins cs)
{
    const auto& [port, pin] = _cs_gpios.at(static_cast<size_t>(cs));
    if (port == nv::gpio::InvalidGpioPort || pin == nv::gpio::InvalidGpioPin) {
        return;
    }
    nv::gpio::bm::Driver::write(port, pin, 0U);
}

void Driver::deassert_cs(nv::spi::bm::CsPins cs)
{
    const auto& [port, pin] = _cs_gpios.at(static_cast<size_t>(cs));
    if (port == nv::gpio::InvalidGpioPort || pin == nv::gpio::InvalidGpioPin) {
        return;
    }
    nv::gpio::bm::Driver::write(port, pin, 1U);
}

}  // namespace sys::spi::bm

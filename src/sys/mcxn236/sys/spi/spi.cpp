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
#include "nv/spi/task.h"
#include "nv/logger/log.h"

using namespace nv;
using namespace sys::spi;

LPSPI_Type* Driver::get_base(nv::spi::Flexcomm flexcomm)
{
    constexpr uint8_t Size = nv::common::to_underlying(nv::spi::Flexcomm::End);
    // NOLINTNEXTLINE: SDK definition
    std::array<LPSPI_Type*, Size> bases LPSPI_BASE_PTRS;
    return bases.at(nv::common::to_underlying(flexcomm));
}

void Driver::bind(nv::spi::Flexcomm  flexcomm,
                  nv::gpio::GpioPort cs_port,
                  nv::gpio::GpioPin  cs_pin,
                  bool               overwrite_freq,
                  uint8_t            freq,
                  void*              task)
{
    NV_ASSERT(task != nullptr);
    auto spi_task = static_cast<nv::spi::Task*>(task);
    logger::info(logger::Event::SpiBind,
                 {static_cast<uint8_t>(spi_task->id()), static_cast<uint8_t>(flexcomm)});
    _base           = get_base(flexcomm);
    cs_port_id      = cs_port;
    cs_pin_id       = cs_pin;
    _flexcomm       = flexcomm;
    _overwrite_freq = overwrite_freq;
    _freq           = freq;
}

void Driver::pull_cs(uint8_t level, uint32_t port, uint32_t pin)
{
    nv::gpio::Driver::write(port, pin, level);
}

clock_div_name_t Driver::getClockDivider(nv::spi::Flexcomm flexcomm)
{
    size_t idx = uint8_t(flexcomm);
    if (flexcomm > nv::spi::Flexcomm::End) {
        idx = (uint8_t)(nv::spi::Flexcomm::End)-1;
    }
    return clockDividers.at(idx);
}

void Driver::init()
{
    if (_overwrite_freq) {
        auto           kCLOCK_Div       = getClockDivider(_flexcomm);
        const uint32_t divided_by_value = MaxClockFreq / _freq;
        CLOCK_SetClkDiv(kCLOCK_Div, divided_by_value);
    }
    while (LPSPI_GetTxFifoCount(_base) != 0U) {}
    const uint32_t _tcr = _base->TCR
                        & (LPSPI_TCR_CPOL_MASK | LPSPI_TCR_CPHA_MASK | LPSPI_TCR_PRESCALE_MASK
                           | LPSPI_TCR_PCS_MASK);
    _tcr_tx            = _tcr | LPSPI_TCR_RXMSK_MASK;
    _tcr_rx            = _tcr | LPSPI_TCR_TXMSK_MASK;
    _tcr_tx_byte_shift = _tcr_tx | LPSPI_TCR_BYSW_MASK;
    _tcr_rx_byte_shift = _tcr_rx | LPSPI_TCR_BYSW_MASK;
}

void Driver::sendRecv(uint32_t                 send_len,
                      const std::span<uint8_t> sbuf,
                      uint32_t                 recv_len,
                      std::span<uint8_t>       rbuf,
                      uint8_t                  bitmap)
{
    // pull cs pin to 0
    if (bitmap & static_cast<uint8_t>(static_cast<uint8_t>(ActionBitmap::CsPull0))) {
        pull_cs(0, cs_port_id, cs_pin_id);
    }

    uint32_t lpspi_cmd = 0;

    // spi send
    if (send_len != 0 && sbuf.size() >= send_len) {
        if (bitmap & static_cast<uint8_t>(ActionBitmap::TxByteShift)) {
            lpspi_cmd = _tcr_tx_byte_shift;
        }
        else {
            lpspi_cmd = _tcr_tx;
        }
        uint32_t write_data = 0;
        uint32_t idx        = 0;

        if (send_len >= WordSize) {
            while ((LPSPI_GetStatusFlags(_base) & (uint32_t)kLPSPI_TxDataRequestFlag) == 0) {}
            _base->TCR = lpspi_cmd | LPSPI_TCR_FRAMESZ((WordSize * 8U) - 1);

            while (send_len >= WordSize) {
                write_data = ((uint32_t)(sbuf[idx]) << ByteShift3)
                           | ((uint32_t)(sbuf[idx + 1]) << ByteShift2)
                           | ((uint32_t)(sbuf[idx + 2]) << ByteShift1)
                           | ((uint32_t)(sbuf[idx + 3]));
                while ((LPSPI_GetStatusFlags(_base) & (uint32_t)kLPSPI_TxDataRequestFlag)
                       == 0) {}
                LPSPI_WriteData(_base, write_data);
                if (idx > UINT32_MAX - WordSize) {
                    return;
                }
                idx      += WordSize;
                send_len -= WordSize;
            }
        }

        if (send_len > 0) {
            while ((LPSPI_GetStatusFlags(_base) & (uint32_t)kLPSPI_TxDataRequestFlag) == 0) {}
            _base->TCR = lpspi_cmd | LPSPI_TCR_FRAMESZ((send_len * 8U) - 1);

            write_data = 0U;

            while (send_len > 0 && idx < sbuf.size()) {
                --send_len;
                write_data |= (uint32_t)(sbuf[idx++]) << (send_len * 8U);
            }

            while ((LPSPI_GetStatusFlags(_base) & (uint32_t)kLPSPI_TxDataRequestFlag) == 0) {}
            LPSPI_WriteData(_base, write_data);
        }
    }
    // spi send end

    // spi recv
    if (recv_len != 0 && rbuf.size() >= recv_len) {
        if (bitmap & static_cast<uint8_t>(ActionBitmap::RxByteShift)) {
            lpspi_cmd = _tcr_rx_byte_shift;
        }
        else {
            lpspi_cmd = _tcr_rx;
        }

        uint32_t read_data      = 0;
        uint32_t cur_read_len   = 0;
        uint32_t cur_frame_size = 0;
        uint32_t idx            = 0;

        while (recv_len > 0) {
            cur_read_len  = (recv_len > LPSPI_MAX_FRAME_BYTE) ? LPSPI_MAX_FRAME_BYTE : recv_len;
            recv_len     -= cur_read_len;

            cur_frame_size = cur_read_len * 8U;

            while ((LPSPI_GetStatusFlags(_base) & (uint32_t)kLPSPI_TxDataRequestFlag) == 0) {}
            // Will not happen
            if (cur_frame_size < 1) {
                return;
            }
            _base->TCR = lpspi_cmd | LPSPI_TCR_FRAMESZ(cur_frame_size - 1);

            if (cur_read_len >= WordSize) {
                while (cur_read_len >= WordSize) {
                    cur_read_len -= WordSize;
                    if (idx > UINT32_MAX - WordSize) {
                        return;
                    }
                    while ((LPSPI_GetStatusFlags(_base) & (uint32_t)kLPSPI_RxDataReadyFlag)
                           == 0) {}
                    read_data = LPSPI_ReadData(_base);

                    rbuf[idx++] = (uint8_t)((read_data >> ByteShift3) & UINT8_MAX);
                    rbuf[idx++] = (uint8_t)((read_data >> ByteShift2) & UINT8_MAX);
                    rbuf[idx++] = (uint8_t)((read_data >> ByteShift1) & UINT8_MAX);
                    rbuf[idx++] = (uint8_t)((read_data)&UINT8_MAX);
                }
            }

            if (cur_read_len > 0) {
                while ((LPSPI_GetStatusFlags(_base) & (uint32_t)kLPSPI_RxDataReadyFlag) == 0) {}
                read_data = LPSPI_ReadData(_base);

                while (cur_read_len > 0 && idx < rbuf.size()) {
                    --cur_read_len;
                    rbuf[idx++] = (uint8_t)((read_data >> (cur_read_len * 8U)) & UINT8_MAX);
                }
            }
        }
    }
    if (bitmap & static_cast<uint8_t>(ActionBitmap::CsPull1)) {
        /* Wait for transfer done. */
        while (LPSPI_GetTxFifoCount(_base) != 0) {}
        while ((LPSPI_GetStatusFlags(_base) & (uint32_t)kLPSPI_ModuleBusyFlag) != 0) {}
        pull_cs(1, cs_port_id, cs_pin_id);
    }
    // spi recv end
}
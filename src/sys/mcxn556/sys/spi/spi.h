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
#pragma once
#include <array>
#include <span>

#include "nv/gpio/driver.h"
#include "fsl_lpspi.h"
constexpr uint8_t ByteShift1 = 8;
constexpr uint8_t ByteShift2 = 16;
constexpr uint8_t ByteShift3 = 24;
constexpr uint8_t WordSize   = 4;
#define LPSPI_MAX_FRAME_BYTE (4096U / 8U)
#define LPSPI_MAX_FRAME_BITS (LPSPI_MAX_FRAME_BYTE * 8)

#include "nv/spi/port.h"

namespace sys::spi {

const std::array<clock_div_name_t, 8> clockDividers = {
    kCLOCK_DivFlexcom0Clk,
    kCLOCK_DivFlexcom1Clk,
    kCLOCK_DivFlexcom2Clk,
    kCLOCK_DivFlexcom3Clk,
    kCLOCK_DivFlexcom4Clk,
    kCLOCK_DivFlexcom5Clk,
    kCLOCK_DivFlexcom6Clk,
    kCLOCK_DivFlexcom7Clk,
};
constexpr uint32_t MaxClockFreq = 48;

class Driver
{
public:
    enum class ActionBitmap : uint8_t
    {
        None        = 0x0,
        CsPull0     = 0x1,
        CsPull1     = 0x2,
        CsPullBoth  = 0x3,
        TxByteShift = 0x4,
        RxByteShift = 0x8,
    };

    void bind(nv::spi::Flexcomm  flexcomm,
              nv::gpio::GpioPort cs_port,
              nv::gpio::GpioPin  cs_pin,
              bool               overwrite_freq,
              uint8_t            freq,
              void*              task);

    void init();

    static void pull_cs(uint8_t level, uint32_t port, uint32_t pin);

    void sendRecv(uint32_t                 send_len,
                  const std::span<uint8_t> sbuf,
                  uint32_t                 recv_len,
                  std::span<uint8_t>       rbuf,
                  uint8_t                  bitmap = (uint8_t)(ActionBitmap::CsPullBoth));

private:
    LPSPI_Type*        _base;
    nv::gpio::GpioPort cs_port_id;
    nv::gpio::GpioPin  cs_pin_id;
    nv::spi::Flexcomm  _flexcomm;
    bool               _overwrite_freq;
    uint8_t            _freq;  // x for x MHz, eq. 48 refers to 48MHz
    uint32_t           _tcr_tx;
    uint32_t           _tcr_tx_byte_shift;
    uint32_t           _tcr_rx;
    uint32_t           _tcr_rx_byte_shift;
    static LPSPI_Type* get_base(nv::spi::Flexcomm flexcomm);

    clock_div_name_t getClockDivider(nv::spi::Flexcomm flexcomm);
};

}  // namespace sys::spi
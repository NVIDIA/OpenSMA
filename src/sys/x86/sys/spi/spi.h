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
#include <stdint.h>

#include "nv/gpio/driver.h"
#include "nv/spi/port.h"

namespace sys::spi {
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

    void sendRecv(uint32_t                 send_len,
                  const std::span<uint8_t> sbuf,
                  uint32_t                 recv_len,
                  std::span<uint8_t>       rbuf,
                  uint8_t                  bitmap = 0);
};
}  // namespace sys::spi
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

#include <span>
#include <cstdint>
#include "sys/sgpio/driver.h"

namespace nv::sgpio {

class Driver : protected sys::sgpio::Driver
{
public:
    enum class SgpioStatus
    {
        Success,
        Error,
        Busy,
    };

    enum class SgpioPort
    {
        Port0,
        Port1
    };

    struct SgpioConfig
    {
        uint8_t   sdo_pin;
        uint8_t   sdi_pin;
        uint8_t   sck_pin;
        uint8_t   csn_pin;
        SgpioPort port;
    };

    Driver(SgpioConfig config);
    bool        init();
    SgpioStatus start(std::span<uint8_t> tx_data,
                      std::span<uint8_t> tx_buffer,
                      std::span<uint8_t> rx_buffer);

private:
    SgpioConfig _config{};
};

}  // namespace nv::sgpio

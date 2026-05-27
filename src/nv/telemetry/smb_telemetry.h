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

#include <stdint.h>
#include <span>
#include <array>
#include <tuple>
#include "nv/common/preproc.h"
#include "nv/mctp/enums.h"
#include "nv/gpio/common.h"
#include "nv/telemetry/types.h"
#if __has_include("telemetry.h")
#include "telemetry.h"
#endif

namespace nv::smb_telemetry {

// GPIO Alert mapping for SMBus Direct ALERTS register
using Gpios = std::tuple<nv::gpio::GpioPort, nv::gpio::GpioPin>;

// Product may override this weak default implementation in telemetry.cpp.
uint8_t get_end_address();

class SmbDirect
{
public:
    SmbDirect()                            = delete;
    SmbDirect(const SmbDirect&)            = delete;
    SmbDirect& operator=(const SmbDirect&) = delete;

    static void                     refresh_cache(std::span<uint8_t> cache);
    static std::span<const uint8_t> get_cache();

    static constexpr uint8_t StartAddress = 0x50;

    static constexpr size_t CacheSize = 160;

private:
    NV_SHARED_DATA static inline std::array<uint8_t, CacheSize> cache_ = []() {
        std::array<uint8_t, CacheSize> arr;
        arr.fill(0xff);
        return arr;
    }();
};

// Product-specific SMBus Direct helpers.
void refresh_product_specific_telemetries(const SmbDirectSensorMapping& entry,
                                          std::span<uint8_t>            cache,
                                          uint8_t                       buffer_pos);

}  // namespace nv::smb_telemetry

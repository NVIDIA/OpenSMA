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

#include <cstdint>
#include <span>

namespace sys::bootloader {

class Driver
{
public:
    Driver() = default;

    enum BootSource
    {
        BootSourceInternalFlash = 0,
        BootSourceFMC           = 2,
    };

    struct ApplicationFaultRecord
    {
        uint32_t           fault_magic;
        uint32_t           cfsr;
        uint32_t           hfsr;
        uint32_t           event_bits;
        std::span<uint8_t> to_span() const
        {
            return {std::bit_cast<uint8_t*>(this), sizeof(*this)};
        }
    };

    static constexpr uint32_t ApplicationFaultMagic = 0xFAFA5A5A;
};

}  // namespace sys::bootloader

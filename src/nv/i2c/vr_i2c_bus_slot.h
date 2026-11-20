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
#include <cstddef>
#include <cstdint>

namespace nv::i2c {

/** @brief Supported emulated I2C device types for a virtual bus. */
enum class VrI2cDeviceId : uint8_t
{
    At24c01,
    Pca9544a,
    Pca9555,
    I2cRtc,
};

/** @brief Compile-time device slot: type and 7-bit address (from project config.h). */
struct VrI2cBusSlot
{
    VrI2cDeviceId device_id;
    uint8_t       address;

    constexpr VrI2cBusSlot(VrI2cDeviceId device_id_, uint8_t address_) noexcept
    : device_id(device_id_)
    , address(address_)
    {}

    VrI2cBusSlot() = delete;
};

/** @brief True when @p slots have known device IDs, valid 7-bit addresses, and no duplicates.
 */
template<size_t SlotCount>
constexpr bool validate_vr_i2c_bus_slots(const std::array<VrI2cBusSlot, SlotCount>& slots)
{
    for (size_t i = 0U; i < SlotCount; ++i) {
        if (slots.at(i).address >= 128U) {
            return false;
        }

        switch (slots.at(i).device_id) {
            case VrI2cDeviceId::At24c01:
            case VrI2cDeviceId::Pca9544a:
            case VrI2cDeviceId::Pca9555:
            case VrI2cDeviceId::I2cRtc  : break;
            default                     : return false;
        }

        for (size_t j = i + 1U; j < SlotCount; ++j) {
            if (slots.at(i).address == slots.at(j).address) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace nv::i2c

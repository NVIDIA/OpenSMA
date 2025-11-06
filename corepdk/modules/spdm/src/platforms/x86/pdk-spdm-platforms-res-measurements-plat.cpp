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
#include "pdk-spdm-app-res-measurements-plat.h"

namespace pdk::spdm::platforms::res::measurements {
namespace {  // namespace anonymous for dummy data
constexpr uint8_t                MeasurementNumber = 3;
constexpr uint8_t                MeasurementType1  = 0x1;
constexpr uint8_t                MeasurementType2  = 0x2;
constexpr uint8_t                MeasurementType3  = 0x3;
constexpr std::array<uint8_t, 1> MeasurementData1  = {0x01};
constexpr std::array<uint8_t, 2> MeasurementData2  = {0x02, 0x03};
constexpr std::array<uint8_t, 3> MeasurementData3  = {0x03, 0x04, 0x05};
};  // namespace

bool get_measurement(uint8_t meas_index, std::span<uint8_t> meas_buffer)
{
    if (meas_index == 0 || meas_index > get_measurement_number()) {
        return false;
    }
    if (meas_buffer.size() < get_measurement_size(meas_index)) {
        return false;
    }
    if (meas_index == 1) {
        std::copy(MeasurementData1.begin(), MeasurementData1.end(), meas_buffer.begin());
        return true;
    }
    if (meas_index == 2) {
        std::copy(MeasurementData2.begin(), MeasurementData2.end(), meas_buffer.begin());
        return true;
    }
    if (meas_index == 3) {
        std::copy(MeasurementData3.begin(), MeasurementData3.end(), meas_buffer.begin());
        return true;
    }
    return false;
}
uint16_t get_measurement_size(uint8_t meas_index)
{
    if (meas_index == 0 || meas_index > get_measurement_number()) {
        return 0;
    }

    if (meas_index == 1) {
        return MeasurementData1.size();
    }
    if (meas_index == 2) {
        return MeasurementData2.size();
    }
    if (meas_index == 3) {
        return MeasurementData3.size();
    }
    return 0;
}
uint8_t get_measurement_type(uint8_t meas_index)
{
    if (meas_index == 0 || meas_index > get_measurement_number()) {
        return 0;
    }
    if (meas_index == 1) {
        return MeasurementType1;
    }
    if (meas_index == 2) {
        return MeasurementType2;
    }
    if (meas_index == 3) {
        return MeasurementType3;
    }
    return 0;
}

uint8_t get_measurement_number()
{
    return MeasurementNumber;
}

bool get_measurement_signature(
    [[maybe_unused]] pdk::spdm::app::res::algorithms::BaseAsymSel base_asym_algo,
    [[maybe_unused]] std::span<const uint8_t>                     hash_data,
    [[maybe_unused]] std::span<uint8_t>                           signature_buffer)
{
    return false;
}

}  // namespace pdk::spdm::platforms::res::measurements

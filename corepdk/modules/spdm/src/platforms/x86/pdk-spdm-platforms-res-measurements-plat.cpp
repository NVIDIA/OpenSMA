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
#include "app/pdk-spdm-app-res-measurements-plat.h"
namespace pdk::spdm::platforms::res::measurements {

bool get_measurement([[maybe_unused]] uint8_t            meas_index,
                     [[maybe_unused]] std::span<uint8_t> meas_buffer)
{
    std::fill(meas_buffer.rbegin(), meas_buffer.rend(), 0x0);
    return true;
}
uint16_t get_measurement_size([[maybe_unused]] uint8_t meas_index)
{
    return 0;
}
uint8_t get_measurement_type([[maybe_unused]] uint8_t meas_index)
{
    return 0;
}
uint8_t get_measurement_number()
{
    return 0;
}
bool get_nonce([[maybe_unused]] std::span<uint8_t> nonce_buffer)
{
    std::fill(nonce_buffer.rbegin(), nonce_buffer.rend(), 0x0);
    return true;
}
}  // namespace pdk::spdm::platforms::res::measurements

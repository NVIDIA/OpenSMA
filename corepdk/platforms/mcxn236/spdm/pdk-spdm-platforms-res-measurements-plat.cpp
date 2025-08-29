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
#include "corepdk/modules/spdm/src/app/pdk-spdm-app-res-measurements-plat.h"
#include "nv/spdm/spdm_get_measurement.h"
#include "nv/spdm/spdm_crypto_helper.h"
namespace pdk::spdm::platforms::res::measurements {

bool get_measurement(uint8_t meas_index, std::span<uint8_t> meas_buffer)
{
    if (meas_index == 0 || meas_index > get_measurement_number()) {
        return false;
    }

    nv::spdm::measurement::spdm_get_measurement(
        static_cast<nv::spdm::measurement::MeasNumT>(meas_index), meas_buffer.data());
    return true;
}
uint16_t get_measurement_size(uint8_t meas_index)
{
    if (meas_index == 0 || meas_index > get_measurement_number()) {
        return 0;
    }

    return nv::spdm::measurement::spdm_get_meas_record_info(
               static_cast<nv::spdm::measurement::MeasNumT>(meas_index))
        .meas_size;
}
uint8_t get_measurement_type(uint8_t meas_index)
{
    if (meas_index == 0 || meas_index > get_measurement_number()) {
        return 0;
    }

    return nv::spdm::measurement::spdm_get_meas_record_info(
               static_cast<nv::spdm::measurement::MeasNumT>(meas_index))
        .meas_type;
}

uint8_t get_measurement_number()
{
    return nv::spdm::measurement::spdm_get_num_meas();
}

bool get_nonce(std::span<uint8_t> nonce_buffer)
{
    auto const Ret = nv::spdm::crypto::spdm_random_data(nonce_buffer.data(),
                                                        nonce_buffer.size());
    if (Ret != nv::spdm::crypto::CryptoStatus::Success) {
        return false;
    }
    return true;
}
}  // namespace pdk::spdm::platforms::res::measurements

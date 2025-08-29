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

#include "pdk-spdm-app-res-algorithms-enum.h"
#include "pdk-spdm-app-res-hash-library-plat.h"

namespace pdk::spdm::platforms::res::ada::library {

constexpr uint32_t NonceSize           = 32;
constexpr uint32_t SpdmResponderNumber = 2;
struct PlatformContext
{
    uint8_t                                                 inst_num;
    pdk::spdm::app::res::algorithms::MeasurementHashAlgo    measurement_hash_alg;
    size_t                                                  measurement_hash_alg_length;
    pdk::spdm::app::res::algorithms::BaseHashSel            base_hash_algo;
    size_t                                                  base_hash_algo_length;
    pdk::spdm::app::res::algorithms::BaseAsymSel            base_asym_algo;
    uint32_t                                                base_asym_algo_length;
    uint8_t                                                 hash_started;
    uint8_t                                                 hash_in_use;
    uint8_t                                                 nonce_good;
    std::array<uint8_t, NonceSize>                          nonce;
    pdk::spdm::platforms::res::hash::library::Sha384Context hash_context;
};

enum class ResponsderNumber : int
{
    ResponsderNumber1 = 1,
    ResponsderNumber2 = 2,
};

ResponsderNumber has_data_for_spdm_responsder();

void   get_data_from_spdm_responsder(std::span<const uint8_t> data_from_spdm_responsder);
size_t send_data_to_spdm_responsder(std::span<uint8_t> data_to_spdm_responsder);

void spdm_platform_context_initialize(PlatformContext** instance);

}  // namespace pdk::spdm::platforms::res::ada::library
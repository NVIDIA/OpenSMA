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
#include "app/pdk-spdm-app-res-ada-library-plat.h"

namespace pdk::spdm::platforms::res::ada::library {

ResponsderNumber has_data_for_spdm_responsder()
{
    while (true) {}
}

void get_data_from_spdm_responsder(
    [[maybe_unused]] std::span<const uint8_t> data_from_spdm_responsder)
{}
size_t send_data_to_spdm_responsder([[maybe_unused]] std::span<uint8_t> data_to_spdm_responsder)
{
    return 0;
}

void spdm_platform_context_initialize(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext** instance)
{}

}  // namespace pdk::spdm::platforms::res::ada::library
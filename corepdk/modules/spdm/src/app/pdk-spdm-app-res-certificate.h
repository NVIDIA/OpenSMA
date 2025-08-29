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

#include "pdk-spdm-app-res-ada-library.h"

namespace pdk::spdm::app::res::certificate {

#ifdef __cplusplus
extern "C" {
#endif

uint8_t spdm_platform_validate_certificate_request(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    uint8_t                                                   slot,
    uint16_t                                                  offset,
    uint16_t                                                  length);

void spdm_platform_get_certificate_data(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    int8_t*                                                   data,
    uint8_t                                                   slot,
    uint16_t                                                  offset,
    uint16_t*                                                 length,
    uint16_t*                                                 total_length);

#ifdef __cplusplus
}
#endif

bool check_slot_existed(uint8_t slot_id);

}  // namespace pdk::spdm::app::res::certificate
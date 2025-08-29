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

#include "pdk-spdm-app-res-ada-library-plat.h"

namespace pdk::spdm::app::res::ada::library {

#ifdef __cplusplus
extern "C" {
#endif
// spdm responsder entry point
// this function will not return, please use muti-thread
extern void spdm_responder_main(void);

void spdm_platform_context_initialize_c(
    pdk::spdm::platforms::res::ada::library::PlatformContext** instance);
int  has_data_for_spdm_responsder_c(void);
void get_data_from_spdm_responsder_c(const void* const buffer, const size_t num_bytes);
void send_data_to_spdm_responsder_c(void* const buffer, size_t* const num_bytes);

#ifdef __cplusplus
}
#endif

}  // namespace pdk::spdm::app::res::ada::library

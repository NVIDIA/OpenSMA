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
#include <span>
#include <stdint.h>

namespace pdk::spdm::app::res::certificate {

#ifdef __cplusplus
extern "C" {
#endif
std::span<uint8_t> spdm_library_get_certificate_chain(uint8_t slot_id);

#ifdef __cplusplus
}
#endif

bool check_slot_existed(uint8_t slot_id);

}  // namespace pdk::spdm::app::res::certificate
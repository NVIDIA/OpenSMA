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
#include "pdk-spdm-app-res-certificate.h"

#include <bitset>

#include "pdk-spdm-app-res-certificate-plat.h"
#include "pdk-spdm-app-res-digests-plat.h"
#include "pdk-spdm-app-res-digests.h"
namespace pdk::spdm::app::res::certificate {
bool check_slot_existed(uint8_t slot_id)
{
    std::bitset<app::res::digests::MaxSlots> slot_available_index(
        platforms::res::digests::get_slot_mask());
    if (slot_id >= 8 || slot_available_index[slot_id] == 0) {
        return false;
    }
    return true;
}

#ifdef __cplusplus
extern "C" {
#endif
std::span<uint8_t> spdm_library_get_certificate_chain(uint8_t slot_id)
{
    return platforms::res::certificate::get_certificate_chain(slot_id);
}
#ifdef __cplusplus
}
#endif

}  // namespace pdk::spdm::app::res::certificate

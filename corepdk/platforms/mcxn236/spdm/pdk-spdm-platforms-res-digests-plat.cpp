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
#include "corepdk/modules/spdm/src/app/pdk-spdm-app-res-digests-plat.h"
#include "corepdk/modules/spdm/src/app/pdk-spdm-app-res-hash-library-plat.h"
#include "nv/spdm/spdm_cert_chain.h"
namespace pdk::spdm::platforms::res::digests {

uint8_t get_slot_mask()
{
    return nv::spdm::cert::get_slot_mask();
}
std::array<uint8_t, pdk::spdm::platforms::res::hash::type::Sha384HashSize>
get_digest(uint8_t slot_index)
{
    std::array<uint8_t, pdk::spdm::platforms::res::hash::type::Sha384HashSize> slot_digest{};
    slot_digest.fill(0x0);
    nv::spdm::cert::get_digest_for_slot(slot_index, slot_digest.data());
    return slot_digest;
}

}  // namespace pdk::spdm::platforms::res::digests
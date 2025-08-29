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

uint8_t spdm_platform_validate_certificate_request(
    [[maybe_unused]] pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    uint8_t                                                                    slot,
    uint16_t                                                                   offset,
    [[maybe_unused]] uint16_t                                                  length)
{
    // validate the slot number
    if (!check_slot_existed(slot)) {
        return 0;
    }
    const size_t ChainLength = platforms::res::certificate::get_certificate_length(slot);

    // check that the offset is within range, compare offset to chain_length
    // don't add length to offset because we want to allow the last request to
    // return the remaining part of the certificate
    if (offset > ChainLength) {
        return 0;
    }

    return 1;
}

void spdm_platform_get_certificate_data(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    int8_t*                                                   data,
    uint8_t                                                   slot,
    uint16_t                                                  offset,
    uint16_t*                                                 length,
    uint16_t*                                                 total_length)
{
    if (instance == nullptr || data == nullptr || length == nullptr
        || total_length == nullptr) {
        if (length) {
            *length = 0;
        }
        return;
    }

    // check slot number is available
    if (!check_slot_existed(slot)) {
        *length = 0;
        return;
    }

    // for not using reinterpret_cast
    const size_t InputBufferSize  = *length;
    auto         void_data_ptr    = static_cast<void*>(&data[0]);
    auto         uint8_t_data_ptr = static_cast<uint8_t*>(void_data_ptr);
    const size_t CopyBytes        = pdk::spdm::platforms::res::certificate::get_certificate(
        slot, offset, InputBufferSize, std::span<uint8_t>(uint8_t_data_ptr, InputBufferSize));

    // check the return length is less or equal than request length
    if (CopyBytes > InputBufferSize) {
        *length = 0;
        return;
    }
    *total_length = pdk::spdm::platforms::res::certificate::get_certificate_length(slot);
    *length       = CopyBytes;
    return;
}

#ifdef __cplusplus
}
#endif

}  // namespace pdk::spdm::app::res::certificate

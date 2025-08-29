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
#include "pdk-spdm-app-res-digests.h"

#include <bitset>
#include <limits>

#include "pdk-spdm-app-res-digests-plat.h"
namespace pdk::spdm::app::res::digests {
#ifdef __cplusplus
extern "C" {
#endif
void spdm_platform_get_digests_data(
    pdk::spdm::platforms::res::ada::library::PlatformContext* instance,
    int8_t*                                                   data,
    int32_t*                                                  length,
    uint8_t*                                                  slot_mask)
{
    // only support sha384 so validate the buffer is large enough, currently only
    // support 1 slot validate all the pointers
    if (data == nullptr || length == nullptr || slot_mask == nullptr || instance == nullptr) {
        if (length != nullptr) {
            *length = 0;
        }
        return;
    }

    // get available slot number
    *slot_mask = platforms::res::digests::get_slot_mask();

    std::bitset<MaxSlots> slot_available_index(platforms::res::digests::get_slot_mask());
    const uint8_t         NumAvailableSlots = slot_available_index.count();

    // save the total length of input buffer and check the buffer is enough to
    // save the digest.
    const int32_t  DataBufferLength = *length;
    constexpr auto Sha384Size       = pdk::spdm::platforms::res::hash::type::Sha384HashSize;

    const auto DigestNeededSize = Sha384Size * NumAvailableSlots;
    if (DataBufferLength < 0
        || DigestNeededSize > static_cast<decltype(DigestNeededSize)>(DataBufferLength)) {
        // input buffer from spdm library is too small, return 0 length
        *length = 0;
        return;
    }

    // clear the data buffer first
    std::fill(data, data + DataBufferLength, 0x00);

    for (uint32_t slot = 0, digest_offset = 0; slot < slot_available_index.size(); slot++) {
        if (slot_available_index[slot]) {
            // get digest when the slot is available to provide certificate chain from
            // index 0 to index 7 if the mask bit is set.
            // for not using reinterpret_cast
            auto void_data_ptr    = static_cast<void*>(&data[0]);
            auto uint8_t_data_ptr = static_cast<uint8_t*>(void_data_ptr);

            *std::bit_cast<std::array<uint8_t, Sha384Size>*>(
                uint8_t_data_ptr + digest_offset) = platforms::res::digests::get_digest(slot);
            if (digest_offset
                <= std::numeric_limits<decltype(digest_offset)>::max() - Sha384Size) {
                digest_offset += Sha384Size;
            }
        }
    }

    if (DigestNeededSize > Sha384Size * MaxSlots) {
        *length = 0;
        return;
    }

    *length = static_cast<int32_t>(DigestNeededSize);
    return;
}

#ifdef __cplusplus
}
#endif

}  // namespace pdk::spdm::app::res::digests

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
#include "pdk-spdm-app-res-version.h"

namespace pdk::spdm::app::res::version {
template<size_t N>
SpdmResVersionPayload& SpdmResVersionPayload::from_arr(std::array<uint8_t, N>& arr)
{
    static_assert(N >= sizeof(SpdmResVersionPayload),
                  "Input array is not enough to transfrom to SpdmResVersionPayload struct");
    return *std::bit_cast<SpdmResVersionPayload*>(arr.data());
}

void spdm_library_get_versions(bool& support_spdm_1_1,
                               bool& support_spdm_1_2,
                               bool& support_spdm_1_3)
{
    pdk::spdm::platforms::res::version::get_versions(
        support_spdm_1_1, support_spdm_1_2, support_spdm_1_3);
}

}  // namespace pdk::spdm::app::res::version
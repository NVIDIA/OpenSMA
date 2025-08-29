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
#include "app/pdk-spdm-app-res-signature-library-plat.h"

namespace pdk::spdm::platforms::res::signature::library {
bool get_signature_from_hash(
    [[maybe_unused]] const std::array<uint8_t,
                                      pdk::spdm::platforms::res::hash::type::Sha384HashSize>&
        input_hash,
    [[maybe_unused]] std::array<
        uint8_t,
        pdk::spdm::platforms::res::signature::type::Ecdsa384SignatureSize>& signature_buffer)
{
    return true;
}
}  // namespace pdk::spdm::platforms::res::signature::library
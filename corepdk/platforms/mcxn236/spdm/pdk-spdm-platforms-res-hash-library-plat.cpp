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
#include "corepdk/modules/spdm/src/app/pdk-spdm-app-res-hash-library-plat.h"
namespace pdk::spdm::platforms::res::hash::library {

bool Sha384Context::init()
{
    constexpr static int UsingSha384 = 1;
    const auto           ReturnCode  = mbedtls_sha512_starts_ret(&_hash_context, UsingSha384);
    if (ReturnCode != 0) {
        return false;
    }
    return true;
}

bool Sha384Context::update(std::span<uint8_t> input_data)
{
    const auto ReturnCode = mbedtls_sha512_update_ret(
        &_hash_context, input_data.data(), input_data.size());
    if (ReturnCode != 0) {
        return false;
    }

    return true;
}
bool Sha384Context::finish(
    std::array<uint8_t, pdk::spdm::platforms::res::hash::type::Sha384HashSize>& arr)
{
    const auto ReturnCode = mbedtls_sha512_finish_ret(&_hash_context, arr.data());
    if (ReturnCode != 0) {
        return false;
    }
    return true;
}

}  // namespace pdk::spdm::platforms::res::hash::library
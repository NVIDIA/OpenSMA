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
#include <numeric>

#include "app/pdk-spdm-app-res-hash-library-plat.h"
namespace pdk::spdm::platforms::res::hash::library {

bool Sha384Context::init()
{
    _hash_context.fill(0x0);
    return true;
}

bool Sha384Context::update(std::span<uint8_t> input_data)
{
    std::fill(input_data.begin(), input_data.end(), 0x0);
    return true;
}
bool Sha384Context::finish(
    std::array<uint8_t, pdk::spdm::platforms::res::hash::type::Sha384HashSize>& arr)
{
    std::iota(arr.begin(), arr.end(), 0x0);
    return true;
}

}  // namespace pdk::spdm::platforms::res::hash::library
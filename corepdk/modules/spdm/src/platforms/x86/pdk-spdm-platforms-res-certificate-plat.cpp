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
#include "app/pdk-spdm-app-res-certificate-plat.h"
namespace pdk::spdm::platforms::res::certificate {

size_t get_certificate([[maybe_unused]] uint8_t            slot,
                       [[maybe_unused]] size_t             offset,
                       [[maybe_unused]] size_t             length,
                       [[maybe_unused]] std::span<uint8_t> buffer_to_write)
{
    return 0x1;
}

size_t get_certificate_length([[maybe_unused]] uint8_t slot)
{
    return 0x1;
}

}  // namespace pdk::spdm::platforms::res::certificate

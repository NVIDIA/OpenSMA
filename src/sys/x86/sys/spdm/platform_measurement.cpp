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
#include "sys/spdm/platform_measurement.h"

namespace sys::spdm {
std::array<uint8_t, 16> get_boot_rom_cmac_in_kernel()
{
    return {};
}
std::array<std::pair<uint32_t, uint32_t>, 0> get_fuse_index_mask_pair()
{
    return {};
}

}  // namespace sys::spdm

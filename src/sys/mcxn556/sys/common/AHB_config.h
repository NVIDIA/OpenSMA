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

#pragma once
#include <cstdint>
#include NV_IPC_CONFIG_H

namespace sys::common {

/**
 * @brief AHB access level enumeration
 *
 * Defines the security and privilege levels for AHB memory regions.
 * Each value represents a 4-bit configuration used in AHB security registers.
 */
enum class AHBAccessLevel : uint32_t
{
    NonSecureUser       = 0x0U,  ///< Non-secure, User access allowed
    NonSecurePrivileged = 0x1U,  ///< Non-secure, Privileged access allowed
    SecureUser          = 0x2U,  ///< Secure, User access allowed
    SecurePrivileged    = 0x3U   ///< Secure, Privileged access allowed
};

void AHBConfig();

}  // namespace sys::common
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

namespace sys::common {

/**
 * @brief ARM TrustZone security extension address offset
 *
 * When ARM_FEATURE_CMSE is enabled (TrustZone security extension),
 * secure addresses are offset by 0x10000000 from non-secure addresses.
 * This allows the same peripheral to be accessed from both secure and
 * non-secure worlds with different address ranges.
 */
#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
static constexpr uint32_t SecureAddr = 0x10000000U;
#else
static constexpr uint32_t SecureAddr = 0x00000000U;
#endif

}  // namespace sys::common
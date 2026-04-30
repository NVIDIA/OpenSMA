/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
#include "enums.h"

namespace nv::mctp {

// Write Protection GPIO configuration structure
struct WriteProtectionGpioConfig
{
    Type4WriteProtectionFunction function;
    uint8_t                      port;
    uint8_t                      pin;
    // Bit position in GetSmaBaseboardSettings(data_index=0x00) response
    // byte 0. Use values from nv::mctp::T5SmaBaseboardWpResponseBit
    // defined in nsm_type_5.h. Stored as uint8_t here to keep nsm_common.h
    // free of Type 5 specific dependencies.
    uint8_t response_bit_pos;
};

}  // namespace nv::mctp

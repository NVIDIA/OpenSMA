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

#include <cstddef>
#include <cstdint>

#include "nv/vrot/interface/types.h"

namespace nv::mctp::ap_provision {

constexpr uint8_t MsgVersion = 0x01;

struct [[gnu::packed]] ProvisionRequestHeader
{
    vrot::ApId ap_id;
    uint8_t    sub_command;
};

struct [[gnu::packed]] ProvisionResponse
{
    vrot::ApId ap_id;
    uint8_t    sub_command;
    uint8_t    completion_code;
};

struct [[gnu::packed]] QueryStatusRequest
{
    vrot::ApId ap_id;
    uint8_t    sub_command;
};

struct [[gnu::packed]] QueryStatusResponse
{
    vrot::ApId ap_id;
    uint8_t    sub_command;
    uint8_t    provision_info;
};

static_assert(sizeof(ProvisionRequestHeader) == 2, "Invalid AP provision request header");
static_assert(sizeof(ProvisionResponse) == 3, "Invalid AP provision response");
static_assert(sizeof(QueryStatusRequest) == 2, "Invalid AP provision status request");
static_assert(sizeof(QueryStatusResponse) == 3, "Invalid AP provision status response");

}  // namespace nv::mctp::ap_provision

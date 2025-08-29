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
#include <array>
#include <stddef.h>

#include "pdk-spdm-app-res-code.h"
#include "stdint.h"

namespace pdk::spdm::app::res {
namespace version {
using MajorVersion        = uint8_t;
using MinorVersion        = uint8_t;
using UpdateVersionNumber = uint8_t;
using Alpha               = uint8_t;

struct VersionNumberEntry
{
    MajorVersion        major_version;
    MinorVersion        minor_version;
    UpdateVersionNumber update_version_number;
    Alpha               alpha;
};

using SPDMVersion             = uint8_t;
using Param1                  = uint8_t;
using Param2                  = uint8_t;
using Reserved                = uint8_t;
using VersionNumberEntryCount = uint8_t;
// use for stack usage
constexpr size_t VersionNumberEntryMaxValue = 8u;
using VersionNumberEntryList = std::array<VersionNumberEntry, VersionNumberEntryMaxValue>;

struct [[gnu::packed]] SpdmResVersionPayload
{
    SPDMVersion             spdm_version;
    code::SpdmResponseCode  RequestResponseCode;
    Param1                  param1;
    Param2                  param2;
    Reserved                reserved;
    VersionNumberEntryCount version_number_entry_count;
    VersionNumberEntryList  version_number_entry_list;
    template<size_t N>
    static SpdmResVersionPayload& from_arr(std::array<uint8_t, N>& arr);
};
static_assert(sizeof(SpdmResVersionPayload) - sizeof(VersionNumberEntryList) == 6,
              "SpdmResVersionPayload struct size check fail");

}  // namespace version
}  // namespace pdk::spdm::app::res

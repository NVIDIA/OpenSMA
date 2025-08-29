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
#include <cstdint>

#include NV_REG_CONFIG_H

namespace nv::reg_table {

// Downstream APIs
enum class DsApiType : uint8_t
{
    Begin,
    Info = Begin,
    Gpio,
    Event
};

// All regtable entries have a downstream data portion of 4 bytes
constexpr int DsDataSize = 4;
using DsData             = std::array<uint8_t, DsDataSize>;

// Downstream Interface Class
class DsInterface
{
public:
    virtual bool read(reg_ds::Handle handle, DsData& data)  = 0;
    virtual bool write(reg_ds::Handle handle, DsData& data) = 0;
};

}  // namespace nv::reg_table

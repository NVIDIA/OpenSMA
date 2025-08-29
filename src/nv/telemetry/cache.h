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
#include "config.h"
#include <stdint.h>
#include <array>
#include <span>
#include "nv/telemetry/utils.h"

namespace nv::telemetry {

class Cache
{
public:
    enum ErrorItem : uint8_t
    {
        I2cSensorAlert = 0,
        MaxAlertItem   = 1
    };

    static constexpr uint8_t  CacheSize     = static_cast<uint8_t>(TelemId::MaxItem);
    static constexpr uint8_t  AlertSize     = static_cast<uint8_t>(ErrorItem::MaxAlertItem);
    static constexpr uint8_t  InvalidData   = 0xFF;
    static constexpr uint32_t InvalidItem   = 0xFFFFFFFF;
    static constexpr uint32_t TempThreshold = (105 * 0.8);

    using Value      = uint32_t;
    using Table      = std::array<uint8_t, CacheSize * sizeof(Value)>;
    using AlertTable = std::array<bool, AlertSize>;

    static Cache& inst();

    void  init();
    Table get_table();
    void  set_cache(TelemId item, Value value);
    Value get_cache(TelemId item);
    void  set_error(ErrorItem item);
    void  clear_error(ErrorItem item);

private:
    Table      _table;
    AlertTable _alert_table;

    void refresh();
};

}  // namespace nv::telemetry

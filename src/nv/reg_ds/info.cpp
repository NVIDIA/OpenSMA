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

#include "nv/reg_ds/info.h"

#include "nv/logger/log.h"

using namespace nv;
using namespace reg_table;
using namespace reg_ds;

constexpr uint8_t Shift0   = 0;
constexpr uint8_t Shift8   = 8;
constexpr uint8_t Shift16  = 16;
constexpr uint8_t Shift24  = 24;
constexpr uint8_t ByteMask = 0xFF;

// Handler: get Table Identifier
void Info::get_table_id(DsData& data)
{
    data[0] = (TableIdentifier >> Shift24) & ByteMask;
    data[1] = (TableIdentifier >> Shift16) & ByteMask;
    data[2] = (TableIdentifier >> Shift8) & ByteMask;
    data[3] = (TableIdentifier >> Shift0) & ByteMask;
}

// Route the handler enum to the custom function to get/set data
bool Info::call_handler(Handle handler, DsData& data)
{
    switch (handler) {
        case Handle::InfoGetTableIdentifier: get_table_id(data); break;
        default:
            nv::error("Info handler %d not supported\n", handler);
            return false;
            break;
    }
    return true;
}

// Read from an Info layer entry by calling their handler
bool Info::read(Handle handle, DsData& data)
{
    auto index = static_cast<size_t>(handle);
    nv::info("RegInfo read at handle %d\n", index);

    if (index >= InfoSize) {
        nv::error("Index out of bounds of info handles\n");
        return false;
    }
    else {
        if (call_handler(handle, data)) {
            nv::info("Info read successful\n");
        }
        else {
            return false;
        }
    }
    return true;
}

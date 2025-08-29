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

#include "nv/reg_table/table.h"

#include "nv/common/preproc.h"
#include "nv/logger/log.h"

using namespace nv;
using namespace reg_table;

// lookup an entry in the register table that matches group and entry IDs
inline auto Table::table_lookup(TableEntry entry)
{
    for (auto it = _tbl.begin(); it != _tbl.end(); ++it) {
        if (entry == it->entry_id) {
            return it;
        }
    }
    return _tbl.end();
}

// generic read of the register table
bool Table::read(TableEntry entry, Data& data)
{
    auto regit = table_lookup(entry);

    if (regit == _tbl.end()) {
        nv::error("No entry found for Reg Table entry %d\n", entry);
        return false;
    }
    else {
        // copy register entry into data
        data.ctrl          = regit->ctrl;
        data.api_type      = regit->api_type;
        data.cached_reason = regit->cached_reason;

        // read downstream interface for entry
        switch (regit->api_type) {
            case DsApiType::Info: {
                nv::info("Read Reg Table INFO\n");
                if (!_dsinfo.read(regit->ds_handle, data.ds_data)) {
                    return false;
                }
                break;
            }
            case DsApiType::Gpio: {
                nv::info("Read Reg Table GPIO\n");
                if (!_dsgpio.read(regit->ds_handle, data.ds_data)) {
                    return false;
                }
                break;
            }
            case DsApiType::Event: {
                // event data just reads control bits
                nv::info("Read Reg Table EVENT\n");
                break;
            }
            default: {
                return false;
                break;
            }
        }
    }
    return true;
}

// generic write to the register table
bool Table::write(TableEntry entry, const Data& data)
{
    auto regit = table_lookup(entry);

    if (regit == _tbl.end()) {
        nv::error("No entry found for Reg Table entry %d\n", entry);
        return false;
    }
    else {
        // update writeable portion of entry
        regit->ctrl          = data.ctrl;
        regit->cached_reason = data.cached_reason;
        // TODO: downstream write goes here - GFWLYNT1-424
    }

    return true;
}

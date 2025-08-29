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
#include "nv/common/utils.h"
#if UBS_FEATURES_reg_table == 1
#include <array>
#include <cstdint>

#include "nv/reg_ds/gpio.h"
#include "nv/reg_ds/info.h"

#include NV_REG_CONFIG_H

namespace nv::reg_table {

// RAS related and other control bits
union ControlBits
{
    struct Bits
    {
        uint8_t is_ras       : 1;  // Is this a RAS entry (handles event, EINJ, etc.)
        uint8_t event_enable : 1;  // Enable event notification for this entry
        uint8_t einj_enable  : 1;  // Turn on error injection (generate error event)
        uint8_t event_status : 1;  // When set, indicates the event trigger is asserted (error
                                   // status)
        uint8_t rsvd         : 4;
    } bits;
    uint8_t byte;
};

// The register table entry
struct [[gnu::packed]] Entry
{
    TableEntry     entry_id;  // Telemetry Entry ID
    uint8_t        rsvd;
    reg_ds::Handle ds_handle;      // Downstream handle or index identifier to specific resource
    ControlBits    ctrl;           // Control bits (see above)
    DsApiType      api_type;       // Downstream API type-route to specific abstraction layer
    uint16_t       cached_reason;  // Reason code received from latest downstream event
};

// Register table data for reads and writes
struct [[gnu::packed]] Data
{
    ControlBits        ctrl;
    DsApiType          api_type;
    uint16_t           cached_reason;
    DsData             ds_data;  // Data from/to the downstream resource for this entry
    static Data&       from(uint8_t* buf) { return *std::bit_cast<Data*>(buf); }
    static uint8_t*    to(Data& data) { return std::bit_cast<uint8_t*>(&data); }
    static const Data& from(const uint8_t* const buf)
    {
        return *std::bit_cast<const Data* const>(buf);
    }
};

// convenience constructors for control bits initialization
constexpr ControlBits ControlBitsDef = {
    .bits = {.is_ras = 0, .event_enable = 0, .einj_enable = 0, .event_status = 0, .rsvd = 0}
};
constexpr ControlBits ControlBitsRas = {
    .bits = {.is_ras = 1, .event_enable = 0, .einj_enable = 0, .event_status = 0, .rsvd = 0}
};

using TableDefinitions   = std::array<Entry, TableSize>;
using TableDefinitionsIt = TableDefinitions::iterator;

class Table
{
public:
    Table();
    bool read(TableEntry, Data&);
    bool write(TableEntry, const Data&);

private:
    TableDefinitions _tbl;
    inline auto      table_lookup(TableEntry);

protected:
    // Downstream Interfaces
    reg_ds::Info _dsinfo;
    reg_ds::Gpio _dsgpio;
};

}  // namespace nv::reg_table
#else
namespace nv::reg_table {
using Table = common::Empty;
}
#endif

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

#include "nv/ut/unittest.h"

using namespace nv;
using namespace ut;

TEST(RegTable, read)
{
    reg_table::Table table;
    uint8_t    test_buf[] = {0, 0, 0, 0, 0, 0, 0, 0};
    reg_table::Data& test_data  = nv::reg_table::Data::from(&test_buf[0]);

    // good read, identifier is hardcoded to 0x5A
    ensure::is_true(table.read(reg_table::TableEntry::TableIdentifier, test_data));
    for (auto& data : test_data.ds_data) {
        ensure::is_eq(0x5A, data);
    }

    // bad read, testrunner does not have this entry
    ensure::is_false(table.read(reg_table::TableEntry(123), test_data));
};

TEST(RegTable, write)
{
    reg_table::Table table;
    uint8_t    test_write[] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t    test_read[]  = {0, 0, 0, 0, 0, 0, 0, 0};
    reg_table::Data& write_data   = nv::reg_table::Data::from(&test_write[0]);
    reg_table::Data& read_data    = nv::reg_table::Data::from(&test_read[0]);

    // set some config bits to write
    write_data.ctrl.bits.event_enable = 1;
    write_data.ctrl.bits.einj_enable  = 1;
    write_data.cached_reason = static_cast<uint16_t>(reg_table::EventReason::WatchdogReset);

    // write data then read it back for testing
    ensure::is_true(table.write(reg_table::TableEntry::WatchdogResetInt, write_data));
    ensure::is_true(table.read(reg_table::TableEntry::WatchdogResetInt, read_data));

    // set bits to compare
    reg_table::ControlBits test_bits     = {0};
    test_bits.bits.event_enable    = 1;
    test_bits.bits.einj_enable     = 1;
    reg_table::EventReason test_reason = reg_table::EventReason::WatchdogReset;
    ensure::is_eq(test_bits.byte, read_data.ctrl.byte);
    ensure::is_eq(static_cast<uint16_t>(test_reason),
                  static_cast<uint16_t>(read_data.cached_reason));

    // bad write, testrunner does not have this entry
    ensure::is_false(table.write(reg_table::TableEntry(123), write_data));
};

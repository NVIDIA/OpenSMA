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
#include "nv/reg_table/hooks.h"
#include "nv/mctp/vendor.h"

using namespace nv;
using namespace nv::reg_table;

bool Hooks::on_register_table_access(const mctp::Packet& rx, mctp::Packet& tx)
{
    constexpr uint8_t ReadOperation     = 1;
    constexpr uint8_t WriteOperation    = 2;
    constexpr uint8_t RxOperationOffset = 0;
    constexpr uint8_t RxGroupIdOffset   = 1;
    constexpr uint8_t RxEntryIdOffset   = 2;
    constexpr uint8_t RxDataOffset      = 4;
    constexpr uint8_t TxDataOffset      = 0;

    auto& vtx           = mctp::VendorPktRes::from(tx);
    auto& vrx           = mctp::VendorPktReq::from(rx);
    vtx.completion_code = mctp::Ccode::Success;

    // check version and see if access is enabled
    if (vrx.msg_version != 0x01) {
        vtx.completion_code = mctp::Ccode::ErrorUnsupportedCmd;
        return true;
    }

    // get command and ids
    auto operation = vrx.data[RxOperationOffset];
    auto group_id  = vrx.data[RxGroupIdOffset];
    auto entry_id  = vrx.data[RxEntryIdOffset];

    if (entry_id >= TableSize) {
        nv::error("Entry ID is out of range\n");
        vtx.completion_code = mctp::Ccode::ErrorInvalidData;
    }
    else {
        switch (operation) {
            // read one
            case ReadOperation: {
                nv::info("Read Register Table: group %d, entry %d\n", group_id, entry_id);
                auto& txdata = Data::from(&vtx.data[TxDataOffset]);
                if (!table.read(static_cast<TableEntry>(entry_id), txdata)) {
                    nv::error("Read Register Table failed\n");
                    vtx.completion_code = mctp::Ccode::ErrorInvalidData;
                }
                break;
            }
            // write one
            case WriteOperation: {
                nv::info("Write Register Table: group %d, entry %d\n", group_id, entry_id);
                const auto& rxdata = Data::from(&vrx.data[RxDataOffset]);
                if (!table.write(static_cast<TableEntry>(entry_id), rxdata)) {
                    nv::error("Write Register Table failed\n");
                    vtx.completion_code = mctp::Ccode::ErrorInvalidData;
                }
                break;
            }
            default: {
                vtx.completion_code = mctp::Ccode::ErrorUnsupportedCmd;
            }
        }
    }

    tx.priv.packet_length = sizeof(mctp::Header) + mctp::Vendor::HeaderSizeResponse
                          + sizeof(Data);
    return true;
}

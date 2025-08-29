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
#include "nv/logger/log_fault.h"

#include <cstring>

#include "nv/bootloader.h"
#include "nv/flash/driver.h"
#include "nv/logger/common.h"
#include "nv/logger/log.h"
#include "nv/nv.h"
#include "nv/uart/driver.h"

#include "nv/flash/driver.h"
#include "nv/ctimer/ctimer.h"

#include NV_IPC_CONFIG_H

using namespace nv::logger;

bool FaultLogger::fault(Fault fault, const FaultBuffer& data, uint8_t size)
{
    nv::ctimer::Driver::init();

    auto status = nv::flash::Flash::init_on_fault();
    if (status != nv::flash::Status::Ok) {
        // Ignore and still try to log fault
    }

    FaultItem item{};
    item.event   = fault;
    item.version = get_fw_version();

    if (size > 0) {
        memcpy(item.data.data(), data.data(), size);
    }

    uint32_t index = get_fatal_download_size();

    if (index == FaultEntryNum) {
        const nv::flash::Address Address = nv::flash::Flash::get_flash_address(
            FaultEntryStart, nv::bootloader::Driver::current_boot_index());
        auto status = nv::flash::Flash::static_erase(Address);
        if (status != nv::flash::Status::Ok) {
            return false;
        }
        index = 0;
    }

    const nv::flash::Address Address = nv::flash::Flash::get_flash_address(
        FaultEntryStart + index * FaultEntrySize, nv::bootloader::Driver::current_boot_index());
    nv::flash::Flash::write_phrase(Address, item.to_span());

    auto driver = nv::uart::Driver(static_cast<nv::uart::Port>(nv::ipc::UartInstance));
    std::array<uint8_t, sizeof(FaultItem) + 2> dump_buffer{};
    dump_buffer.at(0)  = DumpHeadMagicFault;
    dump_buffer.back() = DumpTailMagicFault;

    memcpy(dump_buffer.data() + 1, item.to_span().data(), sizeof(item));
    driver.tx(dump_buffer);

    return true;
}

uint32_t FaultLogger::get_fatal_download_size()
{
    uint32_t index = 0;
    for (index = 0; index < FaultEntryNum; index++) {
        // nv::flash::Address cur_address = FaultEntryStart + index * FaultEntrySize;
        const nv::flash::Address Address = nv::flash::Flash::get_flash_address(
            FaultEntryStart + index * FaultEntrySize,
            nv::bootloader::Driver::current_boot_index());

        auto erased_status = nv::flash::Flash::check_all_erased(Address, FaultEntrySize);
        // nv::info("check erased on 0x%x erased_status:%d\n", cur_address, erased_status);
        if (erased_status == nv::flash::Status::Ok) {
            break;
        }
    }
    // nv::info("FaultLogger::get_fatal_download_size() index:%d\n", index);
    return index;
}

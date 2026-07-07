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
#include "otp.h"

#include "sys/common/utils.h"
#include "nv/ipc/supervisor.h"
#include "nv/common/build.h"
#include "nv/flash/flash.h"
#include <algorithm>
#include <array>
#include <utility>
namespace sys::flash {
using namespace nv;

namespace {
static nv::flash::Key address_to_pds_key(uint32_t address)
{
    if (address >= 0 && address <= 81) {
        return static_cast<nv::flash::Key>(
            static_cast<uint32_t>(nv::flash::Key::PdsOtpSimultaneousFuseAddress0) + address);
    }
    return nv::flash::Key::PdsInvalid;
}
status_t EFUSE_Read_Sim(uint32_t address, uint32_t& output_data)
{
    const auto PdsKey = address_to_pds_key(address);
    if (PdsKey != nv::flash::Key::PdsInvalid) {
        nv::flash::Data data   = 0;
        auto            status = nv::flash::Flash::get_data_from_kernel(PdsKey, data);
        output_data            = data;
        return (status == nv::flash::Status::Ok) ? kStatus_Success : kStatus_Fail;
    }
    return kStatus_Fail;
}

status_t EFUSE_Program_Sim(uint32_t address, uint32_t input_data)
{
    const auto PdsKey = address_to_pds_key(address);
    if (PdsKey != nv::flash::Key::PdsInvalid) {
        auto status = nv::flash::Flash::set_data_from_kernel(PdsKey, input_data);
        return (status == nv::flash::Status::Ok) ? kStatus_Success : kStatus_Fail;
    }
    return kStatus_Fail;
}

}  // namespace

status_t OtpDriver::read(uint32_t address, uint32_t& output_data)
{
    uint32_t otp_data = 0;
    status_t ret_code = kStatus_Success;
    // should not been called in dev mode, just return success
    if constexpr (nv::common::build::Mode == nv::common::build::Modes::Dev) {
        ret_code = EFUSE_Read_Sim(address, otp_data);
    }
    else {
        ret_code = EFUSE_Read(address, &otp_data);
    }
    output_data = otp_data;
    return ret_code;
};

status_t OtpDriver::program(const uint32_t addr, const uint32_t input_data)
{
    // check if the command is for locking fuse
    constexpr uint32_t LockFuseAddress = 0x0;
    if (addr == LockFuseAddress) {
        // check life cycle status
        LifeCycleStatus life_cycle_status{};
        if (auto read_sts = read_life_cycle(life_cycle_status); read_sts != kStatus_Success) {
            return read_sts;
        }
        // if life cycle status is not in field, return error
        if (life_cycle_status == LifeCycleStatus::Blank
            or life_cycle_status == LifeCycleStatus::Fab
            or life_cycle_status == LifeCycleStatus::Develop
            or life_cycle_status == LifeCycleStatus::Develop2) {
            return kStatus_OutOfRange;
        }
        // life cycle check passed - allow programming
        if constexpr (nv::common::build::Mode != nv::common::build::Modes::Dev) {
            return EFUSE_Program(addr, input_data);
        }
        else {
            return EFUSE_Program_Sim(addr, input_data);
        }
    }

    // check if the address is valid
#ifdef EDGELOCK2GO_CERT
    constexpr std::array<uint32_t, 29> UsedEfuseAddr = {31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
                                                        41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
                                                        63, 64, 65, 66, 67, 68, 69, 70, 71};
#else
    constexpr std::array<uint32_t, 25> UsedEfuseAddr = {31, 32, 33, 34, 35, 36, 37, 38, 39,
                                                        40, 41, 42, 43, 44, 45, 46, 47, 48,
                                                        49, 50, 63, 64, 65, 66, 67};
#endif
    static_assert(std::is_sorted(UsedEfuseAddr.begin(), UsedEfuseAddr.end()),
                  "UsedEfuseAddr is not sorted");
    // perform a binary search to check if the address is valid
    if (!std::binary_search(UsedEfuseAddr.begin(), UsedEfuseAddr.end(), addr)) {
        return kStatus_OutOfRange;
    }

    // should not been called in dev mode, just return success
    if constexpr (nv::common::build::Mode == nv::common::build::Modes::Dev) {
        return EFUSE_Program_Sim(addr, input_data);
    }
    else {
        return EFUSE_Program(addr, input_data);
    }
};

status_t OtpDriver::init()
{
    const status_t RetCode = EFUSE_Init();
    // init life cycle status
    LifeCycleStatus life_cycle_status{};
    if (auto read_sts = this->read_life_cycle(life_cycle_status); read_sts != kStatus_Success) {
        return read_sts;
    }
    return RetCode;
};
EfuseDeviceVersion OtpDriver::get_version()
{
    const EfuseDriverT& otp_api_interface = *std::bit_cast<EfuseDriverT*>(OtpApiAddr);
    return otp_api_interface.version;
};

status_t OtpDriver::read_crc(uint32_t& data)
{
    constexpr uint32_t CRC_IDX       = 20;
    constexpr uint32_t timeout_ticks = 5;

    const uint32_t start_ticks = nv::ipc::Supervisor::get_os_ticks();

    // coverity[+ignore]
    OTPC0->RWC = OTPC_RWC_READ_EFUSE_MASK | CRC_IDX;
    nv::info("OTPC0 value: 0x%x", OTPC0);

    // Poll with timeout using OS ticks
    while (true) {
        const uint32_t current_ticks = nv::ipc::Supervisor::get_os_ticks();
        const uint32_t elapsed_ticks = current_ticks - start_ticks;

        if ((OTPC0->SR & OTPC_SR_BUSY_MASK) == false) {  // register is ready
            break;
        }

        if (elapsed_ticks > timeout_ticks) {
            return kStatus_Timeout;
        }
    }

    data = OTPC0->RDATA;
    // coverity[-ignore]

    return kStatus_Success;
}

status_t OtpDriver::read_life_cycle(LifeCycleStatus& data)
{
    if (life_cycle_status) {
        data = *life_cycle_status;
        return kStatus_Success;
    }

    constexpr uint32_t LifeCycleEfuseAddress = 0x1;
    uint32_t           otp_data              = 0;
    const status_t     RetCode               = EFUSE_Read(LifeCycleEfuseAddress, &otp_data);
    if (RetCode != kStatus_Success) {
        life_cycle_status = std::nullopt;
        return RetCode;
    }

    constexpr uint32_t LifeCycleEfuseMask = 0x000000ff;

    const auto LifeCycleStatusValue = static_cast<std::underlying_type_t<LifeCycleStatus>>(
        otp_data & LifeCycleEfuseMask);

    if (LifeCycleStatusValue == std::to_underlying(LifeCycleStatus::Blank)
        or LifeCycleStatusValue == std::to_underlying(LifeCycleStatus::Fab)
        or LifeCycleStatusValue == std::to_underlying(LifeCycleStatus::Develop)
        or LifeCycleStatusValue == std::to_underlying(LifeCycleStatus::Develop2)
        or LifeCycleStatusValue == std::to_underlying(LifeCycleStatus::InField)
        or LifeCycleStatusValue == std::to_underlying(LifeCycleStatus::InFieldLock)
        or LifeCycleStatusValue == std::to_underlying(LifeCycleStatus::FieldReturnOEM)
        or LifeCycleStatusValue == std::to_underlying(LifeCycleStatus::FailureAnalysis)
        or LifeCycleStatusValue == std::to_underlying(LifeCycleStatus::Bricked)) {
        life_cycle_status = static_cast<LifeCycleStatus>(LifeCycleStatusValue);
        data              = *life_cycle_status;
        return kStatus_Success;
    }
    else {
        life_cycle_status = std::nullopt;
        return kStatus_Fail;
    }
}

}  // namespace sys::flash
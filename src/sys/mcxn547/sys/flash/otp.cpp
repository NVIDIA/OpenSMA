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
#include <algorithm>
#include <utility>
namespace sys::flash {
using namespace nv;

status_t OtpDriver::read(uint32_t address, uint32_t& output_data)
{
    uint32_t       otp_data = 0;
    const status_t RetCode  = EFUSE_Read(address, &otp_data);
    output_data             = otp_data;
    return RetCode;
};

status_t OtpDriver::program(const uint32_t addr, const uint32_t input_data)
{
    const status_t RetCode = EFUSE_Program(addr, input_data);

    return RetCode;
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
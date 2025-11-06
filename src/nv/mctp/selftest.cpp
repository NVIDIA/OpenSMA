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
#include "nv/mctp/selftest.h"

#include <bitset>
#include <cstring>
#include <span>

#include "sys/common/utils.h"
#include "nv/flash/flash.h"
#include "nv/common/utils.h"
#include "nv/ipc/supervisor.h"
#include "nv/perf_mon/perf_mon.h"
#include "mpu_syscall_numbers.h"

using namespace nv::mctp;

// privileged functions
#if defined(__cplusplus)
extern "C" {
#endif

NV_PRIVILEGED_FUNCTION void
SelfTest_GetTestingFwResult_Priv(nv::mctp::SelfTest::TestingFwResultStruct& testing_result)
{
    nv::mctp::SelfTest::get_testing_fw_result_impl(testing_result);
}

NV_PRIVILEGED_FUNCTION SelfTestStatus
SelfTest_GetTRNGConfig_Priv(const std::span<uint8_t>& buffer)
{
    return nv::mctp::SelfTest::get_trng_config_impl(buffer);
}

#if defined(__cplusplus)
}
#endif

// Then modify the public methods to use SVC
void SelfTest::get_testing_fw_result(TestingFwResultStruct& testing_result)
{
    return get_testing_fw_result_svc(testing_result);
}

NV_SYS_CALL void SelfTest::get_testing_fw_result_svc(TestingFwResultStruct& testing_result)
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern SelfTest_GetTestingFwResult_Priv              \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_SelfTest_GetTestingFwResult            \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_SelfTest_GetTestingFwResult          \n"
        " Privileged_SelfTest_GetTestingFwResult:               \n"
        "     pop {r0}                                          \n"
        "     b SelfTest_GetTestingFwResult_Priv                \n"
        " Unprivileged_SelfTest_GetTestingFwResult:             \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_SelfTest_GetTestingFwResult)
        : "memory");
#else
    return SelfTest_GetTestingFwResult_Priv(testing_result);
#endif
    // coverity[cert_msc52_cpp_violation] - Expect no return
}

NV_PRIVILEGED_FUNCTION void
SelfTest::get_testing_fw_result_impl(TestingFwResultStruct& testing_result)
{
    if (!nv::ipc::Supervisor::inst()
             .task(nv::ipc::TaskId::Mctp)
             .checking_parameter_is_from_self_stack(testing_result)) {
        return;
    }

    constexpr size_t RedundantAddressForCovetity = 0x0;
    // start to read from ram
    constexpr uint32_t TestingResultRamAddr = 0x20002300;

    auto buffer = std::span<uint8_t>(
        *std::bit_cast<std::array<uint8_t, sizeof(decltype(testing_result))>*>(
            &testing_result));

    std::memcpy(buffer.data(),
                std::bit_cast<std::array<uint8_t, 1>*>(RedundantAddressForCovetity
                                                       + TestingResultRamAddr)
                    ->data(),
                buffer.size());
    return;
}

uint32_t SelfTest::get_length(uint32_t tests)
{
    uint32_t total_length = 0;

    for (const unsigned char I : NeedLength) {
        if (tests & 1U) {
            // overflow check
            if ((UINT32_MAX - total_length) >= I) {
                total_length += I;
            }
        }

        tests >>= 1U;
    }

    return total_length;
}

SelfTestStatus SelfTest::populate_result(uint32_t tests, const std::span<uint8_t>& buffer)
{
    SelfTestStatus                                             status = SelfTestStatus::Success;
    const std::bitset<static_cast<uint32_t>(SelfTestCmd::Max)> TempTests(tests);
    uint8_t                                                    offset = 0;

    for (uint32_t i = 0; i < static_cast<uint32_t>(SelfTestCmd::Max); ++i) {
        if (TempTests[i]) {
            const auto Cmd = static_cast<SelfTestCmd>(i);

            // coverity[unreachable]
            switch (Cmd) {
                case SelfTestCmd::FwVersion:
                    status = fw_version(buffer.subspan(
                        offset, NeedLength[static_cast<uint32_t>(SelfTestCmd::FwVersion)]));
                    if (status != SelfTestStatus::Success) {
                        return status;
                    }
                    offset += NeedLength[static_cast<uint32_t>(SelfTestCmd::FwVersion)];
                    break;
                case SelfTestCmd::TestingFwResult:
                    status = testing_fw_result(buffer.subspan(
                        offset, NeedLength[static_cast<size_t>(SelfTestCmd::TestingFwResult)]));
                    if (status != SelfTestStatus::Success) {
                        return status;
                    }
                    offset += NeedLength[static_cast<uint32_t>(SelfTestCmd::TestingFwResult)];
                    break;
                case SelfTestCmd::TestingPerf:
                    status = test_perf(buffer.subspan(
                        offset, NeedLength[static_cast<uint32_t>(SelfTestCmd::TestingPerf)]));
                    if (status != SelfTestStatus::Success) {
                        return status;
                    }
                    offset += NeedLength[static_cast<uint32_t>(SelfTestCmd::TestingPerf)];
                    break;
                case SelfTestCmd::GetTRNGConfig:
                    status = get_trng_config(buffer.subspan(
                        offset, NeedLength[static_cast<uint32_t>(SelfTestCmd::GetTRNGConfig)]));
                    if (status != SelfTestStatus::Success) {
                        return status;
                    }
                    offset += NeedLength[static_cast<uint32_t>(SelfTestCmd::GetTRNGConfig)];
                    break;
                case SelfTestCmd::GetCMACValue:
                    status = get_crc_value(buffer.subspan(
                        offset, NeedLength[common::to_underlying(SelfTestCmd::GetCMACValue)]));
                    if (status != SelfTestStatus::Success) {
                        return status;
                    }
                    offset += NeedLength[common::to_underlying(SelfTestCmd::GetCMACValue)];
                    break;
                case SelfTestCmd::StackUsage:
                    if constexpr (WantStackUsage) {
                        status  = stack_usage(buffer.subspan(
                            offset,
                            NeedLength[common::to_underlying(SelfTestCmd::StackUsage)]));
                        offset += NeedLength[common::to_underlying(SelfTestCmd::StackUsage)];
                        break;
                    }
                default: break;
            }
        }
    }

    return status;
}

SelfTestStatus SelfTest::testing_fw_result(const std::span<uint8_t>& buffer)
{
    if (buffer.size() < NeedLength[static_cast<uint32_t>(SelfTestCmd::TestingFwResult)]) {
        return SelfTestStatus::ErrorBufferTooSmall;
    }

    // for coverity, since this variable could not be const
    TestingFwResultStruct testing_result;
    testing_result = TestingFwResultStruct{};

    get_testing_fw_result(testing_result);

    constexpr uint8_t ExpectedTestingStatusStart = 0xbb;
    constexpr uint8_t ExpectedTestingStatusEnd   = 0xee;
    constexpr uint8_t ExpectedTestingFwBootSrc   = 0;
    constexpr uint8_t ExpectedTestingId          = 0;
    constexpr auto    ExpectedNvokMagicNumber    = decltype(testing_result.nvok_magic_number){
        'N', 'V', 'O', 'K'};

    // thsis command will not fail
    constexpr auto RetStatus = SelfTestStatus::Success;

    // check the if the testing have run
    if (!(testing_result.testing_fw_boot_src == ExpectedTestingFwBootSrc
          && testing_result.testing_id == ExpectedTestingId
          && testing_result.nvok_magic_number == ExpectedNvokMagicNumber)) {
        buffer[0] = std::underlying_type_t<FwTestingResultResponseCode>(
            FwTestingResultResponseCode::NoTesting);
    }
    else if (testing_result.testing_status == ExpectedTestingStatusStart) {
        buffer[0] = std::underlying_type_t<FwTestingResultResponseCode>(
            FwTestingResultResponseCode::TestingHaveConduct);
    }
    else if (testing_result.testing_status == ExpectedTestingStatusEnd) {
        buffer[0] = testing_result.testing_actually_run != testing_result.testing_expected_run
                      ? std::underlying_type_t<FwTestingResultResponseCode>(
                            FwTestingResultResponseCode::TestingFail)
                      : std::underlying_type_t<FwTestingResultResponseCode>(
                            FwTestingResultResponseCode::TestingSuccess);
    }

    return RetStatus;
}

NV_SYS_CALL SelfTestStatus SelfTest::get_trng_config_svc(const std::span<uint8_t>& buffer)
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern SelfTest_GetTRNGConfig_Priv                   \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_SelfTest_GetTRNGConfig                 \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_SelfTest_GetTRNGConfig               \n"
        " Privileged_SelfTest_GetTRNGConfig:                    \n"
        "     pop {r0}                                          \n"
        "     b SelfTest_GetTRNGConfig_Priv                     \n"
        " Unprivileged_SelfTest_GetTRNGConfig:                  \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_SelfTest_GetTRNGConfig)
        : "memory");
#else
    return SelfTest_GetTRNGConfig_Priv(buffer);
#endif
    // coverity[cert_msc52_cpp_violation] - Expect no return
}

NV_PRIVILEGED_FUNCTION SelfTestStatus
SelfTest::get_trng_config_impl(const std::span<uint8_t>& buffer)
{
    if (!nv::ipc::Supervisor::inst()
             .task(nv::ipc::TaskId::Mctp)
             .checking_parameter_is_from_self_stack(buffer)) {
        return SelfTestStatus::Error;
    }

    if (buffer.size() < NeedLength[static_cast<uint32_t>(SelfTestCmd::GetTRNGConfig)]) {
        return SelfTestStatus::ErrorBufferTooSmall;
    }

    // read TRNG config
    constexpr uintptr_t TRNGConfigAddr = 0x01100050;
    const auto          trng_config = *std::bit_cast<std::array<uint8_t, 4>*>(TRNGConfigAddr);
    std::memcpy(buffer.data(), &trng_config, sizeof(trng_config));

    return SelfTestStatus::Success;
}

SelfTestStatus SelfTest::get_crc_value(const std::span<uint8_t>& buffer)
{
    if (buffer.size() < NeedLength[common::to_underlying(SelfTestCmd::GetCMACValue)]) {
        return SelfTestStatus::ErrorBufferTooSmall;
    }

    uint32_t CRC_data = 0;

    auto flash_status = nv::flash::Flash::read_crc(CRC_data);
    if (flash_status != nv::flash::Status::Ok) {
        return SelfTestStatus::Error;
    }

    std::memcpy(buffer.data(), &CRC_data, sizeof(CRC_data));

    return SelfTestStatus::Success;
}

SelfTestStatus SelfTest::get_trng_config(const std::span<uint8_t>& buffer)
{
    return get_trng_config_svc(buffer);
}

SelfTestStatus SelfTest::fw_version(const std::span<uint8_t>& buffer)
{
    if (buffer.size() < NeedLength[static_cast<uint32_t>(SelfTestCmd::FwVersion)]) {
        return SelfTestStatus::ErrorBufferTooSmall;
    }

    const uint16_t Major = MCU_FW_MAJOR;
    const uint16_t Patch = MCU_FW_PATCH;
    const uint16_t Build = MCU_FW_BUILD;
    buffer[0]            = Major >> 8U;
    buffer[1]            = Major & UINT8_MAX;
    buffer[2]            = MCU_FW_MINOR;
    buffer[3]            = Patch >> 8U;
    buffer[4]            = Patch & UINT8_MAX;
    buffer[5]            = Build >> 8U;
    buffer[6]            = Build & UINT8_MAX;

    return SelfTestStatus::Success;
}

SelfTestStatus SelfTest::stack_usage(const std::span<uint8_t>& buffer)
{
    if constexpr (WantStackUsage) {
        if (buffer.size() < NeedLength[common::to_underlying(SelfTestCmd::StackUsage)]) {
            return SelfTestStatus::ErrorBufferTooSmall;
        }

        uint16_t stack    = 0;
        uint32_t offset   = 0;
        auto     total_id = static_cast<uint8_t>(nv::ipc::TaskId::KernelEnd);

        for (auto id = static_cast<uint8_t>(nv::ipc::TaskId::Begin); id < total_id;
             id      = id + 1) {
            auto& task = nv::ipc::Supervisor::inst().task_pub(static_cast<nv::ipc::TaskId>(id));
            if (nullptr == task.handle()) {
                continue;
            }
            auto stack_tmp = uxTaskGetStackHighWaterMark(task.handle());

            // Check bounds before writing id and stack
            if (common::add(offset, static_cast<uint32_t>(1 + sizeof(stack))) > buffer.size()) {
                return SelfTestStatus::ErrorBufferTooSmall;
            }

            // set id
            buffer[offset] = static_cast<uint8_t>(id);
            offset         = common::add(offset, static_cast<uint32_t>(1));
            // set stack
            stack = (stack_tmp < UINT16_MAX) ? stack_tmp : UINT16_MAX;
            memcpy(&buffer[offset], &stack, sizeof(stack));
            offset = common::add(offset, static_cast<uint32_t>(sizeof(stack)));
        }

        return SelfTestStatus::Success;
    }
    return SelfTestStatus::Error;
}

SelfTestStatus SelfTest::test_perf(const std::span<uint8_t>& buffer)
{
    if (buffer.size() < NeedLength[static_cast<uint32_t>(SelfTestCmd::TestingPerf)]) {
        return SelfTestStatus::ErrorBufferTooSmall;
    }

    uint32_t usb_rx_to_i3c_tx_latency = 0;
    uint32_t i3c_rx_to_usb_tx_latency = 0;
    perf_mon::Driver::get_pkt_latency(usb_rx_to_i3c_tx_latency, i3c_rx_to_usb_tx_latency);
    memcpy(buffer.data(), &usb_rx_to_i3c_tx_latency, sizeof(usb_rx_to_i3c_tx_latency));
    memcpy(buffer.data() + sizeof(usb_rx_to_i3c_tx_latency),
           &i3c_rx_to_usb_tx_latency,
           sizeof(i3c_rx_to_usb_tx_latency));

    return SelfTestStatus::Success;
}
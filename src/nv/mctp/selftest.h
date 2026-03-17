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
#include "nv/common/build.h"
#include "nv/ipc/supervisor.h"
#include "nv/mctp/driver.h"
#include "nv/nv.h"

namespace nv::mctp {

enum class SelfTestStatus : uint8_t
{
    Success               = 0x01,
    ErrorOtp              = 0x02,
    Error                 = 0x03,
    ErrorSpiAccess        = 0x04,
    ErrorBgworkerResquest = 0x05,
    ErrorInProgress       = 0x06,
    ErrorNoResult         = 0x07,
    ErrorBufferTooSmall   = 0x08,
};

enum class SelfTestCmd : uint32_t
{
    FwVersion             = 0,
    TestingFwResult       = 1,
    StackUsage            = 2,
    TestingPerf           = 3,
    GetTRNGConfig         = 4,
    GetCMACValue          = 5,
    I2cLoopbackTest       = 6,
    I2cLoopbackTestResult = 7,
    Max,
};

enum class FwTestingResultResponseCode : uint8_t
{
    TestingSuccess     = 0x00,
    TestingFail        = 0x01,
    TestingHaveConduct = 0x02,
    NoTesting          = 0x7f,
};

class SelfTest
{
public:
    // buf_size (2048) - private hdr (4) - mctp hdr (4) - vendor paylod (12)
    // = 2028
    constexpr static uint32_t MaxResponseLength = nv::mctp::Constants::MctpTxBufSize
                                                - sizeof(nv::mctp::PrivateHeader)
                                                - sizeof(nv::mctp::Header) - 12;

    static uint32_t       get_length(uint32_t tests);
    static SelfTestStatus populate_result(uint32_t tests, const std::span<uint8_t>& buffer);

    /// return supported commands as bit mask
    consteval static auto supported_cmds()
    {
        using enum SelfTestCmd;

        auto v  = common::bit(FwVersion);
        v      |= common::bit(TestingFwResult);
        v      |= common::bit(GetTRNGConfig);
        v      |= common::bit(GetCMACValue);
        v      |= common::bit(I2cLoopbackTest);
        v      |= common::bit(I2cLoopbackTestResult);

        if constexpr (WantStackUsage) {
            v |= common::bit(StackUsage);
        }
        v |= common::bit(TestingPerf);
        return v;
    }
    typedef struct [[gnu::packed]]
    {
        uint8_t  testing_id;
        uint8_t  testing_fw_boot_src;  // where the testing fw boot, ram: 0, internal flash: 1
        uint8_t  testing_status;       // should be 0xbb when start 0xee
        uint32_t testing_actually_run;
        uint32_t testing_expected_run;

        std::array<uint8_t, 4> nvok_magic_number;  // should be "KOVN"
    } TestingFwResultStruct;

    typedef struct [[gnu::packed]]
    {
        uint8_t                 IsRunning;
        std::array<uint8_t, 10> flexcomm_result;
    } I2cLoopbackTestResultStruct;

    consteval static bool validate_stack_usage_buffer_size()
    {
        // id (1) + stack (2) = 3
        const auto required_size  = static_cast<uint8_t>(nv::ipc::TaskId::KernelEnd) * 3;
        const auto available_size = NeedLength[common::to_underlying(SelfTestCmd::StackUsage)];
        return required_size <= available_size;
    }
    static void           get_testing_fw_result_impl(TestingFwResultStruct& testing_result);
    static SelfTestStatus get_trng_config_impl(const std::span<uint8_t>& buffer);

protected:
    /// enable stack usage on dev and dbg builds
    constexpr static inline auto WantStackUsage = common::build::Mode
                                               >= common::build::Modes::Dev;

    constexpr static inline uint8_t NeedLength[common::to_underlying(SelfTestCmd::Max)] = {
        7,                                                            // FwVersion
        sizeof(std::underlying_type_t<FwTestingResultResponseCode>),  // TestingFwResult
        96,                                                           // StackUsage
        8,                                                            // TestingPerf
        sizeof(uint32_t),                                             // TRNGConfig
        sizeof(uint32_t),                                             // CMACValue
        0,                                                            // I2cLoopbackTest
        sizeof(I2cLoopbackTestResultStruct)                           // I2cLoopbackTestResult
    };

    static SelfTestStatus fw_version(const std::span<uint8_t>& buffer);
    static SelfTestStatus testing_fw_result(const std::span<uint8_t>& buffer);
    static SelfTestStatus get_trng_config(const std::span<uint8_t>& buffer);
    static SelfTestStatus get_crc_value(const std::span<uint8_t>& buffer);
    static SelfTestStatus stack_usage(const std::span<uint8_t>& buffer);
    static SelfTestStatus test_perf(const std::span<uint8_t>& buffer);
    static void           get_testing_fw_result(TestingFwResultStruct& testing_result);
    static void           get_testing_fw_result_svc(TestingFwResultStruct& testing_result);
    static SelfTestStatus get_trng_config_svc(const std::span<uint8_t>& buffer);
    static SelfTestStatus i2c_loopback_test(const std::span<uint8_t>& buffer);
    static SelfTestStatus i2c_loopback_test_result(const std::span<uint8_t>& buffer);
};

static_assert(
    nv::mctp::SelfTest::validate_stack_usage_buffer_size(),
    "Stack usage buffer size is too small to hold all task data (need 3 bytes per task)");

}  // namespace nv::mctp

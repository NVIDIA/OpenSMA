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
#include <cstdint>
#include <span>
#include <cstring>
#include "nv/mctp/interface.h"

using namespace nv::mctp;

namespace nv::coverage {
// Structure to represent the state of a download coverage session
struct DownloadCoverageSession
{
    enum class CoveragePhase : uint8_t
    {
        FileName        = 0x00,
        GcovInfo        = 0x01,
        GcovFnInfo      = 0x02,
        GcovCounterInfo = 0x03,
        GcovCtrInfo     = 0x04,
        Invalid         = 0x05,
        Finish          = 0xFF,
    };
    CoveragePhase coverage_phase;
    uint8_t       gcovinfo_index;
    uint8_t       file_name_index;
    uint16_t      gcovfninfo_index;
    uint8_t       gcovcounter_index;
    uint16_t      gcovctrinfo_index;

    DownloadCoverageSession() = default;
    DownloadCoverageSession(const uint8_t* data)
    {
        uint8_t phase_value = data[0];
        // tool will stop sending back session info when it receive Finish phase value
        // so we can use Invalid to check if the phase value is valid
        if (phase_value >= static_cast<uint8_t>(CoveragePhase::Invalid)) {
            coverage_phase = CoveragePhase::Invalid;
        }
        coverage_phase    = static_cast<CoveragePhase>(phase_value);
        gcovinfo_index    = data[1];
        file_name_index   = data[2];
        gcovfninfo_index  = ((data[3] << 8) | data[4]) & 0XFFFF;
        gcovcounter_index = data[5];
        gcovctrinfo_index = ((data[6] << 8) | data[7]) & 0XFFFF;
    }
    void to_data(uint8_t* data) const
    {
        data[0] = static_cast<uint8_t>(coverage_phase);
        data[1] = gcovinfo_index;
        data[2] = file_name_index;
        data[3] = gcovfninfo_index >> 8;
        data[4] = gcovfninfo_index & 0xFF;
        data[5] = gcovcounter_index;
        data[6] = gcovctrinfo_index >> 8;
        data[7] = gcovctrinfo_index & 0xFF;
    }
};

class Coverage
{
public:
    // place the coverage data in tx.data and return the length of the data to be sent
    // additional to the MCTP header and Vendor header
    static uint8_t download(const Packet& rx, Packet& tx);
};
}  // namespace nv::coverage
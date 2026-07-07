/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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

#include "nv/volt_mon/leak_detect_pds_seed.h"

#include <cstdint>

#include "nv/flash/datastore.h"
#include "nv/flash/flash.h"
#include "nv/logger/common.h"
#include "nv/logger/log.h"
#include "nv/volt_mon/common.h"
#include "nv/volt_mon/leak_detect.h"

#include NV_IPC_CONFIG_H

namespace nv::leak_detect {

namespace {

// LeakDetectWarPdsUpdate event payload kinds (see nv/logger/common.h).
constexpr uint8_t WarKindPdsGetFail = 2;
constexpr uint8_t WarKindPdsSetFail = 3;

// Emit a structured flash-only log entry for one WAR failure.
// data layout: [kind, slot, subCall, 0, status_le_u32].
void log_war_failure(uint8_t kind, uint8_t slot, uint8_t subCall, nv::flash::Status status)
{
    const auto                  s = static_cast<uint32_t>(status);
    const nv::logger::EventData data{kind,
                                     slot,
                                     subCall,
                                     0,
                                     static_cast<uint8_t>(s & 0xFFU),
                                     static_cast<uint8_t>((s >> 8) & 0xFFU),
                                     static_cast<uint8_t>((s >> 16) & 0xFFU),
                                     static_cast<uint8_t>((s >> 24) & 0xFFU)};
    nv::logger::error_no_wait(
        nv::logger::Event::LeakDetectWarPdsUpdate, data, nv::logger::OutputDirection::Flash);
}

}  // namespace

/*
 * PDS slot layout mirrors leak_detect.cpp pds_key():
 *   Word 0: [31:16] sensorId  [15:0] minLeak
 *   Word 1: [31:16] maxLeak   [15:0] maxNormal
 */
void war_dieid_based_pds_update(bool isMCURevA2OrLater)
{
    using namespace nv::ipc::voltage_monitor_config;

    if (!isMCURevA2OrLater) {
        return;
    }

    constexpr auto MinLeakAdc   = to_adc_value(DefaultMinLeak);
    constexpr auto MaxLeakAdc   = to_adc_value(DefaultMaxLeak);
    constexpr auto MaxNormalAdc = to_adc_value(DefaultMaxNormal);

    const nv::flash::Data DefaultPack1 = (static_cast<uint32_t>(MaxLeakAdc) << 16)
                                       | static_cast<uint32_t>(MaxNormalAdc);

    for (uint8_t slot = 0; slot < LeakDetectSensorNum; ++slot) {
        const auto baseKey = static_cast<uint32_t>(nv::flash::Key::PdsLeakDetSlot0Part0)
                           + slot * nv::leak_detect::PdsLeakDetEntriesPerSlot;
        const auto key0 = static_cast<nv::flash::Key>(baseKey);
        const auto key1 = static_cast<nv::flash::Key>(baseKey + 1U);

        nv::flash::Data pack0    = 0;
        nv::flash::Data pack1    = 0;
        const auto      getStat0 = nv::flash::Flash::get_data_from_kernel(key0, pack0);
        const auto      getStat1 = nv::flash::Flash::get_data_from_kernel(key1, pack1);
        if (getStat0 != nv::flash::Status::Ok) {
            log_war_failure(WarKindPdsGetFail, slot, 0, getStat0);
        }
        if (getStat1 != nv::flash::Status::Ok) {
            log_war_failure(WarKindPdsGetFail, slot, 1, getStat1);
        }
        if (getStat0 != nv::flash::Status::Ok || getStat1 != nv::flash::Status::Ok) {
            continue;
        }
        if (pack0 != 0 && pack1 != 0) {
            continue;
        }

        // Loop bound (slot < LeakDetectSensorNum) matches the array size by construction.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        const auto            sensorId  = static_cast<uint8_t>(LeakDetectSensorId[slot]);
        const nv::flash::Data SeedPack0 = (static_cast<uint32_t>(sensorId) << 16)
                                        | static_cast<uint32_t>(MinLeakAdc);
        const auto setStat0 = nv::flash::Flash::set_data_from_kernel(key0, SeedPack0);
        const auto setStat1 = nv::flash::Flash::set_data_from_kernel(key1, DefaultPack1);
        if (setStat0 != nv::flash::Status::Ok) {
            log_war_failure(WarKindPdsSetFail, slot, 0, setStat0);
        }
        if (setStat1 != nv::flash::Status::Ok) {
            log_war_failure(WarKindPdsSetFail, slot, 1, setStat1);
        }
    }
}

}  // namespace nv::leak_detect

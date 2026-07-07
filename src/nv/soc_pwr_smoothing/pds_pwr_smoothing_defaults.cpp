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

/**
 * @file pds_pwr_smoothing_defaults.cpp
 * @brief Strong PDS factory defaults for SoC policy keys (from project config.h).
 *
 * Linked with the soc_pwr_smoothing module. Overrides weak stubs in the flash module.
 * Values track Default*PolicyEnabled in the project NV_IPC_CONFIG_H.
 */

#include NV_IPC_CONFIG_H
#include "nv/flash/pds_pwr_smoothing_defaults.h"

#include <cstdint>

namespace {

constexpr uint32_t policy_enabled(bool enabled)
{
    return enabled ? 1U : 0U;
}

}  // namespace

extern "C" uint32_t nv_pds_default_soc_offset_policy_enabled()
{
    return policy_enabled(nv::soc_pwr_smoothing::DefaultOffsetPolicyEnabled);
}

extern "C" uint32_t nv_pds_default_soc_power_brake_enabled()
{
    return policy_enabled(nv::soc_pwr_smoothing::DefaultPowerBrakePolicyEnabled);
}

extern "C" uint32_t nv_pds_default_soc_therm_brake_enabled()
{
    return policy_enabled(nv::soc_pwr_smoothing::DefaultThermBrakePolicyEnabled);
}

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
 * @file pds_pwr_smoothing_defaults_weak.cpp
 * @brief Weak factory defaults for SoC power-smoothing PDS keys.
 *
 * Linked via the flash module alongside pds.cpp. pds.cpp calls these hooks when
 * seeding PDS on first boot or reset.
 *
 * When soc_pwr_smoothing is linked, pds_pwr_smoothing_defaults.cpp provides
 * strong definitions from project config.h. API declarations are in
 * nv/flash/pds_pwr_smoothing_defaults.h.
 */

#include "nv/flash/pds_pwr_smoothing_defaults.h"

extern "C" __attribute__((weak)) uint32_t nv_pds_default_soc_offset_policy_enabled()
{
    return 0U;
}

extern "C" __attribute__((weak)) uint32_t nv_pds_default_soc_power_brake_enabled()
{
    return 0U;
}

extern "C" __attribute__((weak)) uint32_t nv_pds_default_soc_therm_brake_enabled()
{
    return 0U;
}

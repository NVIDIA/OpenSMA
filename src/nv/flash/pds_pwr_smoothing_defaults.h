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
 * @file pds_pwr_smoothing_defaults.h
 * @brief Per-project factory defaults for SoC power-smoothing PDS keys.
 *
 * Weak implementations live in pds_pwr_smoothing_defaults_weak.cpp (flash module).
 * Strong definitions live in nv/soc_pwr_smoothing/pds_pwr_smoothing_defaults.cpp
 * when the soc_pwr_smoothing module is linked. pds.cpp consumes these hooks when
 * seeding PDS on first boot or reset.
 */

#pragma once

#include <cstdint>

extern "C" uint32_t nv_pds_default_soc_offset_policy_enabled();
extern "C" uint32_t nv_pds_default_soc_power_brake_enabled();
extern "C" uint32_t nv_pds_default_soc_therm_brake_enabled();

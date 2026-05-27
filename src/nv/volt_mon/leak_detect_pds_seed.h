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
 * @file leak_detect_pds_seed.h
 * @brief One-shot boot helper that seeds leak-detect PDS slots with operational
 *        defaults on target MCUs.
 *
 * Projects whose leak-detect sensor[] table in config.h holds full-range (0/0/
 * AdcFullScale) defaults call this helper from main() before scheduler start.
 * A2 or later target MCUs pass true to write DefaultMinLeak / DefaultMaxLeak /
 * DefaultMaxNormal to any empty PDS slot so leak detection comes up with real
 * thresholds; older MCUs pass false and keep the full-range defaults.
 */

#pragma once

namespace nv::leak_detect {

/**
 * @brief Seed leak-detect PDS slots with operational defaults on target boards.
 *
 * If isMCURevA2OrLater is true, writes {LeakDetectSensorId[slot], DefaultMinLeak,
 * DefaultMaxLeak, DefaultMaxNormal} to each leak-detect sensor slot whose PDS pair is empty
 * (pack0 == 0 && pack1 == 0). Slots already populated (e.g. NSM-tuned values) are left
 * untouched. Caller owns the target-specific DIEID/A2+ decision.
 *
 * MUST be called after flash::Task::make() (which initializes _pds) and before
 * volt_mon::init() (which calls LeakDetect::init() -> load_pds_thresholds()).
 *
 * Pre-scheduler context: uses Flash::get_data_from_kernel /
 * set_data_from_kernel (kernel bypass).
 */
void war_dieid_based_pds_update(bool isMCURevA2OrLater);

}  // namespace nv::leak_detect

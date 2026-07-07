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
#pragma once

#include <cstdint>

#include "nv/flash/common.h"
#include "nv/vrot/interface/types.h"

namespace nv::vrot {

// Raw bank mirror between two slots. Caller supplies source and destination;
// storage layout is opaque and resolved by platform Ops.
namespace ap_background_copy {

constexpr uint8_t Slot0 = 0;
constexpr uint8_t Slot1 = 1;

bool supports(const ApInfo& ap);

nv::flash::Status start(const ApInfo& ap, uint8_t source_slot, uint8_t dest_slot);
nv::flash::Status service();
nv::flash::Status query(const ApInfo& ap, nv::flash::ProgressPercent& progress);

bool is_in_progress();
void cancel(bool report_failure);

}  // namespace ap_background_copy

}  // namespace nv::vrot

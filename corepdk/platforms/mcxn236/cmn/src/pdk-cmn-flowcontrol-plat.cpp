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
#include <cstdint>
#include "nv/common/system.h"

using namespace nv::common;
// C-Wrapper for Ada
extern "C" void System_Abort(uint8_t exit_value)
{
    if (exit_value > static_cast<uint8_t>(ExitValue::HardFault)) {
        exit_value = static_cast<uint8_t>(ExitValue::HardFault);
    }
    System::inst().abort(static_cast<ExitValue>(exit_value));
}
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
#include "fsl_wwdt.h"

namespace sys::watchdog {

enum wwdt_instance
{
    wwdt0,
    wwdt1,
};

class WwdtDriver
{
public:
    static void init(wwdt_instance instance, uint32_t reset_ms, bool enable_reset);
    static void feed(wwdt_instance instance);
    static void trigger_wdt_reset_if_enabled(wwdt_instance instance);
    static bool is_enabled(wwdt_instance instance);
    static constexpr uint32_t OneSecMs = 1'000;

    // WWDT divide input frequency by fixed value 4
    static constexpr uint32_t WwdtPrescale = 4;

    static constexpr uint32_t WdenMask = 0x1;
};

}  // namespace sys::watchdog

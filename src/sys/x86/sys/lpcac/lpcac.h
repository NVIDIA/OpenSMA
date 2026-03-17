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

namespace sys::lpcac {

// Interface for the LPCAC (Low Power Cache Controller). The LPCAC is a 16KB L1
// cache for the code bus on the primary core (core0 for MCX N-5xx).
class Lpcac
{
public:
    static void enable() {}
    static void disable() {}

    Lpcac() = delete;
};

}  // namespace sys::lpcac
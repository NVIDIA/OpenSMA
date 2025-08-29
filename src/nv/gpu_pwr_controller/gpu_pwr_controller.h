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

#include <array>
#include <stdint.h>
#include <chrono>
#include "nv/ipc/event.h"
#include "nv/ipc/task.h"

/*
P4476 Coridelia Power Compliance Module
-----------------------------------------------
Functionality: To be defined in next fw release

*/
using namespace std::chrono_literals;

namespace nv::gpu_pwr_controller {

class PowerCompliance
{
public:
    // Initializes power compliance module
    PowerCompliance();

    // Main work loop for RTOS task
    [[noreturn]] void main();

private:
    // Nothing for now
};

}  // namespace nv::gpu_pwr_controller

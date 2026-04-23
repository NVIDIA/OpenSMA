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

// Separate build for handle_test function
// This file is compiled independently and the binary is loaded to RAM

// #include "nv/nv.h"  // This brings nv::info into scope via using declaration
// #include "nv/gpio/driver.h"
#include "nv/mctp/driver.h"
extern "C" {

// Single test handler - takes test ID and dispatches to appropriate test
// This function will be built separately and copied to RAM
__attribute__((used, section(".text"))) void handle_test(uint32_t test_id)
{
    switch (test_id) {
        case 1:
            // You can call any MCU function here
            // Example: nv::gpio::Driver::set(4, 15, true);
            break;

        case 2:
            // nv::info("Test2 executing from RAM!\n");
            // Example: I2C operations
            break;

        case 3:
            // nv::info("Test3 executing from RAM!\n");
            // Example: Memory tests
            break;

        case 4:
            // nv::info("Test4 executing from RAM!\n");
            // Example: System diagnostics
            break;

        default:
            // nv::info("Unknown test ID: %d\n", test_id);
            break;
    }
}

}  // extern "C"

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
#include "nv/reg_ds/gpio.h"

#include "nv/ut/unittest.h"

using namespace nv;
using namespace ut;

TEST(DsGpio, read)
{
    reg_ds::Gpio    gpio;
    reg_table::DsData test_data = {1, 1, 1, 1};

    // GPIO test clears the register and sets the bit 0 valid bit
    ensure::is_true(gpio.read(reg_ds::Handle::GpioThermOvertN, test_data));
    ensure::is_eq(0, test_data[0]); // upper valid byte
    ensure::is_eq(1, test_data[1]); // lower valid byte
    ensure::is_eq(0, test_data[2]); // upper data byte
    ensure::is_eq(0, test_data[3]); // lower data byte
};

TEST(DsGpio, write)
{
    reg_ds::Gpio    gpio;
    reg_table::DsData test_data = {1, 1, 1, 1};

    // GPIO write just clears the register for now, verify that
    ensure::is_true(gpio.write(reg_ds::Handle::GpioThermOvertN, test_data));
    for (auto& data : test_data) {
        ensure::is_eq(0, data);
    }
};

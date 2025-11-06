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
#include "nv/reg_ds/info.h"

#include "nv/ut/unittest.h"

using namespace nv;
using namespace ut;

TEST(DsInfo, read)
{
    reg_ds::Info    info;
    reg_table::DsData test_data = {1, 1, 1, 1};

    // Verify read changes value for just first handle for now
    for (size_t i = 0; i < 1; i++) {
        ensure::is_true(info.read(reg_ds::Handle(i), test_data));
        for (auto& data : test_data) {
            ensure::is_ne(1, data);
            data = 1;
        }
    }

    // bad read, read beyond info entry in testrunner
    ensure::is_false(info.read(reg_ds::Handle(reg_ds::InfoSize), test_data));
};

TEST(DsInfo, write)
{
    reg_ds::Info    info;
    reg_table::DsData test_data = {1, 1, 1, 1};

    // Info writes are not supported
    ensure::is_false(info.write(reg_ds::Handle::InfoGetCpuUsage, test_data));
};

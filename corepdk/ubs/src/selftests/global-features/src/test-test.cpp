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
#include "ubs/unittest.hpp"

using namespace ubs::unittest;

UBS_TEST(GlobalFeatures, CppTest)
{
#if UBS_GLOBAL_FORMAT_TEST == 0
    ensure::is_true(ubs::features::RegTable);
    ensure::is_eq(ubs::features::RegTable, 1);
#elif UBS_GLOBAL_FORMAT_TEST == 1
    ensure::is_false(ubs::features::RegTable);
    ensure::is_eq(ubs::features::RegTable, 0);
#elif UBS_GLOBAL_FORMAT_TEST == 2
    ensure::is_true(ubs::features::RegTable);
    ensure::is_eq(ubs::features::RegTable, 2);
#elif UBS_GLOBAL_FORMAT_TEST == 3
    ensure::is_false(ubs::features::RegTable);
    ensure::is_eq(ubs::features::RegTable, 0);
#elif UBS_GLOBAL_FORMAT_TEST == 4
    ensure::is_true(ubs::features::RegTable);
    ensure::is_eq(ubs::features::RegTable, 1);
#else
#error "UBS_GLOBAL_FORMAT_TEST is not defined"
#endif
    ensure::is_eq(ubs::features::Count, 5);
};

UBS_TEST(GlobalFeatures, CTest)
{
#if UBS_GLOBAL_FORMAT_TEST == 0
    ensure::is_eq(UbsFeaturesRegTable, 1);
#elif UBS_GLOBAL_FORMAT_TEST == 1
    ensure::is_eq(UbsFeaturesRegTable, 0);
#elif UBS_GLOBAL_FORMAT_TEST == 2
    ensure::is_eq(UbsFeaturesRegTable, 2);
#elif UBS_GLOBAL_FORMAT_TEST == 3
    ensure::is_eq(UbsFeaturesRegTable, 0);
#elif UBS_GLOBAL_FORMAT_TEST == 4
    ensure::is_eq(UbsFeaturesRegTable, 1);
#else
#error "UBS_GLOBAL_FORMAT_TEST is not defined"
#endif
    ensure::is_eq(UbsFeaturesCount, 5);
};

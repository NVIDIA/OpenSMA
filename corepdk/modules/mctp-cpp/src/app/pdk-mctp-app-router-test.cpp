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
#include "pdk-mctp-app-router-plat.h"
#include "pdk-mctp-platforms-config.h"
#include "pdk-mctp-platforms-router.h"
#include "ubs/unittest.hpp"

using namespace ubs::unittest;
using namespace pdk::mctp;

UBS_TEST(MctpRouter, SetAndGetCureid)
{
    uint8_t                 test_eid = 159;
    platforms::RoutingTable test_info{};
    for (uint8_t i = static_cast<uint8_t>(platforms::Interface::Begin);
         i < static_cast<uint8_t>(platforms::Interface::UsEnd);
         ++i) {
        ensure::is_eq(platforms::get_cur_eid(test_info, i), 0);
        platforms::set_cur_eid(test_info, i, test_eid);
        ensure::is_eq(platforms::get_cur_eid(test_info, i), test_eid);
    }
};

UBS_TEST(MctpRouter, GetCureidInvalidInterfaceIndex)
{
    uint8_t    default_eid       = 159;
    const auto default_interface = static_cast<uint8_t>(platforms::DefaultInterface);
    const auto invalid_interface = static_cast<uint8_t>(platforms::Interface::UsEnd);
    platforms::RoutingTable test_info{};
    ensure::is_eq(platforms::get_cur_eid(test_info, invalid_interface), 0);
    platforms::set_cur_eid(test_info, default_interface, default_eid);
    ensure::is_eq(platforms::get_cur_eid(test_info, invalid_interface), default_eid);
};

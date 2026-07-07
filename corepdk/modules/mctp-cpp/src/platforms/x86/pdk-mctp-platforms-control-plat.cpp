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
#include "app/pdk-mctp-app-control-plat.h"
#include "app/pdk-mctp-app-control.h"
#include "app/pdk-mctp-app-enums.h"
#include "app/pdk-mctp-app-packet-plat.h"
#include "app/pdk-mctp-app-router-plat.h"
#include "pdk-mctp-platforms-enums.h"
#include "pdk/cmn/log/log.h"

void pdk::mctp::platforms::on_enumerate_plat()
{
    pdk::cmn::log::here().info("MCTP: Enumerate");
}

void pdk::mctp::platforms::on_routing_info_update_plat(uint8_t requester_eid,
                                                       uint8_t requester_interface,
                                                       uint8_t update_eid)
{
    pdk::cmn::log::here().info(
        "MCTP: Update Routing Table, requester_eid: %d, requester_interface: %d, "
        "update_eid: %d",
        requester_eid,
        requester_interface,
        update_eid);
}

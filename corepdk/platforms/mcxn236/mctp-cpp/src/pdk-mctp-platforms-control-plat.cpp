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
#include "corepdk/modules/mctp-cpp/src/app/pdk-mctp-app-control-plat.h"

#include <cstring>

#include "nv/mctp/driver.h"
#include "nv/mctp/enums.h"
#include "nv/mctp/interface.h"
#include "nv/nv.h"
#include "nv/logger/common.h"
#include "nv/logger/log.h"

using namespace nv;
using namespace nv::mctp;

void pdk::mctp::platforms::on_enumerate_plat()
{
    // notify to do enumerate
    Driver::mctp_send_cmd(Driver::CmdCode::Enumerate);
    logger::info(nv::logger::Event::MctpEnumerated, {});
}

void pdk::mctp::platforms::on_routing_info_update_plat(uint8_t requester_eid,
                                                       uint8_t requester_interface,
                                                       uint8_t update_eid)
{
    logger::info(nv::logger::Event::MctpRoutingInfoUpdateRequester,
                 {requester_eid, requester_interface, update_eid});

    // Use on_enumerate_done to update routing info to each task
    Driver::mctp_send_cmd(Driver::CmdCode::EnumerateDone);
}

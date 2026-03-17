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

#include "corepdk/modules/mctp-cpp/src/app/pdk-mctp-app-enums.h"
#include "corepdk/platforms/mcxn236/mctp-cpp/src/pdk-mctp-platforms-router.h"

#include "nv/ipc/queue.h"

namespace pdk::mctp::platforms {
constexpr uint8_t   DownStreamNum           = nv::ipc::DownStreamNum;
constexpr uint8_t   UpStreamNum             = nv::ipc::UpStreamNum;
constexpr uint8_t   DefaultRoutingTableSize = nv::ipc::DefaultRoutingTableSize;
constexpr uint8_t   EidPoolSize             = DownStreamNum;
constexpr uint8_t   RoutingInfoUpdateSize   = nv::ipc::RoutingInfoUpdateSize;
constexpr uint8_t   RoutingTableSize        = nv::ipc::RoutingTableSize;
constexpr Interface DefaultInterface        = Interface::UsUsb;

constexpr inline std::array<RoutingTableEntry, DownStreamNum>
    DownStreamInfos = nv::ipc::DownStreamInfos;

constexpr std::array<app::MsgType, 4> SupportedType = {
    app::MsgType::Pldm,
    app::MsgType::Spdm,
    app::MsgType::VendorPci,
    app::MsgType::VendorIani,
};
constexpr uint8_t SupportedTypeNum = static_cast<uint8_t>(SupportedType.size());

}  // namespace pdk::mctp::platforms
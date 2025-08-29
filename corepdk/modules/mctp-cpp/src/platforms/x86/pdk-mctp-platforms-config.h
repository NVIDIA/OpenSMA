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

#include "app/pdk-mctp-app-enums.h"
#include "pdk-mctp-platforms-router.h"

namespace pdk::mctp::platforms {
// moving testrunner's config here for x86 build since we can't use nv::ipc in mctp-cpp module
constexpr uint8_t DownStreamNum           = 0;
constexpr uint8_t UpStreamNum             = 1;
constexpr uint8_t DefaultRoutingTableSize = DownStreamNum + UpStreamNum;
constexpr uint8_t EidPoolSize             = DownStreamNum;
constexpr uint8_t RoutingInfoUpdateSize   = 4;
constexpr uint8_t RoutingTableSize        = DefaultRoutingTableSize + RoutingInfoUpdateSize;
constexpr bool    I2cIsEndpoint           = false;
constexpr bool    I2cTransparent          = false;
constexpr inline std::array<RoutingTableEntry, DownStreamNum> DownStreamInfos{

};

constexpr std::array<app::MsgType, 4> SupportedType = {
    app::MsgType::Pldm,
    app::MsgType::Spdm,
    app::MsgType::VendorPci,
    app::MsgType::VendorIani,
};
constexpr uint8_t SupportedTypeNum = static_cast<uint8_t>(SupportedType.size());

}  // namespace pdk::mctp::platforms
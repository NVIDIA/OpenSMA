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
#include <bit>

#include "app/pdk-mctp-app-control.h"
#include "app/pdk-mctp-app-enums.h"
#include "pdk-mctp-platforms-config.h"

namespace pdk::mctp::platforms {
class Control : public pdk::mctp::app::Control
{
public:
    Control() = default;
    bool process(const app::Packet& rx, app::Packet& tx);

protected:
    void on_set_endpoint_id(const app::Packet& rx, app::Packet& tx);
    void on_get_routing_table_entry(const app::Packet& rx, app::Packet& tx) const;
};
}  // namespace pdk::mctp::platforms
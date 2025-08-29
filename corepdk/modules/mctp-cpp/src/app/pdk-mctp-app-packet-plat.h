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
#include "pdk-mctp-app-packet.h"

namespace pdk::mctp::platforms {
Packet::InterfaceType get_packet_interface(const app::Packet& pkt);

Packet::LengthType get_packet_length(const app::Packet& pkt);

Packet::InterfaceType get_packet_interface(const PrivateHeader& priv);

Packet::LengthType get_packet_length(const PrivateHeader& priv);

void set_packet_interface(app::Packet& pkt, Packet::InterfaceType interface);

void set_packet_length(app::Packet& pkt, Packet::LengthType len);

void set_packet_interface(PrivateHeader& priv, Packet::InterfaceType interface);

void set_packet_length(PrivateHeader& priv, Packet::LengthType len);
}  // namespace pdk::mctp::platforms
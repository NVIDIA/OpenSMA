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
#include <cstddef>
#include <cstdint>

namespace pdk::mctp::platforms {
// Define the size of MCTP packet packet payload
constexpr size_t TransmitUnit = 64;

// PrivateHeader used for MCTP inter communication
// Platforms can define their own PrivateHeader
namespace Packet {
using LengthType    = uint16_t;
using InterfaceType = uint8_t;
}  // namespace Packet

struct [[gnu::packed]] PrivateHeader
{
    Packet::LengthType    packet_length;
    Packet::InterfaceType packet_interface;
    uint8_t               reserved0;
};

// PrivateHeader should not be empty
constexpr size_t PrivHeaderSize = sizeof(PrivateHeader);

}  // namespace pdk::mctp::platforms
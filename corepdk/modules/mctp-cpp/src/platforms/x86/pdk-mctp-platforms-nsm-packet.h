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
#include <bit>
#include <span>

#include "app/pdk-mctp-app-packet.h"
#include "pdk-cmn-flowcontrol.h"

namespace pdk::mctp::platforms {
constexpr size_t NsmPktBufDataLen = 256;
struct [[gnu::packed]] NsmPacket
{
    PrivateHeader        priv;  ///< private header
    app::TransportHeader hdr;   ///< mctp header
    std::array<uint8_t, NsmPktBufDataLen - sizeof(app::TransportHeader)> msg;  ///< mctp
                                                                               ///< message
                                                                               ///< body

    std::span<uint8_t> to_span() const
    {
        return {std::bit_cast<uint8_t*>(this), sizeof(*this)};
    }

    static auto& from(std::span<uint8_t> data)
    {
        pdk::cmn::flowcontrol::corepdk_assert(data.size() >= sizeof(app::Packet),
                                              "data size >= packet size assertion fail");
        return *std::bit_cast<app::Packet*>(data.data());
    };
};
}  // namespace pdk::mctp::platforms
/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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

#include <span>
#include <expected>

#include "nv/lstp/lstp_common.h"

namespace nv::lstp {

class LstpParser
{
public:
    struct SpiRequest
    {
        uint8_t               channel_id;
        nv::spi::bm::CsPins   cs;
        uint16_t              read_length;
        std::span<uint8_t>    write_data;
        nv::spi::bm::SpiFlags flags;
    };

    /**
     * @brief Validate request header and length
     *
     * Intended for initial router validation before routing between channel types.
     *
     * @param req_buffer The buffer with the message data
     * @param req_size Actual received length on the wire
     * @return LstpStatus
     */
    static LstpStatus validate_request(std::span<uint8_t>& req_buffer, size_t req_size);

    /**
     * @brief Parse channel type for a validated request
     *
     * @param req_buffer The buffer with the message data (validated)
     * @return LstpChannelType
     */
    static LstpChannelType parse_channel_type(std::span<uint8_t>& req_buffer);

    /**
     * @brief Retrieve SPI request metadata from LSTP request
     *
     * @param req_buffer The buffer with the request data
     * @return LstpParser::SpiRequest or error status
     */
    static std::expected<SpiRequest, LstpStatus>
    parse_spi_request(std::span<uint8_t>& req_buffer);
};

}  // namespace nv::lstp

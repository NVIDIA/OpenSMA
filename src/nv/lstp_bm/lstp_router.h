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

#include "nv/lstp/lstp_common.h"

namespace nv::lstp::bm {

class LstpRouter
{
public:
    /**
     * @brief Receives an LSTP message and route to appropriate channel
     *
     * Assumes the buffer has already been validated via LstpParser::validate_request.
     *
     * @param buffer The buffer with the message data
     * @return The status of the operation
     */
    static LstpStatus receive(std::span<uint8_t>& buffer);

    /**
     * @brief Send an error response for a failed request
     *
     * No-op for Success and SilentDrop statuses.
     *
     * @param buffer The original request buffer (used to read the channel ID)
     * @param status The status to report in the error response
     */
    static void send_error(std::span<uint8_t>& buffer, LstpStatus status);

private:
    /**
     * @brief Receive a message from the SPI channel
     * @param buffer The buffer with the request data
     * @return The status of the operation
     */
    static LstpStatus receive_spi(std::span<uint8_t>& buffer);

    /**
     * @brief Receive a message from the I2C channel
     * @param buffer The buffer with the request data
     * @return The status of the operation
     */
    static LstpStatus receive_i2c(std::span<uint8_t>& buffer);
};

}  // namespace nv::lstp::bm

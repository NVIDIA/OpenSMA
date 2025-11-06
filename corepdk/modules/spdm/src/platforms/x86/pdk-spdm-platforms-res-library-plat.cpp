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
#include <cstdlib>
#include <numeric>

#include "pdk-spdm-app-res-library-plat.h"

namespace pdk::spdm::platforms::res::library {
// NOLINTBEGIN
std::array<uint8_t, 1024> buffer_send_to_spdm_responder = {0};
size_t                    send_to_spdm_responder_size   = 0;
std::array<uint8_t, 1024> buffer_send_to_spdm_requester = {0};
size_t                    send_to_spdm_requester_size   = 0;

/*
 * get_data_from_spdm_responsder()
 *
 * This function is called by the SPDM responder to send data to the transport layer.  This is
 * primarily for the SPDM responder to send responses to the requester.
 *
 * buffer is a pointer to the buffer containing bytes to send
 * num_bytes is the number of bytes to send
 *
 * Returns: void
 */
void get_data_from_spdm_responsder(std::span<const uint8_t> data_from_spdm_responsder)
{
    send_to_spdm_responder_size = data_from_spdm_responsder.size();
    std::copy(data_from_spdm_responsder.begin(),
              data_from_spdm_responsder.end(),
              buffer_send_to_spdm_responder.begin());
    return;
}

/*
 * send_data_to_spdm_responsder()
 *
 * This function is called by the SPDM responder to read data.
 *
 * buffer is a pointer to the buffer to copy the data into
 * *num_bytes is the number of bytes in the buffer, routine returns number of bytes copied into
 * buffer
 *
 * Returns: size_t
 */
size_t send_data_to_spdm_responsder(std::span<uint8_t> data_to_spdm_responsder)
{
    // assume the data is for spdm library, which start with spdm msg type 0x05
    send_to_spdm_requester_size = data_to_spdm_responsder.size();
    std::copy(data_to_spdm_responsder.begin(),
              data_to_spdm_responsder.end(),
              buffer_send_to_spdm_requester.begin());
    return send_to_spdm_requester_size;
}

constexpr size_t                        sender_buffer_size = 1024;
std::array<uint8_t, sender_buffer_size> sender_buffer;
void*                                   acquire_sender_buffer()
{
    return sender_buffer.data();
}

void release_sender_buffer(const void* msg_buf_ptr)
{
    return;
}

constexpr size_t                          receiver_buffer_size = 1024;
std::array<uint8_t, receiver_buffer_size> receiver_buffer;
void*                                     acquire_receiver_buffer()
{
    return receiver_buffer.data();
}

void release_receiver_buffer(const void* msg_buf_ptr)
{
    return;
}

size_t get_sender_buffer_size()
{
    return sender_buffer_size;
}

size_t get_receiver_buffer_size()
{
    return receiver_buffer_size;
}
// NOLINTEND
}  // namespace pdk::spdm::platforms::res::library
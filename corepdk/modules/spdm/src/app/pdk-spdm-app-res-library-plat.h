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
#include <span>
#include <stdint.h>

#include "pdk-spdm-app-res-algorithms-enum.h"
namespace pdk::spdm::platforms::res::library {

void*  acquire_sender_buffer();
void   release_sender_buffer(const void* msg_buf_ptr);
size_t get_sender_buffer_size();

void*  acquire_receiver_buffer();
void   release_receiver_buffer(const void* msg_buf_ptr);
size_t get_receiver_buffer_size();

void   get_data_from_spdm_responsder(std::span<const uint8_t> data_from_spdm_responsder);
size_t send_data_to_spdm_responsder(std::span<uint8_t> data_to_spdm_responsder);

}  // namespace pdk::spdm::platforms::res::library
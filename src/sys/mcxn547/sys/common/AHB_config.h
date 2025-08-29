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

#include <cstdint>

namespace sys::common {

/**
 * @brief Main AHB configuration function that automatically configures
 *        stream buffer access using linker symbols.
 */
bool AHBConfig();

/**
 * @brief Configure AHB security rules to allow access to stream buffer
 * @param stream_buffer_start Start address of the stream buffer
 * @param stream_buffer_end End address of the stream buffer in bytes
 * @return true if configuration successful, false if alignment check failed
 */
bool ConfigureStreamBufferAccess(uint32_t stream_buffer_start, uint32_t stream_buffer_end);
bool ConfigureCore1TextAccess();
bool ConfigureCore1RAMAccess();

}  // namespace sys::common
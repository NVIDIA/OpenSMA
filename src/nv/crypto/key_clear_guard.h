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
#include <stdint.h>

namespace nv::crypto {

inline void clear_key(std::span<uint8_t> key)
{
    volatile uint8_t* vp = key.data();
    for (auto n = key.size(); n != 0U; --n) {
        *vp++ = 0U;
    }
}

class [[nodiscard]] KeyClearGuard
{
public:
    explicit KeyClearGuard(std::span<uint8_t> key) noexcept : key_(key) {}
    ~KeyClearGuard() { clear_key(key_); }
    KeyClearGuard(const KeyClearGuard&)            = delete;
    KeyClearGuard(KeyClearGuard&&)                 = delete;
    KeyClearGuard& operator=(const KeyClearGuard&) = delete;
    KeyClearGuard& operator=(KeyClearGuard&&)      = delete;

private:
    std::span<uint8_t> key_;
};

}  // namespace nv::crypto

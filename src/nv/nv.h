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

#include "nv/common/debug.h"
#include "nv/common/expected.h"
#include "nv/common/preproc.h"

#define NV_TRY(...) NV_COMMON_TRY_ROUTER(__VA_ARGS__)(__VA_ARGS__)
#define NV_LAZY     NV_COMMON_LAZY

#define NV_ROE_1(func)                                                                         \
    if (auto status = func; status != decltype(func)::Ok) {                                    \
        return status;                                                                         \
    }

#define NV_ROE_PR(func, ...)                                                                   \
    if (auto status = func; status != decltype(func)::Ok) {                                    \
        nv::error(static_cast<int>(status), __VA_ARGS__);                                      \
        return status;                                                                         \
    }

#define NV_ROE_TR(func, tr, ...)                                                               \
    if (auto status = func; status != decltype(func)::Ok) {                                    \
        nv::error(static_cast<int>(status), __VA_ARGS__);                                      \
        return tr(status);                                                                     \
    }

#define NV_ROE_ROUTER(...) NV_VA_GET_4TH(__VA_ARGS__, NV_ROE_TR, NV_ROE_PR, NV_ROE_1, )
#define NV_ROE(...)        NV_ROE_ROUTER(__VA_ARGS__)(__VA_ARGS__)

// pull some really common stuff into nv namespace
namespace nv {
using common::always_assert;
using common::assert;
using common::debug;
using common::error;
using common::fatal;
using common::info;
using common::warn;
}  // namespace nv

#ifdef __clang__
#pragma clang final(NV_TRY)
#pragma clang final(NV_ROE)
#pragma clang final(NV_LAZY)
#endif

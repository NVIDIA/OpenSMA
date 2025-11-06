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
#if defined(__SSP__) || defined(__SSP_STRONG__) || defined(__SSP_ALL__)                        \
    || defined(__SSP_EXPLICIT__)
constexpr bool SSP_ENABLED = true;
#else
constexpr bool SSP_ENABLED = false;
#endif

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-dcl37-c,cert-dcl51-cpp)
extern uintptr_t __stack_chk_guard;
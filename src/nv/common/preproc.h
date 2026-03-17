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

#define NV_VA_GET_1ST(a0, ...)                                 a0
#define NV_VA_GET_2ND(a0, a1, ...)                             a1
#define NV_VA_GET_3ND(a0, a1, a2, ...)                         a2
#define NV_VA_GET_4TH(a0, a1, a2, a3, ...)                     a3
#define NV_VA_GET_5TH(a0, a1, a2, a3, a4, ...)                 a4
#define NV_VA_GET_6TH(a0, a1, a2, a3, a4, a5, ...)             a5
#define NV_VA_GET_7TH(a0, a1, a2, a3, a4, a5, a6, ...)         a6
#define NV_VA_GET_8TH(a0, a1, a2, a3, a4, a5, a6, a7, ...)     a7
#define NV_VA_GET_9TH(a0, a1, a2, a3, a4, a5, a6, a7, a8, ...) a8
#define NV_VA_SIZE(...)                                        NV_VA_GET_9TH(__VA_OPT__(__VA_ARGS__, ) 8, 7, 6, 5, 4, 3, 2, 1, 0)

static_assert(NV_VA_SIZE() == 0);
static_assert(NV_VA_SIZE(1) == 1);
static_assert(NV_VA_SIZE(1, 2, 3, 4) == 4);

#define NAME_TO_STRING_READ(arg) #arg
#define NAME_TO_STRING(name)     NAME_TO_STRING_READ(name)

#define NV_COMMON_STRINGIZE(...) #__VA_ARGS__
#define NV_NAME_TO_STRING(name)  NV_COMMON_STRINGIZE(name)

#if defined(MCU)

#define SHARED_BSS_LINE(x)  shared_bss_##x
#define SHARED_BSS_STR(x)   NV_NAME_TO_STRING(SHARED_BSS_LINE(x))
#define SHARED_DATA_LINE(x) shared_data_##x
#define SHARED_DATA_STR(x)  NV_NAME_TO_STRING(SHARED_DATA_LINE(x))

/* stack */
#define NV_STACK __attribute__((section("stack")))

/* task data */
#define NV_TASK_DATA alignas(32) __attribute__((section("task_data")))

/* shared variable w\o initial */
#define NV_SHARED_BSS __attribute__((section(SHARED_BSS_STR(__LINE__))))

/* shared variable with initial */
#define NV_SHARED_DATA __attribute__((section(SHARED_DATA_STR(__LINE__))))

/* SRAMX function */
#define NV_SRAMX_CODE __attribute__((section("sramx_code")))

/* system call used to enter kernel mode */
#define NV_SYS_CALL                                                                            \
    __attribute__((naked)) __attribute__((section("system_calls")))                            \
    __attribute__((no_profile_instrument_function))

#define NV_FORCE_INLINE inline __attribute__((always_inline))

#define NV_PRIVILEGED_FUNCTION __attribute__((section("privileged_functions")))

#else

#define NV_STACK
#define NV_TASK_DATA
#define NV_SHARED_BSS
#define NV_SHARED_DATA
#define NV_SRAMX_CODE
#define NV_SYS_CALL
#define NV_FORCE_INLINE
#define NV_PRIVILEGED_FUNCTION

#endif

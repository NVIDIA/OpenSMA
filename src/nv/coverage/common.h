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
#include <stdint.h>
#include <stdlib.h>

using gcov_type = int64_t;

constexpr uint32_t GcovDataMagic          = 0x67636461;
constexpr uint32_t GcovUnitSize           = 4;
constexpr uint32_t GcovTagFunctionLength  = 3;
constexpr uint32_t GcovCounters           = 9;
constexpr uint32_t GcovTagFunction        = 0x01000000;
constexpr uint32_t GcovTagCounterBase     = 0x01a10000;
constexpr uint32_t GcovTagForCounterShift = 17u;

struct gcov_ctr_info
{
    unsigned int num;
    gcov_type*   values;
};

struct GcovFnInfo
{
    const struct GcovInfo* key;
    unsigned int           ident;
    unsigned int           lineno_checksum;
    unsigned int           cfg_checksum;
    gcov_ctr_info          ctrs[];  // NOLINT(*-c-arrays);
};

struct GcovInfo
{
    unsigned int version;
    GcovInfo*    next;
    unsigned int stamp;
    unsigned int checksum;
    const char*  filename;
    void (*merge[GcovCounters])(int64_t*, unsigned int);  // NOLINT
    unsigned int n_functions;
    GcovFnInfo** functions;
};
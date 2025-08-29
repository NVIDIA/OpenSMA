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
//
// This is support code for getting coverage to work in a freestanding environment
// TODO: not complete
// ref:
// https://gcc.gnu.org/onlinedocs/gcc/gcov/profiling-and-test-coverage-in-freestanding-environments.html
//
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "pdk/cmn/console_cpp/pdk-cmn-console.h"

using gcov_type = int64_t;

constexpr uint32_t GcovDataMagic          = 0x67636461;
constexpr uint32_t GcovUnitSize           = 4;
constexpr uint32_t GcovTagFunctionLength  = 3;
constexpr uint32_t GcovCounters           = 8;
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

extern "C" {
void __gcov_merge_add(gcov_type* counters, unsigned int n_counters) {}
}
extern const GcovInfo* const __gcov_info_start[];
extern const GcovInfo* const __gcov_info_end[];

namespace ubs::coverage {

void on_complete()
{
    auto counter_active = [](const GcovInfo* const info, auto type) {
        return info->merge[type] != nullptr;
    };

    constexpr auto gcov_tag_for_counter = [](uint32_t count) {
        return GcovTagCounterBase + (count << GcovTagForCounterShift);
    };

    auto       info_start = &__gcov_info_start[0];
    const auto info_end   = &__gcov_info_end[0];

    // outputs the gcda file for each file
    while (info_start != info_end) {
        auto info = *info_start;
        auto f    = fopen(info->filename, "wb");
        fwrite(&GcovDataMagic, sizeof(uint32_t), 1, f);

        // TODO: we currently get a version mismatch warning due to not having access
        // to x64 adacore c++ pro
        if (info->version < 0x42333220) {  // 'B32 ' allow override
            pdk::cmn::console::fatal("compiler version not supported\n");
        }

        uint32_t ver = 0x4233332a;  // B33*
        fwrite(&ver, sizeof(uint32_t), 1, f);
        fwrite(&info->stamp, sizeof(uint32_t), 1, f);
        fwrite(&info->checksum, sizeof(uint32_t), 1, f);
        for (unsigned i = 0; i < info->n_functions; i++) {
            auto fptr = info->functions[i];
            fwrite(&GcovTagFunction, sizeof(uint32_t), 1, f);
            auto v = GcovTagFunctionLength * GcovUnitSize;
            fwrite(&v, sizeof(uint32_t), 1, f);
            fwrite(&fptr->ident, sizeof(fptr->ident), 1, f);
            fwrite(&fptr->lineno_checksum, sizeof(fptr->lineno_checksum), 1, f);
            fwrite(&fptr->cfg_checksum, sizeof(fptr->cfg_checksum), 1, f);

            auto cptr = fptr->ctrs;

            for (unsigned j = 0; j < GcovCounters; j++) {
                if (!counter_active(info, j)) {
                    continue;
                }

                /* Counter record. */
                v = gcov_tag_for_counter(j);
                fwrite(&v, sizeof(v), 1, f);
                v = cptr->num * 2 * GcovUnitSize;
                fwrite(&v, sizeof(v), 1, f);
                for (unsigned cv_idx = 0; cv_idx < cptr->num; cv_idx++) {
                    fwrite(&cptr->values[cv_idx], sizeof(gcov_type), 1, f);
                }
                cptr++;
            }
        }
        fclose(f);
        info_start++;
    }
}
}  // namespace ubs::coverage

const static struct AutoInstallExitHook
{
    AutoInstallExitHook() noexcept
    {
        auto ret = std::atexit(ubs::coverage::on_complete);
        if (ret) {
            pdk::cmn::console::error("cannot install coverage hook\n");
        }
    }
} ubs_coverage_on_exit;

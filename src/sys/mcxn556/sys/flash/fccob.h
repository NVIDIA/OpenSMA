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

#include "fsl_cache_lpcac.h"
#include "fsl_mem_interface.h"

#include "nv/flash/common.h"
#include "nv/nv.h"

namespace sys::flash {

constexpr uint32_t PhraseSize = 0x10;

struct [[gnu::packed]] FstatReg
{
    uint32_t fail      : 1;
    uint32_t rvsd0     : 1;
    uint32_t cmdabt    : 1;
    uint32_t rvsd1     : 1;
    uint32_t pviol     : 1;
    uint32_t accerr    : 1;
    uint32_t cwsabt    : 1;
    uint32_t ccif      : 1;
    uint32_t cmdprt    : 2;
    uint32_t rvsd2     : 1;
    uint32_t cmdp      : 1;
    uint32_t cmddid    : 4;
    uint32_t dfdif     : 1;
    uint32_t salv_used : 1;
    uint32_t rvsd3     : 6;
    uint32_t pewen     : 2;
    uint32_t rvsd4     : 5;
    uint32_t perdy     : 1;
};
static_assert(sizeof(FstatReg) == 4, "Size of FstatReg is not 4");

enum class FccobCmdCode : uint32_t
{
    ReadAllOnePhrase = 0x4,
    PhraseProgram    = 0x24,
    SectorErase      = 0x42,
};

class FccobDriver
{
public:
    static status_t
    write(nv::flash::Address address, uint32_t length, const nv::flash::Buffer& buffer);
    static bool     verify_erased(nv::flash::Address address, uint32_t length);
    static status_t erase(nv::flash::Address address);
    static status_t read(nv::flash::Address, uint32_t length, const std::span<uint8_t>& buffer);

    static status_t
    read_svc(nv::flash::Address, uint32_t length, const std::span<uint8_t>& buffer);
    static status_t
    read_impl(nv::flash::Address, uint32_t length, const std::span<uint8_t>& buffer);
    static status_t erase_svc(nv::flash::Address address);
    static status_t erase_impl(nv::flash::Address address);
    static status_t phrase_write_svc(nv::flash::Address       address,
                                     const nv::flash::Buffer& buffer,
                                     uint32_t                 offset);
    static status_t phrase_write_impl(nv::flash::Address       address,
                                      const nv::flash::Buffer& buffer,
                                      uint32_t                 offset);

private:
    static status_t          phrase_write(nv::flash::Address       address,
                                          const nv::flash::Buffer& buffer,
                                          uint32_t                 offset = 0);
    static bool              verify_phrase_erased(nv::flash::Address address);
    static void              prepare_fccob_cmd(FccobCmdCode cmd);
    static void              start_fccob_cmd();
    static status_t          wait_write_enable();
    static status_t          wait_write_ready();
    static status_t          wait_command_complete();
    static constexpr uint8_t PewenPhraseWriteEn = 0x1;

    static constexpr uint32_t AllOneWord = 0xFFFFFFFF;
    static constexpr uint8_t  AllOneByte = 0xFF;

    static constexpr uint32_t WaitCounts = 1000;
    static constexpr uint32_t UsecDelay  = 50;
};

}  // namespace sys::flash
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
#include <span>

#include "fsl_runbootloader.h"
#include "nv/mainbox/mailbox.h"
#include "sys/common/address_map.h"

namespace sys::bootloader {

class Driver
{
public:
    Driver() = default;

    enum BootSource
    {
        BootSourceInternalFlash = 0,
        BootSourceFMC           = 2,
    };

    struct ApplicationFaultRecord
    {
        uint32_t           fault_magic;
        uint32_t           cfsr;
        uint32_t           hfsr;
        uint32_t           event_bits;
        std::span<uint8_t> to_span() const
        {
            return {std::bit_cast<uint8_t*>(this), sizeof(*this)};
        }
    };
    static constexpr uint32_t ApplicationFaultMagic = 0xFAFA5A5A;

    static_assert(sizeof(ApplicationFaultRecord) == 16, "ApplicationFaultRecord size mismatch");

    // WAR: Not all platform are boot with FMC in this phase
    static void mark_inactive_auth_pass(bool pass);

protected:
    static constexpr uint8_t  Tag               = 0xEB;
    static constexpr uint32_t ElsAsBootLog0Addr = 0x400009E8 | sys::common::SecureAddr;

    static constexpr uint32_t AliasImage0Addr = 0x20000860;
    static constexpr uint32_t AliasImage1Addr = 0x200018F0;
    static constexpr uint32_t AliasLength     = 0x890;

    static constexpr uint32_t FmcStatusCodeAddr = 0x20002190;
    static constexpr uint32_t BootReasonAddr    = FmcStatusCodeAddr + 4;
    static constexpr uint32_t AuthResultAddr    = BootReasonAddr + 4;
    static constexpr uint32_t TRNGConfigAddr    = 0x01100050;

    static constexpr uint32_t FmcFaultMagic          = 0xABABABAB;
    static constexpr uint32_t FmcFaultStatusAddr     = 0x2000219C;
    static constexpr uint32_t FmcJumpInformationAddr = 0x200021B8;
    static constexpr uint32_t FmcOrdinalNumberAddr   = 0x200022C0;

    static constexpr uint32_t SignatureSize    = 96;
    static constexpr uint32_t SignatureAk0Addr = 0x20002200;
    static constexpr uint32_t SignatureAk1Addr = 0x20002260;

    static constexpr uint32_t BootSourceMask = 0x3;

    static constexpr uint32_t BootSourceAddr = 0x1004000;
    static constexpr uint32_t Ifr0WpRegister = 0x400C7180;
    static constexpr uint32_t FmcWpValue     = 0x22220000;
    static constexpr uint32_t FmcWpSelMask   = 0xFFFF;
    static constexpr uint32_t CustMkSkAddr   = 0x1004160;
    static constexpr uint8_t  CustMkSkSize   = 0x30;

    static constexpr uint32_t OriginalBootReasonAddr = 0x200022C8;
    static constexpr uint32_t
        ApplicationFaultRecordAddr = nv::mainbox::MainBoxMemoryStart
                                   + nv::mainbox::MainBoxMemoryDescs
                                         [static_cast<uint8_t>(
                                              nv::mainbox::MainBoxMemoryType::FaultWdtRecords)]
                                             .offset;

    struct FmcFaultRecord
    {
        uint32_t           fault_magic;
        uint32_t           fault_hfsr;
        uint32_t           fault_cfsr;
        uint32_t           fault_mmfar;
        uint32_t           fault_bfar;
        std::span<uint8_t> to_span() const
        {
            return {std::bit_cast<uint8_t*>(this), sizeof(*this)};
        }
    };
};

}  // namespace sys::bootloader

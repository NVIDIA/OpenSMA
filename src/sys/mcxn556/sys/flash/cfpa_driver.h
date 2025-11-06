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
#include "fsl_flash.h"
#include "fsl_flash_ffr.h"

#include "nv/common/uuid.h"
#include "nv/flash/common.h"
#include "nv/nv.h"

namespace sys::flash {

struct CfpaCustomerProgramSession
{
    cfpa_cfg_info_t cfpa_content{};  // Record the cfpa content before erase
    uint32_t offset{};  // If offset is equals to 0, perform sector erase on inactive page
};

struct RotRecordFlags
{
    uint32_t type_of_rot  : 4;  // 0x1: secp256r1, 0x2: secp384r1;
    uint32_t used_rot_num : 4;
    uint32_t rvsd         : 19;
    uint32_t no_ca_flag   : 1;  // If the NoCa flag is set to 1, then the iskCertificate section
                                // is not present in the certificate block
};

enum class TypeOfRot : uint32_t
{
    Secp256R1 = 0x1,
    Secp384R1 = 0x2
};

static_assert(sizeof(RotRecordFlags) == 4, "Invalid ROTRecordFlags size");

class CfpaDriver
{
public:
    CfpaDriver() : inactive_page(CfpaInactivePageNotInit){};

    void     init_cfpa_flash_config(const flash_config_t& config);
    status_t read_cfpa(const std::span<uint8_t>& buffer, uint32_t offset);
    status_t
    read_cfpa_customer(const std::span<uint8_t>& buffer, uint8_t page_index, uint32_t offset);
    status_t increase_cfpa_secure_version(uint32_t                     sec_version,
                                          nv::flash::KeyRollbackSelect key_rollback_select);
    status_t
    write_cfpa_customer(const nv::flash::Buffer& buffer, uint8_t page_index, uint32_t offset);
    status_t recover_cfpa_customer(uint8_t page_index);

    uint8_t get_cfpa_write_page();

    status_t increase_cfpa_image_key_revoke(uint32_t                     key_revoke,
                                            nv::flash::KeyRollbackSelect key_rollback_select);

    status_t read_cmpa(const std::span<uint8_t>& buffer, uint32_t offset);

private:
    flash_config_t             flash_config{};
    static constexpr uint32_t  CfpaLength              = 0x200;
    static constexpr uint32_t  CmpaLength              = 0x200;
    static constexpr uint32_t  CfpaCustomerOffset      = CfpaLength;
    static constexpr uint32_t  CfpaPageOffset          = 0x2000;
    static constexpr uint32_t  CfpaStart               = 0x1000000;
    static constexpr uint32_t  CfpaVersionOffset       = 0x4;
    static constexpr uint32_t  CfpaHeaderOffset        = 0x2;
    static constexpr uint32_t  CfpaVersionMask         = 0xFFFFFF;
    static constexpr uint32_t  CfpaVersionAuto         = 0xFFFFFF;
    static constexpr uint32_t  CfpaHeaderValue         = 0x9635;
    static constexpr uint32_t  CfpaCustomerUseSize     = 0x1000;
    static constexpr uint8_t   CfpaInactivePageNotInit = 0xFF;
    uint8_t                    inactive_page{};
    CfpaCustomerProgramSession cfpa_custoer_session{};

    status_t write_cfpa_customer_process(const nv::flash::Buffer& buffer,
                                         uint8_t                  page_index,
                                         uint32_t                 offset);
    status_t recover_configuration_area();

    // If the cfpa customer area is updated during runtime, we will need to reset it and make it
    // active before next cfpa configuration write. Otherwise, the content in the customer area
    // will be overwrite by active one.
    bool cfpa_customer_updated{};

    // Get active image version
    static constexpr uint32_t ImageVersionOffset = 0x24;
    static constexpr uint32_t ImageVersionMask   = 0xFFFF0000;
    static constexpr uint32_t ImageVersionShift  = 16;

    uint32_t get_image_version();

    // Get active firmware version
    static constexpr uint32_t InvalidValue           = 0xFFFFFFFF;
    static constexpr uint32_t ManifestMagic          = 0x6d676d69;
    static constexpr uint32_t KeyIndexSkipOffset     = 0x14;
    static constexpr uint32_t CertBlockAddressOffset = 0x28;
    static constexpr uint32_t CertBlockSizeOffset    = 0x8;
    static constexpr uint32_t ManifestVersionOffset  = 0x8;
    uint32_t                  get_firmware_version();

    // Get active key index
    static constexpr uint32_t CertBlockMagic       = 0x72646863;
    static constexpr uint32_t CertBlockFlagsOffset = 0xC;
    uint32_t                  get_key_index();
};

}  // namespace sys::flash

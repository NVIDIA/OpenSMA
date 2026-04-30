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
#include "cfpa_driver.h"

#include "sys/common/utils.h"
#include "sys/flash/fccob.h"
using namespace sys::flash;

namespace {
template<typename T>
std::array<uint8_t, sizeof(T)> as_byte_array(const T& obj)
{
    std::array<uint8_t, sizeof(T)> result{};
    memcpy(result.data(), &obj, sizeof(T));
    return result;
}
}  // namespace

void CfpaDriver::init_cfpa_flash_config(const flash_config_t& config)
{
    flash_config = config;

    // Require SDK ffr_config structure fix
    inactive_page = flash_config.ffrConfig.cfpaPageOffset > 0 ? 0 : 1;
    nv::info("inactive_page %d\n", inactive_page);
}

uint8_t CfpaDriver::get_cfpa_write_page()
{
    // Require SDK ffr_config structure fix
    return inactive_page;
}

status_t CfpaDriver::read_cfpa(const std::span<uint8_t>& buffer, uint32_t offset)
{
    if (nv::common::add(static_cast<uint32_t>(buffer.size()), offset) > CfpaLength) {
        return kStatus_InvalidArgument;
    }

    if (inactive_page == CfpaInactivePageNotInit) {
        nv::info("FFR not init");
        return kStatus_InvalidArgument;
    }

    status_t sts = kStatus_Success;
    sts = FFR_GetCustomerInfieldData(&flash_config, buffer.data(), offset, buffer.size());

#if 0

    if (offset == 0) {
        nv::info("Version 0x%x 0x%x 0x%x 0x%x\n", buffer[4], buffer[5], buffer[6], buffer[7]);
        
    }
    else if (offset == 256) {
        nv::info("API read 0x%x 0x%x 0x%x 0x%x\n",
                 buffer[144],
                 buffer[145],
                 buffer[146],
                 buffer[147]);
        nv::info("CMAC  0x%x 0x%x 0x%x 0x%x\n", buffer[252], buffer[253], buffer[254], buffer[255]);
    }
#endif
    return sts;
}

status_t CfpaDriver::read_cfpa_customer(const std::span<uint8_t>& buffer,
                                        uint8_t                   page_index,
                                        uint32_t                  offset)
{
    if (page_index > 1) {
        return kStatus_InvalidArgument;
    }

    if (nv::common::add(offset, static_cast<uint32_t>(buffer.size())) > CfpaCustomerUseSize) {
        return kStatus_InvalidArgument;
    }

    if (inactive_page == CfpaInactivePageNotInit) {
        nv::info("FFR not init");
        return kStatus_InvalidArgument;
    }

    const uint32_t CustomerAddress = CfpaStart + CfpaCustomerOffset
                                   + (CfpaPageOffset * page_index) + offset;

    memcpy(buffer.data(), std::bit_cast<void*>(CustomerAddress), buffer.size());

#if 0
    nv::info("read_cfpa_customer page_index %d address 0x%x\n", page_index, address);
    uint32_t another_address = CfpaStart + CfpaCustomerOffset + (CfpaPageOffset * (1-page_index)); 
    std::array<uint8_t, 512> read_buf2;
    nv::info("\nPAGE start\n");
    memcpy(read_buf2.data(), std::bit_cast<void*>(0x1000004), read_buf2.size());
    nv::info("0x%x 0x%x 0x%x 0x%x 0x%x\n", 0x1000004, read_buf2[0],read_buf2[1],read_buf2[2],read_buf2[3]);
    memcpy(read_buf2.data(), std::bit_cast<void*>(0x1002004), read_buf2.size());
    nv::info("0x%x 0x%x 0x%x 0x%x 0x%x\n", 0x1002004, read_buf2[0],read_buf2[1],read_buf2[2],read_buf2[3]);


    memcpy(read_buf2.data(), std::bit_cast<void*>(0x1000190), read_buf2.size());
    nv::info("0x%x 0x%x 0x%x 0x%x 0x%x\n", 0x1000190, read_buf2[0],read_buf2[1],read_buf2[2],read_buf2[3]);
    memcpy(read_buf2.data(), std::bit_cast<void*>(0x1002190), read_buf2.size());
    nv::info("0x%x 0x%x 0x%x 0x%x 0x%x\n", 0x1002190, read_buf2[0],read_buf2[1],read_buf2[2],read_buf2[3]);
    nv::info("\nCustomer start\n");
    for(uint32_t i = 0 ; i < 16; i++) {
        memcpy(read_buf2.data(), std::bit_cast<void*>(address + i * 256), read_buf2.size());
        nv::info("0x%x: \n", address + i * 256);
        for(uint32_t j = 0 ; j < 4; j++) {
            nv::info("0x%x ",read_buf2[j]);
        }
        nv::info("\n");
    }
    nv::info("\nAnother Customer start\n");

    for(uint32_t i = 0 ; i < 16; i++) {
        memcpy(read_buf2.data(), std::bit_cast<void*>(another_address + i * 256), read_buf2.size());
        // nv::info("0x%x 0x%x 0x%x 0x%x 0x%x\n", another_address + i * 256, read_buf2[0],read_buf2[1],read_buf2[2],read_buf2[3]);
        nv::info("0x%x: \n", another_address + i * 256);
        for(uint32_t j = 0 ; j < 4; j++) {
            nv::info("0x%x ",read_buf2[j]);
        }
        nv::info("\n");

    }

#endif

    return kStatus_Success;
}

status_t
CfpaDriver::increase_cfpa_secure_version(uint32_t                     sec_version,
                                         nv::flash::KeyRollbackSelect key_rollback_select)
{
    // 0x01000080	MCTR_CUST_CTR0: AP0 key revocation
    // 0x01000090	MCTR_CUST_CTR4: AP0 rollback protection

    if (inactive_page == CfpaInactivePageNotInit) {
        nv::info("FFR not init");
        return kStatus_InvalidArgument;
    }

    if (cfpa_customer_updated) {
        nv::info("Error: will overwrite cfpa customer\n");
        return kStatus_Fail;
    }

    status_t        sts = kStatus_Success;
    cfpa_cfg_info_t cfpa_read{};
    sts = FFR_GetCustomerInfieldData(
        &flash_config, std::bit_cast<uint8_t*>(&cfpa_read), 0, CfpaLength);

    if (sts != kStatus_Success) {
        return sts;
    }
    // Only enforce CFPA-local monotonicity here. The "sec_version must not exceed the
    // running FW's SVN / inactive slot SVN / FMC SVN" policy belongs to the NSM layer
    // (update_min_sec_ver_num) and is already validated there before this path runs.
    if (key_rollback_select == nv::flash::KeyRollbackSelect::Mcu) {
        if (cfpa_read.secureFwVersion > sec_version) {
            nv::info("Invalid sec_version %d\n", sec_version);
            return kStatus_InvalidArgument;
        }
        cfpa_read.secureFwVersion = sec_version;
    }
    else if (key_rollback_select == nv::flash::KeyRollbackSelect::Ap0) {
        if (cfpa_read.custCtr[4] > sec_version) {
            nv::info("Invalid sec_version %d\n", sec_version);
            return kStatus_InvalidArgument;
        }
        cfpa_read.custCtr[4] = sec_version;
    }
    else {
        return kStatus_InvalidArgument;
    }

    cfpa_read.cfpa_page_version.version = CfpaVersionAuto;
    sts                                 = FFR_InfieldPageWrite(
        &flash_config, std::bit_cast<uint8_t*>(&cfpa_read), CfpaLength);  // Version will
                                                                          // updated

    L1CACHE_InvalidateCodeCache();

    return sts;
}

status_t
CfpaDriver::increase_cfpa_image_key_revoke(uint32_t                     key_revoke,
                                           nv::flash::KeyRollbackSelect key_rollback_select)
{
    // 0x01000080	MCTR_CUST_CTR0: AP0 key revocation
    // 0x01000090	MCTR_CUST_CTR4: AP0 rollback protection

    if (inactive_page == CfpaInactivePageNotInit) {
        nv::info("FFR not init");
        return kStatus_InvalidArgument;
    }

    if (cfpa_customer_updated) {
        nv::info("Error: will overwrite cfpa customer\n");
        return kStatus_Fail;
    }

    status_t        sts = kStatus_Success;
    cfpa_cfg_info_t cfpa_read{};
    sts = FFR_GetCustomerInfieldData(
        &flash_config, std::bit_cast<uint8_t*>(&cfpa_read), 0, CfpaLength);
    if (sts != kStatus_Success) {
        return sts;
    }

    // Only enforce CFPA-local monotonicity here. The "key_revoke must not exceed the
    // running FW's signing key index / inactive / FMC" policy belongs to the NSM layer
    // (update_key_permission) and is already validated there before this path runs.
    if (key_rollback_select == nv::flash::KeyRollbackSelect::Mcu) {
        if (cfpa_read.imageKeyRevoke > key_revoke) {
            nv::info("Invalid key_revoke %d\n", key_revoke);
            return kStatus_InvalidArgument;
        }
        cfpa_read.imageKeyRevoke = key_revoke;
    }
    else if (key_rollback_select == nv::flash::KeyRollbackSelect::Ap0) {
        if (cfpa_read.custCtr[0] > key_revoke) {
            nv::info("Invalid key_revoke %d\n", key_revoke);
            return kStatus_InvalidArgument;
        }
        cfpa_read.custCtr[0] = key_revoke;
    }
    else {
        return kStatus_InvalidArgument;
    }

    cfpa_read.cfpa_page_version.version = CfpaVersionAuto;
    sts                                 = FFR_InfieldPageWrite(
        &flash_config, std::bit_cast<uint8_t*>(&cfpa_read), CfpaLength);  // Version will
                                                                          // updated

    L1CACHE_InvalidateCodeCache();

    return sts;
}

status_t CfpaDriver::write_cfpa_customer(const nv::flash::Buffer& buffer,
                                         uint8_t                  page_index,
                                         uint32_t                 offset)
{
    if (inactive_page == CfpaInactivePageNotInit) {
        nv::info("FFR not init");
        return kStatus_InvalidArgument;
    }

    if (nv::common::add(offset, static_cast<uint32_t>(buffer.size())) > CfpaCustomerUseSize) {
        return kStatus_InvalidArgument;
    }

    status_t sts = kStatus_Success;

    // Update the version without update the secure fw version
    // This will become active page in next boot if the copy content back is done

    if (cfpa_custoer_session.offset == 0) {
        uint32_t address = CfpaStart + (CfpaPageOffset * page_index);
        nv::info("write_cfpa_customer page_index %d addr 0x%x Value:0x%x\n",
                 page_index,
                 address,
                 buffer[0]);

        sts = FFR_GetCustomerInfieldData(
            &flash_config,
            std::bit_cast<uint8_t*>(&cfpa_custoer_session.cfpa_content),
            0,
            CfpaLength);
        if (sts != kStatus_Success) {
            return sts;
        }

        cfpa_custoer_session.cfpa_content.cfpa_page_version.version = CfpaVersionAuto;
        sts = FFR_InfieldPageWrite(&flash_config,
                                   std::bit_cast<uint8_t*>(&cfpa_custoer_session.cfpa_content),
                                   CfpaLength);
        if (sts != kStatus_Success) {
            return sts;
        }
        sts = FFR_GetCustomerInfieldData(
            &flash_config,
            std::bit_cast<uint8_t*>(&cfpa_custoer_session.cfpa_content),
            0,
            CfpaLength);
        if (sts != kStatus_Success) {
            return sts;
        }

        sts = sys::flash::FccobDriver::erase(address);
        if (sts != kStatus_Success) {
            return sts;
        }

        sts = recover_configuration_area();
        if (sts != kStatus_Success) {
            return sts;
        }
    }

    sts = write_cfpa_customer_process(buffer, page_index, offset);

    if (offset == 0 && sts == kStatus_Success) {
        cfpa_customer_updated = true;
    }

    return sts;
}

status_t CfpaDriver::write_cfpa_customer_process(const nv::flash::Buffer& buffer,
                                                 uint8_t                  page_index,
                                                 uint32_t                 offset)
{
    uint32_t address = CfpaStart + (CfpaPageOffset * page_index) + CfpaLength + offset;
    if (!FccobDriver::verify_erased(address, buffer.size())) {
        nv::info("[write_cfpa_customer_process]:Sector not erased 0x%x\n", address);
        return kStatus_Fail;
    }
    auto sts = sys::flash::FccobDriver::write(address, buffer.size(), buffer);

    cfpa_custoer_session.offset = nv::common::add(offset, static_cast<uint32_t>(buffer.size()));

    L1CACHE_InvalidateCodeCache();
    return sts;
}

// Write the content of cfpa back
status_t CfpaDriver::recover_configuration_area()
{
    nv::flash::Buffer write_buffer{};
    const uint32_t    ConfigAddress = CfpaStart + (CfpaPageOffset * inactive_page);

    // nv::info("[write_cfpa_customer_end]: address 0x%x\n", address);

    if (!FccobDriver::verify_erased(ConfigAddress, nv::flash::BufferSize)
        || !FccobDriver::verify_erased(ConfigAddress + nv::flash::BufferSize,
                                       nv::flash::BufferSize)) {
        nv::info("[write_cfpa_customer_end]: Sector not erased\n");
        return kStatus_Fail;
    }

    memcpy(write_buffer.data(),
           std::bit_cast<uint8_t*>(&cfpa_custoer_session.cfpa_content),
           nv::flash::BufferSize);
    auto sts = sys::flash::FccobDriver::write(
        ConfigAddress, nv::flash::BufferSize, write_buffer);
    if (sts != kStatus_Success) {
        return sts;
    }

    const size_t offset = offsetof(CfpaCustomerProgramSession, cfpa_content)
                        + nv::flash::BufferSize;
    if (offset + nv::flash::BufferSize <= sizeof(cfpa_custoer_session)) {
        auto byte_array = as_byte_array(cfpa_custoer_session);
        memcpy(write_buffer.data(), byte_array.data() + offset, nv::flash::BufferSize);
    }

    sts = sys::flash::FccobDriver::write(
        ConfigAddress + nv::flash::BufferSize, nv::flash::BufferSize, write_buffer);
    if (sts != kStatus_Success) {
        return sts;
    }

    L1CACHE_InvalidateCodeCache();
    return sts;
}

status_t CfpaDriver::read_cmpa(const std::span<uint8_t>& buffer, uint32_t offset)
{
    if (nv::common::add(static_cast<uint32_t>(buffer.size()), offset) > CmpaLength) {
        return kStatus_InvalidArgument;
    }

    if (inactive_page == CfpaInactivePageNotInit) {
        nv::info("FFR not init");
        return kStatus_InvalidArgument;
    }

    status_t sts = kStatus_Success;
    sts          = FFR_GetCustomerData(&flash_config, buffer.data(), offset, buffer.size());

    return sts;
}
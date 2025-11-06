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
#include "nv/flash/driver.h"

using namespace nv::flash;
namespace {
Status kstatus_to_status(status_t sts)
{
    switch (sts) {
        case kStatus_FLASH_Success                    : return Status::Ok;
        case kStatus_FLASH_SizeError                  :
        case kStatus_FLASH_AlignmentError             :
        case kStatus_FLASH_AddressError               :
        case kStatus_InvalidArgument                  :
        case kStatus_OutOfRange                       :
        case kStatus_FLASH_InvalidPropertyValue       : return Status::InvalidParam;
        case sys::flash::IapDriver::IapCumulativeWrite:
        case kStatusMemoryCumulativeWrite             : return Status::CumulativeWrite;
        default                                       : return Status::Error;
    }
    return Status::Error;
}
}  // namespace

Status Driver::init(ApiSelect cur_select)
{
    if (api_select < ApiSelect::End) {
        return Status::Error;
    }
    constexpr uint32_t NvmCtrlDisableEcc  = 0x20000;
    SYSCON0->NVM_CTRL                    |= (NvmCtrlDisableEcc);

    api_select            = cur_select;
    Status   flash_status = Status::Ok;
    status_t sts{};
    switch (api_select) {
        case ApiSelect::Flash: {
            sts = flash_api_driver.init();
            cfpa_driver.init_cfpa_flash_config(flash_api_driver.get_flash_config());
            if (sts != kStatus_Success) {
                flash_status = kstatus_to_status(sts);
            }
        } break;
        case ApiSelect::Iap: {
            sts = iap_driver.init();
            if (sts != kStatus_Success) {
                flash_status = kstatus_to_status(sts);
            }
        } break;
        default: {
            // Error
            flash_status = Status::InvalidParam;
        } break;
    }

    return flash_status;
}

Status Driver::read(uint32_t address, uint32_t length, Buffer& buffer)
{
    status_t sts          = kStatus_Success;
    Status   flash_status = Status::Ok;
    switch (api_select) {
        case ApiSelect::Flash: {
            sts = flash_api_driver.read(address, length, buffer);
            if (sts != kStatus_Success) {
                flash_status = kstatus_to_status(sts);
            }
        } break;
        case ApiSelect::Iap: {
            sts = iap_driver.read(address, length, buffer);
            if (sts != kStatus_Success) {
                flash_status = kstatus_to_status(sts);
            }
        } break;
        default: {
            // Error
            flash_status = Status::InvalidParam;
        } break;
    }
    return flash_status;
}
Status Driver::write(uint32_t address, uint32_t length, const Buffer& buffer)
{
    status_t sts          = kStatus_Success;
    Status   flash_status = Status::Ok;

    if (length % sys::flash::BromApiAlignLength != 0 && length % sys::flash::PhraseSize != 0) {
        return Status::InvalidParam;
    }

    if (length % sys::flash::BromApiAlignLength != 0 && length % sys::flash::PhraseSize == 0) {
        auto sts = sys::flash::FccobDriver::write(address, length, buffer);
        if (sts != kStatus_Success) {
            flash_status = kstatus_to_status(sts);
        }
        return flash_status;
    }

    switch (api_select) {
        case ApiSelect::Flash: {
            sts = flash_api_driver.write(address, length, buffer);
            if (sts != kStatus_Success) {
                flash_status = kstatus_to_status(sts);
            }
        } break;
        case ApiSelect::Iap: {
            sts = iap_driver.write(address, length, buffer);
            if (sts != kStatus_Success) {
                flash_status = kstatus_to_status(sts);
            }
        } break;
        default: {
            flash_status = Status::InvalidParam;
        } break;
    }

    return flash_status;
}
Status Driver::erase(uint32_t address)
{
    status_t sts          = kStatus_Success;
    Status   flash_status = Status::Ok;
    switch (api_select) {
        case ApiSelect::Flash: {
            sts = flash_api_driver.erase(address);
            if (sts != kStatus_Success) {
                flash_status = kstatus_to_status(sts);
            }
        } break;
        case ApiSelect::Iap: {
            sts = iap_driver.erase(address);
            if (sts != kStatus_Success) {
                flash_status = kstatus_to_status(sts);
            }
        } break;
        default: {
            // Error
            flash_status = Status::InvalidParam;
        } break;
    }
    return flash_status;
}

Status Driver::write_phrase(uint32_t address, uint32_t length, const Buffer& buffer)
{
    Status flash_status = Status::Ok;

    if (length % sys::flash::PhraseSize != 0 || address % sys::flash::PhraseSize != 0) {
        return Status::InvalidParam;
    }

    auto sts = sys::flash::FccobDriver::write(address, length, buffer);
    if (sts != kStatus_Success) {
        flash_status = kstatus_to_status(sts);
    }
    return flash_status;
}

Status Driver::check_all_erased(uint32_t address, uint32_t length)
{
    if (length % sys::flash::PhraseSize != 0 || address % sys::flash::PhraseSize != 0) {
        return Status::InvalidParam;
    }

    auto erased = sys::flash::FccobDriver::verify_erased(address, length);

    if (erased) {
        return Status::Ok;
    }

    return Status::Error;
}

Status Driver::static_erase(uint32_t address)
{
    Status flash_status = Status::Ok;

    if (address % SectorSize != 0) {
        return Status::InvalidParam;
    }
    auto sts = sys::flash::FccobDriver::erase(address);

    if (sts != kStatus_Success) {
        flash_status = Status::Error;
    }
    return flash_status;
}

Status Driver::get_uuid(nv::common::Uuid& uuid)
{
    auto status = sys::flash::ApiDriver::get_uuid(uuid);

    Status flash_status = Status::Ok;

    if (status != kStatus_Success) {
        flash_status = Status::Error;
    }
    return flash_status;
}

uint8_t Driver::get_cfpa_write_page()
{
    return cfpa_driver.get_cfpa_write_page();
}

nv::flash::Status Driver::read_cfpa(const std::span<uint8_t>& buffer, uint32_t offset)
{
    auto flash_status = Status::Ok;

    auto sts = cfpa_driver.read_cfpa(buffer, offset);
    if (sts != kStatus_Success) {
        flash_status = kstatus_to_status(sts);
    }

    return flash_status;
}

nv::flash::Status Driver::read_cfpa_customer(const std::span<uint8_t>& buffer,
                                             uint8_t                   page_index,
                                             uint32_t                  offset)
{
    auto flash_status = Status::Ok;

    auto sts = cfpa_driver.read_cfpa_customer(buffer, page_index, offset);
    if (sts != kStatus_Success) {
        flash_status = kstatus_to_status(sts);
    }

    return flash_status;
}

nv::flash::Status Driver::increase_cfpa_secure_version(uint32_t          sec_version,
                                                       KeyRollbackSelect key_rollback_select)
{
    (void)key_rollback_select;
    auto flash_status = Status::Ok;
    auto sts          = cfpa_driver.increase_cfpa_secure_version(sec_version);
    if (sts != kStatus_Success) {
        flash_status = kstatus_to_status(sts);
    }

    return flash_status;
}

nv::flash::Status Driver::increase_cfpa_image_key_revoke(uint32_t          key_revoke,
                                                         KeyRollbackSelect key_rollback_select)
{
    (void)key_rollback_select;
    auto flash_status = Status::Ok;

    auto sts = cfpa_driver.increase_cfpa_image_key_revoke(key_revoke);
    if (sts != kStatus_Success) {
        flash_status = kstatus_to_status(sts);
    }

    return flash_status;
}

nv::flash::Status
Driver::write_cfpa_customer(const Buffer& buffer, uint8_t page_index, uint32_t offset)
{
    auto flash_status = Status::Ok;

    auto sts = cfpa_driver.write_cfpa_customer(buffer, page_index, offset);
    if (sts != kStatus_Success) {
        flash_status = kstatus_to_status(sts);
    }

    return flash_status;
}

nv::flash::Status Driver::read_cmpa(const std::span<uint8_t>& buffer, uint32_t offset)
{
    auto flash_status = Status::Ok;

    auto sts = cfpa_driver.read_cmpa(buffer, offset);
    if (sts != kStatus_Success) {
        flash_status = kstatus_to_status(sts);
    }

    return flash_status;
}

nv::flash::Status Driver::read_efuse(uint32_t& data, uint32_t offset)
{
    auto flash_status = Status::Ok;

    auto sts = otp_driver.read(offset, data);
    if (sts != kStatus_Success) {
        flash_status = kstatus_to_status(sts);
    }

    return flash_status;
}

nv::flash::Status Driver::program_efuse(const uint32_t data, uint32_t offset)
{
    auto flash_status = Status::Ok;

    auto sts = otp_driver.program(offset, data);
    if (sts != kStatus_Success) {
        flash_status = kstatus_to_status(sts);
    }

    return flash_status;
}

nv::flash::Status Driver::read_crc(uint32_t& data)
{
    auto flash_status = Status::Ok;

    auto sts = otp_driver.read_crc(data);
    if (sts != kStatus_Success) {
        flash_status = kstatus_to_status(sts);
    }

    return flash_status;
}

nv::flash::Status Driver::read_life_cycle(sys::flash::LifeCycleStatus& data)
{
    auto flash_status = Status::Ok;

    auto sts = otp_driver.read_life_cycle(data);
    if (sts != kStatus_Success) {
        flash_status = kstatus_to_status(sts);
    }

    return flash_status;
}

nv::flash::Status Driver::init_efuse()
{
    auto flash_status = Status::Ok;

    auto sts = otp_driver.init();
    if (sts != kStatus_Success) {
        flash_status = Status::Error;
    }
    auto efuse_version = otp_driver.get_version();
    nv::info("efuse version: major: %d, minor: %d, bugfix: %d, name %c",
             efuse_version.major,
             efuse_version.minor,
             efuse_version.bugfix,
             efuse_version.name);
    return flash_status;
}

Status Driver::init_on_fault()
{
    flash_config_t config = {};
    auto           status = FLASH_Init(&config);
    if (status != kStatus_Success) {
        return Status::Error;
    }
    return Status::Ok;
}
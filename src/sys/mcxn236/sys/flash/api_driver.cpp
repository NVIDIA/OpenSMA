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
#include "api_driver.h"

#include <FreeRTOSConfig.h>
#include <portmacrocommon.h>

#include "nv/nv.h"
#include "sys/common/utils.h"
#include "sys/flash/fccob.h"
using namespace sys::flash;

status_t ApiDriver::init()
{
    auto sts = FLASH_Init(&flash_config);
    if (sts != kStatus_Success) {
        return sts;
    }
    sts = FFR_Init(&flash_config);
    nv::info("flash_config.ffrConfig.cfpaPageOffset 0x%x\n",
             flash_config.ffrConfig.cfpaPageOffset);
    nv::info("flash_config.ffrConfig.cfpaPageVersion 0x%x\n",
             flash_config.ffrConfig.cfpaPageVersion);

    return sts;
}

status_t ApiDriver::read(uint32_t address, uint32_t length, nv::flash::Buffer& buffer)
{
    return FLASH_Read(&flash_config, address, static_cast<uint8_t*>(&buffer[0]), length);
}

status_t ApiDriver::write(uint32_t address, uint32_t length, const nv::flash::Buffer& buffer)
{
    auto verify_erase_status = FLASH_VerifyErase(&flash_config, address, length);
    if (verify_erase_status != kStatus_FLASH_Success) {
        if (verify_erase_status == KstatusEraseVerifyFail) {
            return kStatusMemoryCumulativeWrite;
        }
        return verify_erase_status;
    }

    auto sts = FLASH_Program(
        &flash_config, address, std::bit_cast<uint8_t*>(&buffer[0]), length);
    L1CACHE_InvalidateCodeCache();
    return sts;
}

status_t ApiDriver::erase(uint32_t address)
{
    auto sts = FLASH_Erase(
        &flash_config, address, flash_config.PFlashSectorSize, kFLASH_ApiEraseKey);
    L1CACHE_InvalidateCodeCache();
    return sts;
}

// @WAR GFWLYNT1-296
// NOLINTNEXTLINE(*-avoid-non-const-global-variables*)
flash_config_t g_flash_config;

status_t ApiDriver::get_uuid(nv::common::Uuid& uuid)
{
    auto sts = FLASH_Init(&g_flash_config);
    if (sts != kStatus_Success) {
        return sts;
    }

    sts = FFR_Init(&g_flash_config);
    if (sts != kStatus_Success) {
        return sts;
    }

    sts = FFR_GetUUID(&g_flash_config, uuid.begin());
    if (sts != kStatus_Success) {
        return sts;
    }

    sts = FLASH_Deinit(&g_flash_config);
    if (sts != kStatus_Success) {
        return sts;
    }

    return sts;
}

const flash_config_t& ApiDriver::get_flash_config()
{
    return flash_config;
}
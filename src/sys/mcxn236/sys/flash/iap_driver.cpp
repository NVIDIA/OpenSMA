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
#include "iap_driver.h"

#include <FreeRTOSConfig.h>
#include <portmacrocommon.h>

#include "fsl_flash_ffr.h"

#include "sys/common/utils.h"

using namespace sys::flash;

inline bool need_to_flush(uint32_t length)
{
    return (length < 512) || (length % 1024);
}

status_t IapDriver::init()
{
    api_init_param.allocStart = std::bit_cast<uintptr_t>(&iap_buffer[0]);
    memset(&api_core_ctx, 0, sizeof(api_core_ctx));
    auto sts = API_Init(&api_core_ctx, &api_init_param);

    // TBD: the following call cause BG failed, need another way to enable cfpa driver
    // sts = FLASH_Init(&api_core_ctx.flashConfig);
    // FFR_Init(&api_core_ctx.flashConfig);

    return sts;
}

#if 0
const flash_config_t& IapDriver::get_flash_config()
{
    return api_core_ctx.flashConfig;
}
#endif

status_t IapDriver::read(uint32_t address, uint32_t length, nv::flash::Buffer& buffer)
{
    const status_t Sts = kStatus_Success;
    memcpy(static_cast<uint8_t*>(&buffer[0]), std::bit_cast<void*>(address), length);
    return Sts;
}
status_t IapDriver::write(uint32_t address, uint32_t length, const nv::flash::Buffer& buffer)
{
    auto sts = MEM_Write(
        &api_core_ctx, address, length, std::bit_cast<uint8_t*>(&buffer[0]), kMemoryInternal);
    if (sts != kStatus_Success) {
        L1CACHE_InvalidateCodeCache();
        return sts;
    }

    if (need_to_flush(length)) {
        sts = MEM_Flush(&api_core_ctx);
    }
    L1CACHE_InvalidateCodeCache();
    return sts;
}
status_t IapDriver::erase(uint32_t address)
{
    auto sts = MEM_Erase(&api_core_ctx, address, nv::flash::SectorSize, kMemoryInternal);
    L1CACHE_InvalidateCodeCache();
    if (sts != kStatus_Success) {
        nv::info("IapDriver::erase sts:%d\n", sts);
    }
    return sts;
}

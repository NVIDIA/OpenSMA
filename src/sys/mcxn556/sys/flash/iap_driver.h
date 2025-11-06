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

static constexpr size_t IapBufferSize = 0xC00;
class IapDriver
{
public:
    IapDriver() = default;

    status_t read(uint32_t address, uint32_t length, nv::flash::Buffer& buffer);
    status_t write(uint32_t address, uint32_t length, const nv::flash::Buffer& buffer);
    status_t erase(uint32_t address);
    status_t init();

    // const flash_config_t& get_flash_config();
    constexpr static status_t IapCumulativeWrite = 100004;

private:
    std::array<uint8_t, IapBufferSize> iap_buffer{};
    api_core_context_t                 api_core_ctx{};
    kp_api_init_param_t                api_init_param = {
                       .allocSize = IapBufferSize /* Configuration information size. */
    };
};

}  // namespace sys::flash
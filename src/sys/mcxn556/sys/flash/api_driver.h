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
class ApiDriver
{
public:
    ApiDriver() = default;

    status_t        read(uint32_t address, uint32_t length, nv::flash::Buffer& buffer);
    status_t        write(uint32_t address, uint32_t length, const nv::flash::Buffer& buffer);
    status_t        erase(uint32_t address);
    status_t        init();
    static status_t get_uuid(nv::common::Uuid& uuid);
    const flash_config_t& get_flash_config();

private:
    flash_config_t            flash_config{};
    constexpr static status_t KstatusEraseVerifyFail = 154;
};

}  // namespace sys::flash

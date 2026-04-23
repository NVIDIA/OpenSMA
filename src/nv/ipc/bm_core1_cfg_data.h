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

namespace nv::ipc {

/**
 * Layout of the block passed from Core0 to Core1 as the single startup_data pointer.
 * Stored in shared_bss on Core0; Core1 reads it via the pointer received from MCMGR.
 * Data at core1_cfg_data_base is written by Core0 before starting Core1 and
 * is read-only for Core1 (never changed after Core1 has started).
 */
struct StartupInfo
{
    uint32_t c2c_buffers_base;     ///< Address of _c2c_buffers[0] for C2C init
    uint32_t core1_cfg_data_base;  ///< Address of read-only block for Core1 (optional, 0 if
                                   ///< unused)
};

/// Instance in shared_bss (Core0). Core0 fills and passes &g_core1_startup_data as startup
/// data.
extern StartupInfo g_core1_startup_data;

struct [[gnu::packed]] Core1CfgData
{
    uint32_t magic;            // magic number 0xC04E1CF6("CORE1CFG") to identify valid layout
    uint32_t version;          // version of core1 config layout, ver=1
    uint32_t init_data_valid;  // non-zero=core0 has written and validated init data.
    uint32_t reserved;
    uint32_t ncsi_mac_valid;   // non-zero=mac address below is valid and shall be used
    uint8_t  ncsi_mac[6];      // NSCI/CDC_ECM MAC address (big-endian)
    uint8_t  ncsi_padding[2];  // pad to 4-byte boundary
    // uint8_t  unused[228];
};

/** Core1CfgData magic value: 0xC04E1CF6 (big-endian in magic[4]) */
static constexpr uint32_t kCore1CfgMagic = 0xC04E1CF6U;

}  // namespace nv::ipc

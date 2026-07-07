/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
#include "nv/vrot/platform/lpu_flash_layout.h"

#include <array>

#include "nv/ut/unittest.h"

using namespace nv;
using namespace ut;

TEST(LpuFlashLayout, encrypted_firmware_data_size)
{
    constexpr std::array<uint8_t, vrot::lpu::BlockSize> pointer_data = {
        0x27, 0x1f, 0x02, 0x00, 0x08, 0x00, 0x00, 0x00,
        0x85, 0x17, 0x00, 0x00, 0x00, 0x01, 0x63, 0xcd,
    };

    const auto pointer = vrot::lpu::parse_pointer_block(pointer_data);
    ensure::is_eq(vrot::lpu::ErrorCode::Ok, pointer.error);

    const auto size = vrot::lpu::get_firmware_data_size(pointer.block);
    ensure::is_eq(vrot::lpu::ErrorCode::Ok, size.error);
    ensure::is_eq(uint32_t{0x178f0}, size.size);
};

TEST(LpuFlashLayout, plaintext_firmware_data_size)
{
    constexpr std::array<uint8_t, vrot::lpu::BlockSize> pointer_data = {
        0x27, 0x1f, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x13, 0x1a, 0x00, 0x00, 0x00, 0x00, 0x86, 0xde,
    };

    const auto pointer = vrot::lpu::parse_pointer_block(pointer_data);
    ensure::is_eq(vrot::lpu::ErrorCode::Ok, pointer.error);

    const auto size = vrot::lpu::get_firmware_data_size(pointer.block);
    ensure::is_eq(vrot::lpu::ErrorCode::Ok, size.error);
    ensure::is_eq(uint32_t{0x1a180}, size.size);
};

TEST(LpuFlashLayout, encrypted_header_must_follow_spi_data)
{
    constexpr vrot::lpu::PointerBlock pointer = {
        .metadata = {
            .magic          = vrot::lpu::Magic,
            .num_spi_blocks = 2,
            .fw_idx         = 7,
            .fw_size        = 1,
            .uds_idx        = 0,
            .is_encrypted   = vrot::lpu::EncryptedFlagMask,
        },
        .crc = 0,
    };

    const auto size = vrot::lpu::get_firmware_data_size(pointer);
    ensure::is_eq(vrot::lpu::ErrorCode::InvalidLayout, size.error);
};

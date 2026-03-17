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
#include "sys/bootloader/bootloader.h"
#include "nv/ipc/timer.h"

#include NV_IPC_CONFIG_H

namespace nv::bootloader {

class Driver : protected sys::bootloader::Driver
{
public:
    enum class ImageIndex : uint32_t
    {
        Begin  = 0,
        Image0 = Begin,
        Image1,
        Invalid,
        End,
    };

    enum class State : uint32_t
    {
        Begin      = 0,
        Idle       = Begin,
        InProgress = 1,
        Complete   = 2,
        Invalid    = 3,
        Stage      = 4,
        End,
    };

    Driver() = default;

    static void run_on_index(ImageIndex image_index);

    static ImageIndex current_boot_index();
    static void       boot_init();
    static void       set_stack_cookie();
    static void       hw_init();
    static void       disable_all_flexcomm_irq();
    static bool       set_image_bootable(ImageIndex slot, bool bootable);
    static bool       set_inactive_bootable(bool bootable);
    static void       check_boot_reason();
    static void       self_reset();

    static void on_timer([[maybe_unused]] ipc::Timer& id);
    static void set_task_booted(nv::ipc::BootedEventBits boot_bit);

    static uint32_t   get_boot_reason();
    static uint32_t   get_auth_result();
    static uint32_t   get_fmc_status();
    static uint32_t   get_fmc_jump_information();
    static uint64_t   get_fmc_ordinal_number();
    static uint32_t   get_TRNG_config();
    static void       clear_inactive_alias(ImageIndex inactive_index);
    static void       get_fmc_fault(const std::span<uint8_t>& buffer);
    static uint8_t    get_boot_src();
    static uint8_t    get_boot_src_from_kernel();
    static bool       is_inactive_auth_pass();
    static ImageIndex get_previous_boot_slot();
    static void       enable_fault();
    static void       set_fmc_wp();
    static bool       check_cust_mk_sk();

    static void     write_original_boot_reason(uint32_t reason);
    static uint32_t get_original_boot_reason();

    static void write_application_fault_record(uint32_t fault_magic,
                                               uint32_t cfsr,
                                               uint32_t hfsr,
                                               uint32_t event_bits);
    static sys::bootloader::Driver::ApplicationFaultRecord get_application_fault_record();

    // static void mark_updated(ImageIndex index);
};

}  // namespace nv::bootloader

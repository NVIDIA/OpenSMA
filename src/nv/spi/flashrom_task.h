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
#include <span>

#include "nv/spi/port.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/task.h"
#include "nv/gpio/common.h"
#include "sys/spi/spi_edma.h"
#include "nv/spi/common.h"
#include "nv/spi/flashrom.h"

namespace nv::spi {
class FlashromTask : public nv::ipc::Task
{
public:
    struct Config
    {
        nv::ipc::TaskId          task_id;
        std::string_view         task_name;
        nv::spi::Flexcomm        flexcomm_id;
        sys::spi::EdmaDriver*    edma_driver;
        gpio::GpioPort           cs_port_id;
        gpio::GpioPin            cs_pin_id;
        gpio::GpioPort           cs1_port_id;
        gpio::GpioPin            cs1_pin_id;
        nv::ipc::BootedEventBits boot_event;

        Config(nv::ipc::TaskId          task_id_,
               std::string_view         task_name_,
               nv::spi::Flexcomm        flexcomm_id_,
               sys::spi::EdmaDriver*    edma_driver_,
               gpio::GpioPort           cs_port_id_,
               gpio::GpioPin            cs_pin_id_,
               gpio::GpioPort           cs1_port_id_,
               gpio::GpioPin            cs1_pin_id_,
               nv::ipc::BootedEventBits boot_event_)
        : task_id(task_id_)
        , task_name(task_name_)
        , flexcomm_id(flexcomm_id_)
        , edma_driver(edma_driver_)
        , cs_port_id(cs_port_id_)
        , cs_pin_id(cs_pin_id_)
        , cs1_port_id(cs1_port_id_)
        , cs1_pin_id(cs1_pin_id_)
        , boot_event(boot_event_)
        {}
    };

    static void make(Config config);
    static void entrypoint(void* params);
    static bool to_spi(std::span<uint8_t>& msg);

    FlashromTask(Config config) noexcept;
    [[noreturn]] void start();

private:
    nv::ipc::BootedEventBits _boot_event;

    Flashrom _flashrom;
};
}  // namespace nv::spi

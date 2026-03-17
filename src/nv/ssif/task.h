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
#pragma once
#include <string_view>

#include "nv/ipc/event.h"
#include "nv/ipc/task.h"
#include "nv/ssif/ssif.h"

namespace nv::ssif {

class Task : public nv::ipc::Task
{
public:
    struct Config
    {
        nv::ipc::TaskId  task_id;
        std::string_view task_name;
        nv::ipc::EventId event;
        nv::i2c::Port    i2c_bus;
        gpio::GpioPort   alert_port_id;
        gpio::GpioPin    alert_pin_id;

        Config(nv::ipc::TaskId  task_id_,
               std::string_view task_name_,
               nv::ipc::EventId event_,
               nv::i2c::Port    i2c_bus_,
               gpio::GpioPort   alert_port_id_,
               gpio::GpioPin    alert_pin_id_)
        : task_id(task_id_)
        , task_name(task_name_)
        , event(event_)
        , i2c_bus(i2c_bus_)
        , alert_port_id(alert_port_id_)
        , alert_pin_id(alert_pin_id_)
        {}
    };

    static void make(Config config);
    static void entrypoint(void* params);
    static bool to_ssif(std::span<uint8_t>& msg);
    static void enable();
    static bool is_default_enabled();

    Task(Config config) noexcept;
    [[noreturn]] void start();

private:
    nv::ipc::Event _event;
    Ssif           _ssif;
};
}  // namespace nv::ssif

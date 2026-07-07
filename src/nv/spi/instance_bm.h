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

#include <cstddef>
#include NV_IPC_CONFIG_H
#include "nv/spi/spi_types.h"
#include "sys/spi_bm/spi_bm.h"

namespace nv::spi::bm {

class SpiManager
{
public:
    static void bind(nv::spi::Port                                         port,
                     volatile uint32_t*                                    event,
                     nv::ecm_bm::AppEvent                                  event_bit,
                     std::array<nv::ipc::Gpios, nv::spi::bm::SpiMaxCsPins> cs_pins)
    {
        get(port).bind(port, event, event_bit, cs_pins);
    }

    static SpiStatus transfer(nv::spi::Port            port,
                              CsPins                   cs,
                              uint32_t                 read_length,
                              const std::span<uint8_t> send_buffer,
                              SpiFlags                 flags)
    {
        return get(port).transfer(cs, read_length, send_buffer, flags);
    }

    static SpiResult& get_result(nv::spi::Port port) { return get(port).get_result(); }

    static void spi_gpio_wait_complete(nv::spi::Port port)
    {
        get(port).spi_gpio_wait_complete();
    }

    static bool is_busy(nv::spi::Port port) { return get(port).is_busy(); }

private:
    static inline std::array<sys::spi::bm::Driver, SpiNumInstances> _drivers;

    static constexpr sys::spi::bm::Driver& get(nv::spi::Port port)
    {
        for (size_t i = 0; i < SpiNumInstances; ++i) {
            if (SpiInstances.at(i).port == port) {
                return _drivers.at(i);
            }
        }
        __builtin_unreachable();
    }
};

}  // namespace nv::spi::bm

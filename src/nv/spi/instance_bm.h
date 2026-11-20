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

#include <array>
#include <cstddef>
#include <cstdint>
#include NV_IPC_CONFIG_H
#include "nv/spi/spi_types.h"
#include "sys/spi_bm/spi_bm.h"

namespace nv::spi::bm {

/**
 * Bare-metal SPI manager.
 *
 * Driver slots are allocated at runtime by bind(). get() looks up the bound
 * port → slot map. Envelope config only needs SpiNumInstances (max slots);
 * product-specific port/CS tables live with the caller (e.g. SpiInstances for
 * single-product images, or product_spi_instances() for multi-product images).
 */
class SpiManager
{
public:
    static void bind(nv::spi::Port                                         port,
                     volatile uint32_t*                                    event,
                     nv::ecm_bm::AppEvent                                  event_bit,
                     std::array<nv::ipc::Gpios, nv::spi::bm::SpiMaxCsPins> cs_pins)
    {
        get_or_allocate(port).bind(port, event, event_bit, cs_pins);
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

    static void
    check_gpio_wait_timeout(nv::spi::Port port, uint32_t now_ms, uint32_t timeout_ms)
    {
        get(port).check_gpio_wait_timeout(now_ms, timeout_ms);
    }

    static bool is_gpio_wait_pending(nv::spi::Port port)
    {
        return get(port).is_gpio_wait_pending();
    }

    static void poll_gpio_war(nv::spi::Port port,
                              uint32_t      now_ms,
                              uint32_t      ready_timeout_ms,
                              uint32_t      idle_timeout_ms,
                              uint32_t      recovery_timeout_ms)
    {
        get(port).poll_gpio_war(now_ms, ready_timeout_ms, idle_timeout_ms, recovery_timeout_ms);
    }

    static bool is_recovery(nv::spi::Port port) { return get(port).is_recovery(); }

    static bool is_awaiting_ready_release(nv::spi::Port port)
    {
        return get(port).is_awaiting_ready_release();
    }

    static bool is_busy(nv::spi::Port port) { return get(port).is_busy(); }

private:
    static inline std::array<sys::spi::bm::Driver, SpiNumInstances> _drivers{};
    static inline std::array<nv::spi::Port, SpiNumInstances>        _ports{};
    static inline std::array<bool, SpiNumInstances>                 _bound{};

    static sys::spi::bm::Driver& get_or_allocate(nv::spi::Port port)
    {
        for (size_t i = 0; i < SpiNumInstances; ++i) {
            if (_bound[i] && _ports[i] == port) {
                return _drivers[i];
            }
        }
        for (size_t i = 0; i < SpiNumInstances; ++i) {
            if (!_bound[i]) {
                _bound[i] = true;
                _ports[i] = port;
                return _drivers[i];
            }
        }
        __builtin_unreachable();
    }

    static sys::spi::bm::Driver& get(nv::spi::Port port)
    {
        for (size_t i = 0; i < SpiNumInstances; ++i) {
            if (_bound[i] && _ports[i] == port) {
                return _drivers[i];
            }
        }
        __builtin_unreachable();
    }
};

}  // namespace nv::spi::bm

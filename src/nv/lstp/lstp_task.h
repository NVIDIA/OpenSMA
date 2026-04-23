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
#include <atomic>
#include <string_view>

#include NV_IPC_CONFIG_H

#include "sys/gpio/constant.h"
#include "nv/gpio/common.h"
#include "nv/ipc/event.h"
#include "nv/ipc/task.h"
#include "nv/lstp/lstp_common.h"
#include "nv/lstp/lstp_router.h"

namespace nv::lstp {

/** @brief (port, pin) -> lstp_idx. LstpGpioNum = not LSTP. */
constexpr auto BuildLstpGpioIdxMap()
{
    std::array<std::array<uint16_t, sys::gpio::PinsPerPort>, sys::gpio::PortsNumber> result{};
    for (auto& row : result) {
        row.fill(LstpGpioNum);
    }
    for (size_t i = 0; i < LstpGpioNum; ++i) {
        const auto& pin_info = PinConfigs.at(i);
        if (pin_info.port < sys::gpio::PortsNumber && pin_info.pin < sys::gpio::PinsPerPort) {
            result[pin_info.port][pin_info.pin] = static_cast<uint16_t>(i);
        }
    }
    return result;
}
constexpr inline auto LstpGpioIdxMap = BuildLstpGpioIdxMap();

/**
 * @brief LSTP GPIO task: event-driven loop for GPIO channel requests.
 *
 * Waits for request event, processes from task buffers, sends response.
 * Other LSTP channels are unaffected; the GPIO channel returns Busy while a request
 * is being processed. Mutex protects _req_pkt/_resp_payload; _processing allows at most
 * one request in progress.
 */
class LstpTask : public nv::ipc::Task
{
public:
    struct Config
    {
        nv::ipc::TaskId          task_id;
        std::string_view         task_name;
        nv::ipc::BootedEventBits boot_event;

        Config(nv::ipc::TaskId          task_id_,
               std::string_view         task_name_,
               nv::ipc::BootedEventBits boot_event_)
        : task_id(task_id_)
        , task_name(task_name_)
        , boot_event(boot_event_)
        {}
    };

    LstpTask(Config config) noexcept;
    static void       make(Config config);
    static void       entrypoint(void* params);
    [[noreturn]] void start();

    /**
     * @brief Submits request and returns immediately (non-blocking).
     * Copies into task buffer, signals task. Response sent async via LstpRouter::send_gpio.
     * Returns Busy while a request is being processed.
     */
    static LstpStatus submit_gpio_req(std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& req_buffer);

    /** @brief Submits event and returns immediately (non-blocking).
     * Sends IRQ event to LstpGpioIrq queue. Events sent async via
     * LstpRouter::send_gpio_irq_event. Drops event if queue is full.
     */
    static void submit_gpio_irq(GpioIndex gpio_idx);

    /** @brief Initializes LSTP GPIO configuration.
     * Should not be called if pins are handled/init by a different task
     */
    static void LstpGpioInit();

private:
    /** @brief Processes request from _req_pkt, fills _resp_payload, and sends
     * via LstpRouter::send_gpio. */
    void process_gpio_req();

    /** @brief Drain IRQ buffer, send each event via LstpRouter::send_gpio_irq_event. */
    void process_gpio_irq();

    static constexpr nv::ipc::Event::Bits GpioReqBit = 1U << 0; /* bit of Lstp */
    static constexpr nv::ipc::Event::Bits GpioIrqBit = 1U << 1; /* bit of Lstp */

    nv::ipc::BootedEventBits _boot_event;

    /** Note: best practice is to have LSTP GPIO IRQ pins disabled by default - they
     * should be enabled later by the host, after enumeration
     */
    std::array<LstpGpioIrqConfig, LstpGpioNum> _irq_config{};
};

}  // namespace nv::lstp

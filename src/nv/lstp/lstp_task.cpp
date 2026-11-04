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
#include "nv/lstp/lstp_router.h"
#include "nv/lstp/lstp_task.h"

#include <bit>
#include <chrono>
#include <cstring>
#include <span>
#include <task.h>

#include "nv/bootloader.h"
#include "nv/gpio/common.h"
#include "nv/gpio/driver.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/tasknotify.h"
#include "nv/nv.h"
#include "nv/usb/task.h"
#include "sys/ipc/supervisor.h"
#include NV_IPC_CONFIG_H

namespace nv::lstp {

namespace {

inline nv::gpio::Status gpio_read(nv::gpio::GpioPort port, nv::gpio::GpioPin pin, uint8_t& data)
{
    if constexpr (projectHasVirtualGpios(PinConfigs)) {
        return nv::gpio::Driver::read_virtual_physical_gpio(port, pin, data);
    }
    else {
        return nv::gpio::Driver::read(port, pin, data);
    }
}

inline nv::gpio::Status
gpio_write(nv::gpio::GpioPort port, nv::gpio::GpioPin pin, const uint8_t data)
{
    if constexpr (projectHasVirtualGpios(PinConfigs)) {
        return nv::gpio::Driver::write_virtual_physical_gpio(port, pin, data);
    }
    else {
        return nv::gpio::Driver::write(port, pin, data);
    }
}

}  // namespace

LstpTask::LstpTask(Config config) noexcept
: nv::ipc::Task(config.task_id, config.task_name)
, _boot_event(config.boot_event)
{}

void LstpTask::make(Config config)
{
    if constexpr (EnableGpio) {
        constexpr auto StackSize = std::max(640, int(configMINIMAL_STACK_SIZE));

        NV_TASK_DATA static LstpTask                   task(config);
        NV_STACK static sys::ipc::TaskStack<StackSize> stack;

        // NOLINTNEXTLINE(*-reinterpret-cast)
        const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&task), sizeof(LstpTask));
        task.setup(stack.span(), Priv, Priority::Lstp, LstpTask::entrypoint);
    }
}

void LstpTask::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<LstpTask*>(params);
    task.start();
    task.suspend();
}

[[noreturn]] void LstpTask::start()
{
    _irq_config.fill(LstpGpioIrqConfig::IrqDisabled);

    nv::bootloader::Driver::set_task_booted(_boot_event);

    // coverity[no_escape] should never leave here
    while (true) {
        auto result = nv::ipc::TaskNotify::wait(nv::ipc::TaskNotify::Usecs::max(), true);
        if (!result.has_value()) {
            continue;
        }
        const auto bits = result.value();
        if (bits & LstpTask::GpioReqBit) {
            process_gpio_req();
        }
        if (bits & LstpTask::GpioIrqBit) {
            process_gpio_irq();
        }
    }
}

/*****************************************************
 * Process LSTP Requests from USB
 *****************************************************/

LstpStatus LstpTask::submit_gpio_req(std::span<uint8_t>& req_buffer)
{
    const auto& req_hdr     = from<LstpHdr>(req_buffer.data());
    const auto  payload_len = static_cast<size_t>(req_hdr.len_lsb
                                                 | (req_hdr.len_msb << BYTE1_SHIFT));
    if (payload_len > LstpMaxPayloadSize) {
        return LstpStatus::Error;
    }
    auto status = nv::ipc::Queue::make(nv::ipc::QueueId::LstpToGpio).send(req_buffer);
    if (status != nv::ipc::Queue::Status::Ok) {
        return LstpStatus::Error;
    }

    (void)nv::ipc::TaskNotify::send(nv::ipc::TaskId::Lstp, GpioReqBit);
    return LstpStatus::Success;
}

void LstpTask::process_gpio_req()
{
    std::array<uint8_t, nv::ipc::UsbLstpMsgSize> req_buffer{};
    std::array<uint8_t, nv::ipc::UsbLstpMsgSize> resp_buffer{};

    std::span<uint8_t> item(req_buffer.data(), nv::ipc::UsbLstpMsgSize);
    auto&              queue  = nv::ipc::Queue::make(nv::ipc::QueueId::LstpToGpio);
    auto               status = queue.recv(item);
    if (status != ipc::Queue::Status::Ok) {
        return;
    }

    auto&  req_hdr    = from<LstpHdr>(req_buffer.data());
    size_t req_offset = sizeof(LstpHdr);

    auto& resp_hdr      = from<LstpHdr>(resp_buffer.data());
    resp_hdr.channel_id = req_hdr.channel_id;
    size_t resp_offset  = sizeof(LstpHdr);

    auto send_resp = [&](size_t len, LstpStatus resp_status) {
        resp_hdr.cmd_status_code = static_cast<uint8_t>(resp_status) | LstpResponseBit;
        resp_hdr.len_lsb         = static_cast<uint8_t>(len & LSB_MASK);
        resp_hdr.len_msb         = static_cast<uint8_t>(len >> BYTE1_SHIFT);
        LstpRouter::send_gpio(resp_buffer);
    };

    const uint16_t payload_len = req_hdr.len_lsb | (req_hdr.len_msb << BYTE1_SHIFT);
    if (payload_len > LstpMaxPayloadSize) {
        send_resp(0, LstpStatus::Error);
        return;
    }

    const size_t req_size = sizeof(LstpHdr) + payload_len;
    const size_t resp_max = LstpMaxPayloadSize;

    auto cmd = static_cast<LstpGpioCommand>(req_hdr.cmd_status_code & LstpGpioCmdMask);
    switch (cmd) {
        case LstpGpioCommand::GetValue: {
            while (req_offset + sizeof(LstpGpioGetValueRequest) <= req_size) {
                auto& req   = from<LstpGpioGetValueRequest>(&req_buffer.data()[req_offset]);
                req_offset += sizeof(LstpGpioGetValueRequest);
                if (req.gpio_index >= LstpGpioNum) {
                    send_resp(0, LstpStatus::Error);
                    return;
                }
                const auto& pin_info    = PinConfigs.at(req.gpio_index);
                uint8_t     value       = 0;
                auto        gpio_status = gpio_read(pin_info.port, pin_info.pin, value);
                if (LstpStatus_from(gpio_status) != LstpStatus::Success) {
                    send_resp(0, LstpStatus_from(gpio_status));
                    return;
                }
                if (resp_offset + sizeof(LstpGpioGetValueResponse) > resp_max) {
                    send_resp(0, LstpStatus::Error);
                    return;
                }
                auto& resp   = from<LstpGpioGetValueResponse>(&resp_buffer.data()[resp_offset]);
                resp.value   = static_cast<LstpGpioState>(value);
                resp_offset += sizeof(LstpGpioGetValueResponse);
            }
            break;
        }
        case LstpGpioCommand::SetValue: {
            while (req_offset + sizeof(LstpGpioSetValueRequest) <= req_size) {
                auto& req   = from<LstpGpioSetValueRequest>(&req_buffer.data()[req_offset]);
                req_offset += sizeof(LstpGpioSetValueRequest);
                if (req.gpio_index >= LstpGpioNum
                    || (req.value != LstpGpioState::Low && req.value != LstpGpioState::High)) {
                    send_resp(0, LstpStatus::Error);
                    return;
                }
                const auto&         pin_info = PinConfigs.at(req.gpio_index);
                nv::gpio::Direction dir{};
                gpio_get_direction(pin_info, dir);
                if (dir != nv::gpio::Direction::Output) {
                    send_resp(0, LstpStatus::Error);
                    return;
                }
                const auto write_value = static_cast<uint8_t>(req.value);
                const auto gpio_status = gpio_write(pin_info.port, pin_info.pin, write_value);
                if (LstpStatus_from(gpio_status) != LstpStatus::Success) {
                    send_resp(0, LstpStatus_from(gpio_status));
                    return;
                }
            }
            break;
        }
        case LstpGpioCommand::GetIrqConfig: {
            if (req_offset + sizeof(LstpGpioGetIrqConfigRequest) > req_size) {
                send_resp(0, LstpStatus::Error);
                return;
            }
            auto& req = from<LstpGpioGetIrqConfigRequest>(&req_buffer.data()[req_offset]);
            if (req.gpio_index >= LstpGpioNum) {
                send_resp(0, LstpStatus::Error);
                return;
            }
            if (resp_offset + sizeof(LstpGpioGetIrqConfigResponse) > resp_max) {
                send_resp(0, LstpStatus::Error);
                return;
            }
            auto& resp = from<LstpGpioGetIrqConfigResponse>(&resp_buffer.data()[resp_offset]);
            resp.irq_type  = _irq_config.at(req.gpio_index);
            resp_offset   += sizeof(LstpGpioGetIrqConfigResponse);
            break;
        }
        case LstpGpioCommand::SetIrqConfig: {
            if (req_offset + sizeof(LstpGpioSetIrqConfigRequest) > req_size) {
                send_resp(0, LstpStatus::Error);
                return;
            }
            auto& req = from<LstpGpioSetIrqConfigRequest>(&req_buffer.data()[req_offset]);
            if (req.gpio_index >= LstpGpioNum || req.irq_type > LstpGpioIrqConfig::Max) {
                send_resp(0, LstpStatus::Error);
                return;
            }
            const auto& [port, pin, pin_config, allow_set_irq, _] = PinConfigs.at(
                req.gpio_index);
            if (!allow_set_irq) {
                send_resp(0, LstpStatus::NotSupported);
                return;
            }
            if (port == nv::gpio::vrPort) {
                if (pin_config.direction != LstpGpioDirection::Input) {
                    send_resp(0, LstpStatus::Error);
                    return;
                }
                _irq_config.at(req.gpio_index) = req.irq_type;
                break;
            }
            nv::gpio::Direction dir{};
            nv::gpio::Driver::getDirection(port, pin, dir);
            if (dir != nv::gpio::Direction::Input) {
                send_resp(0, LstpStatus::Error);
                return;
            }
            nv::gpio::InterruptSelect irq_sel = nv::gpio::InterruptSelect::InterruptSelect0;
            for (const auto& irq_config : nv::ipc::GpioInterruptSetup) {
                if (std::get<0>(irq_config) == port && std::get<1>(irq_config) == pin) {
                    irq_sel = std::get<3>(irq_config);
                    break;
                }
            }
            const auto                         prev_irq = _irq_config.at(req.gpio_index);
            const nv::gpio::InterruptDetection irq_det  = InterruptDetection_from(req.irq_type);
            _irq_config.at(req.gpio_index)              = req.irq_type;
            auto gpio_status = nv::gpio::Driver::init_interrupt(port, pin, irq_det, irq_sel);
            if (LstpStatus_from(gpio_status) != LstpStatus::Success) {
                _irq_config.at(req.gpio_index) = prev_irq;
                send_resp(0, LstpStatus_from(gpio_status));
                return;
            }
            break;
        }
        default: send_resp(0, LstpStatus::NotSupported); return;
    }

    send_resp(resp_offset - sizeof(LstpHdr), LstpStatus::Success);
}

/*****************************************************
 * Process IRQ events from GPIO pins
 *****************************************************/

void LstpTask::submit_gpio_irq(GpioIndex gpio_idx)
{
    if (!nv::usb::Task::is_lstp_device_connected()) {
        return;
    }

    uint8_t     value    = 0;
    const auto& pin_info = PinConfigs.at(gpio_idx);
    if (LstpStatus_from(gpio_read(pin_info.port, pin_info.pin, value)) != LstpStatus::Success) {
        return;
    }

    LstpGpioIrqEventRequest req{};
    req.gpio_index = gpio_idx;
    req.value      = static_cast<LstpGpioState>(value);

    auto  item   = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&req), sizeof(req));
    auto& queue  = nv::ipc::Queue::make(nv::ipc::QueueId::LstpGpioIrq);
    auto  status = sys::ipc::is_in_isr() ? queue.send_isr(item)
                                         : queue.send(item, std::chrono::microseconds(0));
    if (status != nv::ipc::Queue::Status::Ok) {
        return;  // queue full, drop event
    }

    (void)nv::ipc::TaskNotify::send(nv::ipc::TaskId::Lstp, GpioIrqBit);
}

void LstpTask::process_gpio_irq()
{
    LstpGpioIrqEventRequest event{};  // NOLINT(misc-const-correctness) recv() writes into event
                                      // via Queue::Item
    auto  item      = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&event), sizeof(event));
    auto& irq_queue = nv::ipc::Queue::make(nv::ipc::QueueId::LstpGpioIrq);

    // Find GPIO channel ID (assuming single GPIO channel)
    const uint8_t ch_id = GetFirstChannelId(LstpChannels, LstpChannelType::Gpio);
    if (ch_id == LstpNumChannels) {
        return;
    }

    while (irq_queue.size() > 0) {
        std::array<uint8_t, nv::ipc::UsbLstpMsgSize> irq_buffer{};
        auto&                                        irq_hdr = from<LstpHdr>(irq_buffer.data());
        irq_hdr.channel_id                                   = ch_id;
        irq_hdr.cmd_status_code = static_cast<uint8_t>(LstpGpioCommand::IrqEvent);
        size_t payload_offset   = sizeof(LstpHdr);

        while (irq_queue.size() > 0
               && payload_offset + sizeof(LstpGpioIrqEventRequest) <= nv::ipc::UsbLstpMsgSize) {
            if (irq_queue.recv(item, std::chrono::microseconds(0))
                != nv::ipc::Queue::Status::Ok) {
                break;
            }
            if (!LstpGpioIrqEventMatches(_irq_config.at(event.gpio_index), event.value)) {
                continue;
            }
            auto& req       = from<LstpGpioIrqEventRequest>(&irq_buffer.data()[payload_offset]);
            req             = event;
            payload_offset += sizeof(LstpGpioIrqEventRequest);
        }

        const size_t payload_len = payload_offset - sizeof(LstpHdr);
        if (payload_len == 0) {
            break;
        }
        irq_hdr.len_lsb = static_cast<uint8_t>(payload_len & LSB_MASK);
        irq_hdr.len_msb = static_cast<uint8_t>(payload_len >> BYTE1_SHIFT);
        LstpRouter::send_gpio(irq_buffer);
    }
}
}  // namespace nv::lstp

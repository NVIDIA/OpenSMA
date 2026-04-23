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
#include <task.h>

#include "nv/bootloader.h"
#include "nv/gpio/common.h"
#include "nv/gpio/driver.h"
#include "nv/ipc/event.h"
#include "nv/ipc/queue.h"
#include "nv/nv.h"
#include "nv/usb/task.h"
#include NV_IPC_CONFIG_H

namespace nv::lstp {

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
    auto& event = nv::ipc::Event::make(nv::ipc::EventId::Lstp);

    // coverity[no_escape] should never leave here
    while (true) {
        constexpr auto wait   = LstpTask::GpioReqBit | LstpTask::GpioIrqBit;
        auto           result = event.wait(wait, true, false);
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

LstpStatus LstpTask::submit_gpio_req(std::array<uint8_t, nv::ipc::UsbLstpMsgSize>& req_buffer)
{
    const auto& req_hdr     = from<LstpHdr>(req_buffer.data());
    const auto  payload_len = static_cast<size_t>(req_hdr.len_lsb
                                                 | (req_hdr.len_msb << BYTE1_SHIFT));
    if (payload_len > LstpMaxPayloadSize) {
        return LstpStatus::Error;
    }
    auto item = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&req_buffer), sizeof(req_buffer));
    auto status = nv::ipc::Queue::make(nv::ipc::QueueId::LstpToGpio).send(item);
    if (status != nv::ipc::Queue::Status::Ok) {
        return LstpStatus::Error;
    }

    auto& event = nv::ipc::Event::make(nv::ipc::EventId::Lstp);
    (void)event.set(GpioReqBit);
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
                const auto& pin_info = PinConfigs.at(req.gpio_index);
                uint8_t     value    = 0;
                auto gpio_status = nv::gpio::Driver::read(pin_info.port, pin_info.pin, value);
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
                const auto& [port, pin, pin_config, _] = PinConfigs.at(req.gpio_index);
                (void)_;
                if (pin_config.direction != LstpGpioDirection::Output) {
                    send_resp(0, LstpStatus::Error);
                    return;
                }
                auto write_value = static_cast<uint8_t>(req.value);
                auto gpio_status = nv::gpio::Driver::write(port, pin, write_value);
                if (LstpStatus_from(gpio_status) != LstpStatus::Success) {
                    send_resp(0, LstpStatus_from(gpio_status));
                    return;
                }
                if ((pin_config.output_drive_config == LstpGpioOutputDriveConfig::PushPull)
                    || (pin_config.output_drive_config == LstpGpioOutputDriveConfig::OpenDrain
                        && req.value == LstpGpioState::Low)
                    || (pin_config.output_drive_config == LstpGpioOutputDriveConfig::OpenSource
                        && req.value == LstpGpioState::High)) {
                    uint8_t read_value = 0;
                    gpio_status        = nv::gpio::Driver::read(port, pin, read_value);
                    if (LstpStatus_from(gpio_status) != LstpStatus::Success
                        || static_cast<LstpGpioState>(read_value) != req.value) {
                        send_resp(0, LstpStatus::Error);
                        return;
                    }
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
            const auto& [port, pin, pin_config, allow_set_irq] = PinConfigs.at(req.gpio_index);
            if (!allow_set_irq) {
                send_resp(0, LstpStatus::NotSupported);
                return;
            }
            if (pin_config.direction != LstpGpioDirection::Input) {
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
            const nv::gpio::InterruptDetection irq_det = InterruptDetection_from(req.irq_type);
            auto gpio_status = nv::gpio::Driver::init_interrupt(port, pin, irq_det, irq_sel);
            if (LstpStatus_from(gpio_status) != LstpStatus::Success) {
                send_resp(0, LstpStatus_from(gpio_status));
                return;
            }
            _irq_config.at(req.gpio_index) = req.irq_type;
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
    nv::gpio::Driver::read(pin_info.port, pin_info.pin, value);

    LstpGpioIrqEventRequest req{};
    req.gpio_index = gpio_idx;
    req.value      = static_cast<LstpGpioState>(value);

    auto item   = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&req), sizeof(req));
    auto status = nv::ipc::Queue::make(nv::ipc::QueueId::LstpGpioIrq).send_isr(item);
    if (status != nv::ipc::Queue::Status::Ok) {
        return;  // queue full, drop event
    }

    auto& event = nv::ipc::Event::make(nv::ipc::EventId::Lstp);
    (void)event.set(GpioIrqBit);
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
        if (irq_queue.recv(item, std::chrono::microseconds(0)) != nv::ipc::Queue::Status::Ok) {
            break;
        }
        if (event.value == LstpGpioState::High
            && !(_irq_config.at(event.gpio_index) == LstpGpioIrqConfig::Rising
                 || _irq_config.at(event.gpio_index) == LstpGpioIrqConfig::BothEdge
                 || _irq_config.at(event.gpio_index) == LstpGpioIrqConfig::High)) {
            continue;
        }
        if (event.value == LstpGpioState::Low
            && !(_irq_config.at(event.gpio_index) == LstpGpioIrqConfig::Falling
                 || _irq_config.at(event.gpio_index) == LstpGpioIrqConfig::BothEdge
                 || _irq_config.at(event.gpio_index) == LstpGpioIrqConfig::Low)) {
            continue;
        }
        LstpRouter::send_gpio_irq_event(
            ch_id, event.gpio_index, static_cast<uint8_t>(event.value));
    }
}

/*****************************************************/

void LstpTask::LstpGpioInit()
{
    using GpioDriver = nv::gpio::Driver;
    using nv::gpio::Direction;
    using nv::gpio::GpioOpenDrain;
    using nv::gpio::GpioPullDir;
    using nv::gpio::GpioPullStrength;
    using nv::gpio::GpioState;

    /* Initialize GPIOs from config */
    for (size_t i = 0; i < LstpGpioNum; ++i) {
        const auto& [port, pin, pin_config, _] = PinConfigs.at(i);
        (void)_;
        auto direction = (pin_config.direction == LstpGpioDirection::Output) ? Direction::Output
                                                                             : Direction::Input;
        auto state     = (pin_config.default_output == LstpGpioState::High) ? GpioState::High
                                                                            : GpioState::Low;
        GpioPullDir pull_dir = GpioPullDir::Disabled;
        switch (pin_config.bias_pull_config) {
            case LstpGpioBiasPullConfig::PullUp  : pull_dir = GpioPullDir::PullUp; break;
            case LstpGpioBiasPullConfig::PullDown: pull_dir = GpioPullDir::PullDown; break;
            case LstpGpioBiasPullConfig::NoPull  :
            default                              : pull_dir = GpioPullDir::Disabled; break;
        }
        auto pull_strength = (pin_config.bias_pull_strength > 0) ? GpioPullStrength::High
                                                                 : GpioPullStrength::Low;
        auto open_drain    = (pin_config.output_drive_config
                           == LstpGpioOutputDriveConfig::OpenDrain)
                               ? GpioOpenDrain::Enable
                               : GpioOpenDrain::Disable;

        GpioDriver::init_pin(port, pin, direction, state);
        GpioDriver::init_pin_cfg(port, pin, pull_dir, pull_strength, open_drain);
    }
}
}  // namespace nv::lstp

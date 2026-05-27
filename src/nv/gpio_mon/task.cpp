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

#include "nv/gpio_mon/task.h"

#include <algorithm>
#include <chrono>
#include <cstring>

#include <FreeRTOS.h>
#include <task.h>

#include "nv/bootloader.h"
#include "nv/common/preproc.h"
#include "nv/ctimer/ctimer.h"
#include "nv/gpio/driver.h"
#include "nv/ipc/event.h"
#include "nv/ipc/timer.h"
#include "sys/usb/usb.h"

#include NV_IPC_CONFIG_H

namespace nv::gpio_mon {

using namespace std::chrono_literals;

namespace {

// All monitored pins are routed to GPIO interrupt Channel 0
constexpr nv::gpio::InterruptSelect
                  MonitorChannel = nv::gpio::InterruptSelect::InterruptSelect0;
constexpr uint8_t PinsPerBank    = 32;

void on_timer_tick(nv::ipc::Timer& /*t*/)
{
    auto& event = nv::ipc::Event::make(nv::ipc::EventId::GpioMonEvent);
    (void)event.set(ScanTickBit);
}

template<typename T>
void append_to(uint8_t*& dest, const T& value)
{
    std::memcpy(dest, &value, sizeof(value));
    dest += sizeof(value);
}

}  // namespace

Task& Task::inst()
{
    NV_TASK_DATA static Task task;
    return task;
}

Task::Task() : ipc::Task(ipc::TaskId::GpioMon, "GpioMon") {}

void Task::make()
{
    auto&          task      = inst();
    constexpr auto StackSize = std::max(1024, int(configMINIMAL_STACK_SIZE));
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;
    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));
    task.setup(stack.span(), Priv, Priority::Norm, Task::entrypoint);
}

void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<Task*>(params);
    task.main();
}

// Strong overrides of weak hooks declared in sys/mcxn556/sys/usb/usb.cpp.
// Only emit them in builds that actually wire gpio_mon to the USB VCOM endpoint;
// to avoid collisions with other overrides (e.g. in the testrunner build, which compiles every
// src/nv/*.cpp).
#if defined(USB_CONFIG_GPIO_MONITOR)
extern "C" uint8_t nv_usb_vcom_rx(uint8_t* data, uint32_t length)
{
    return Task::usb_rx_callback(data, length);
}

extern "C" void nv_usb_vcom_close()
{
    Task::usb_close_callback();
}
#endif  // USB_CONFIG_GPIO_MONITOR

[[noreturn]] void Task::main()
{
    auto& timer = nv::ipc::Timer::make(nv::ipc::TimerId::GpioMonScan,
                                       std::chrono::milliseconds(ScanPeriodMs),
                                       on_timer_tick,
                                       true);
    (void)timer.start();

    auto& event = nv::ipc::Event::make(nv::ipc::EventId::GpioMonEvent);

    nv::bootloader::Driver::set_task_booted(nv::ipc::BootedEventBits::GpioMon);

    constexpr uint32_t WaitBits = ScanTickBit | UsbRxBit | UsbCloseBit;

    while (true) {
        auto bits = event.wait(WaitBits, false, false, 100ms);
        if (!bits.has_value()) {
            continue;
        }
        const uint32_t active = bits.value() & WaitBits;

        // Close before Rx so a fast close→reopen leaves a clean slate
        // before the new init-mask is applied.
        if (active & UsbCloseBit) {
            event.clear(UsbCloseBit);
            disable_all_triggers();
        }

        if (active & UsbRxBit) {
            event.clear(UsbRxBit);
            handle_usb_rx();
        }

        if (active & ScanTickBit) {
            event.clear(ScanTickBit);
            on_scan_tick();
        }
    }
}

void Task::on_scan_tick()
{
    if (!triggers_enabled) {
        // Buffers were already drained when triggers_enabled was cleared
        return;
    }

    // Each frame at tick K:
    //   bytes [0, FullIoDumpRecordSize)             -> FullIoDump(t = K)
    //   bytes [FullIoDumpRecordSize, frozen.offset) -> BankInt records, t ∈ (K-100ms, K)
    // Steps: swap -> write full io dump into frozen -> send -> reset offset.
    const uint8_t old_idx = active_idx;
    const uint8_t new_idx = old_idx ^ 1U;

    taskENTER_CRITICAL();
    active_idx = new_idx;
    taskEXIT_CRITICAL();

    Buffer& frozen = buffers.at(old_idx);

    write_full_io_dump(frozen);

    if (sys::usb::Driver::is_vcom_ready()) {
        void*              handle       = sys::usb::Driver::get_vcom_handle();
        constexpr int      MaxRetries   = 5;
        constexpr uint32_t RetryDelayUs = 20;
        for (int retry = 0; retry < MaxRetries; ++retry) {
            if (!sys::usb::Driver::vcom_send(handle, frozen.data.data(), frozen.offset)) {
                break;
            }
            nv::ctimer::Driver::delay_for_us(RetryDelayUs);
        }
    }
    frozen.offset = FullIoDumpRecordSize;
}

void Task::write_full_io_dump(Buffer& buf)
{
    // Capture timestamp + all bank PDIRs atomically wrt GPIO ISRs and lay the
    // record into the buffer's reserved head bytes [0 .. FullIoDumpRecordSize).
    taskENTER_CRITICAL();
    const uint32_t                  ts = nv::ctimer::Driver::read_ticks_inline();
    std::array<uint32_t, BankCount> snap{};
    uint8_t                         bank = 0;
    for (auto& port_val : snap) {
        (void)nv::gpio::Driver::read_gpio_port(bank++, port_val);
    }
    taskEXIT_CRITICAL();

    uint8_t*   dest   = buf.data.data();
    const auto header = static_cast<uint32_t>(PacketHeader::FullIoDump);
    append_to(dest, header);
    append_to(dest, ts);
    append_to(dest, snap);
}

bool Task::append_record_isr(uint32_t header, uint32_t bank_io_val)
{
    Buffer& buf = buffers.at(active_idx);
    if (buf.offset + BankInterruptDumpRecordSize > BufferSize) {
        return false;
    }
    const uint32_t ts   = nv::ctimer::Driver::read_ticks_inline();
    uint8_t*       dest = buf.data.data() + buf.offset;
    append_to(dest, header);
    append_to(dest, ts);
    append_to(dest, bank_io_val);
    buf.offset += BankInterruptDumpRecordSize;
    return true;
}

void Task::on_gpio_interrupt(uint8_t bank, uint32_t bank_io_val)
{
    Task& self = inst();
    if (!self.triggers_enabled || bank >= BankCount) {
        return;
    }
    (void)self.append_record_isr(bank_interrupt_header(bank), bank_io_val);
}

uint8_t Task::usb_rx_callback(uint8_t* data, uint32_t length)
{
    if (data == nullptr) {
        sys::usb::Driver::vcom_rearm_rx(sys::usb::Driver::get_vcom_handle(), data);
        return 0;
    }

    Task& self           = inst();
    self.pending_usb_len = std::min<std::size_t>(length, self.pending_usb_cmd.size());
    std::memcpy(self.pending_usb_cmd.data(), data, self.pending_usb_len);

    auto& event = nv::ipc::Event::make(nv::ipc::EventId::GpioMonEvent);
    (void)event.set(UsbRxBit);

    sys::usb::Driver::vcom_rearm_rx(sys::usb::Driver::get_vcom_handle(), data);
    return 0;
}

void Task::usb_close_callback()
{
    auto& event = nv::ipc::Event::make(nv::ipc::EventId::GpioMonEvent);
    (void)event.set(UsbCloseBit);
}

void Task::handle_usb_rx()
{
    std::array<uint8_t, InitMaskCmdSize> cmd{};

    taskENTER_CRITICAL();
    const std::size_t length = pending_usb_len;
    std::memcpy(cmd.data(), pending_usb_cmd.data(), length);
    pending_usb_len = 0;
    taskEXIT_CRITICAL();

    if (length >= InitMaskCmdSize) {
        uint32_t header = 0;
        std::memcpy(&header, cmd.data(), sizeof(header));
        if (header == static_cast<uint32_t>(PacketHeader::InitGpiTriggerMask)) {
            std::memcpy(pending_mask.data(),
                        cmd.data() + sizeof(header),
                        pending_mask.size() * sizeof(uint32_t));
            apply_pending_mask();
        }
    }
}

void Task::apply_pending_mask()
{
    using nv::gpio::Driver;
    using nv::gpio::InterruptDetection;

    const auto snapshot = pending_mask;

    triggers_enabled = false;  // pause ISR appends during reconfig

    // Only touch pins whose state actually changes
    for (uint8_t bank = 0; bank < BankCount; ++bank) {
        const uint32_t active   = active_mask.at(bank);
        const uint32_t pending  = snapshot.at(bank);
        const uint32_t to_clear = active & ~pending;
        const uint32_t to_set   = ~active & pending;
        for (uint8_t pin = 0; pin < PinsPerBank; ++pin) {
            if ((to_clear >> pin) & 1U) {
                (void)Driver::init_interrupt(
                    bank, pin, InterruptDetection::InterruptDisabled, MonitorChannel);
            }
            if ((to_set >> pin) & 1U) {
                (void)Driver::init_interrupt(
                    bank, pin, InterruptDetection::InterruptBothEdge, MonitorChannel);
            }
        }
    }
    active_mask      = snapshot;
    triggers_enabled = std::any_of(
        active_mask.begin(), active_mask.end(), [](uint32_t v) { return v != 0; });

    if (!triggers_enabled) {
        drop_pending_records();
    }
}

void Task::disable_all_triggers()
{
    using nv::gpio::Driver;
    using nv::gpio::InterruptDetection;
    triggers_enabled = false;
    for (uint8_t bank = 0; bank < BankCount; ++bank) {
        const uint32_t active = active_mask.at(bank);
        for (uint8_t pin = 0; pin < PinsPerBank; ++pin) {
            if ((active >> pin) & 1U) {
                (void)Driver::init_interrupt(
                    bank, pin, InterruptDetection::InterruptDisabled, MonitorChannel);
            }
        }
    }
    active_mask  = {};
    pending_mask = {};
    drop_pending_records();
}

void Task::drop_pending_records()
{
    taskENTER_CRITICAL();
    buffers[0].offset = FullIoDumpRecordSize;
    buffers[1].offset = FullIoDumpRecordSize;
    taskEXIT_CRITICAL();
}

}  // namespace nv::gpio_mon

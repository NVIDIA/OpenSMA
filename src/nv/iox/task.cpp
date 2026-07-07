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
#include "nv/common/debug.h"
#include "nv/gpio/driver.h"
#include "nv/iox/task.h"

#include <bit>
#include <cstdint>
#include <span>
#include <climits>
#include <cstring>
#include <algorithm>

#include "nv/nv.h"
#include "nv/logger/log.h"
#include "nv/i2c/error_injection.h"
#include "nv/mctp/nsm.h"
#include "nv/usb/task.h"
#include "sys/i2c/utils.h"
#include "nv/iox/common.h"

namespace nv::iox {

void Task::make()
{
    constexpr auto           StackSize = std::max(480, int(configMINIMAL_STACK_SIZE));
    NV_TASK_DATA static Task task;
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;

    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));
    task.setup(stack.span(), Priv, Priority::IOX, Task::entrypoint);

    nv::info("Iox task created\n");
}

void Task::entrypoint(void* params)
{
    nv::info("Iox task entrypoint\n");
    NV_ASSERT(params != nullptr);
    auto& task = *static_cast<Task*>(params);
    task.main();
}

Task::Task() : ipc::Task(ipc::TaskId::Iox, "Iox"), ioexp()
{
    const uint8_t val = 0xFF;
    Iox::set_gpio_value(val);
    // coverity[unsigned_compare] - IoxNum is not 0 once compiled
    for (size_t i = 0; i < IoxNum; ++i) {
        // coverity[dead_error_line] - IoxNum is not 0 once compiled
        const auto& config = nv::ipc::IoxConfigs.at(i);
        ioexp.at(i)        = Iox(config.addr, config.pinConfig);
        for (const auto& pinConfig : config.pinConfig) {
            // vrPort and unUsed have the same value
            if (pinConfig.port != nv::gpio::vrPort) {
                Iox::set_ioxn(pinConfig.port, pinConfig.pin, i);
            }
        }
    }
    sync_virtual_gpio_shadow();
}

[[noreturn]] void Task::main()
{
    using namespace nv::ipc;
    Request request{};
    auto    item = Queue::Item(std::bit_cast<uint8_t*>(&request), sizeof(request));

    auto& queue = ipc::Queue::make(ipc::QueueId::Iox);

    nv::bootloader::Driver::set_task_booted(nv::ipc::BootedEventBits::Iox);

    while (true) {
        auto status = queue.recv(item);
        if (status != Queue::Status::Ok) {
            nv::error("Iox queue recv failed\n");
            continue;
        }
        switch (request.type) {
            case RequestType::I2cRequest:
                handle_i2c_request(std::span<uint8_t>(request.data));
                break;
            case RequestType::VrGpioRequest:
                handle_vrgpio_request(std::span<uint8_t>(request.data));
                break;
            case RequestType::GpioSpoofingUpdate:
                handle_gpio_spoofing_update(std::span<uint8_t>(request.data));
                break;
            case RequestType::FilterUpdate:
                handle_filter_update(std::span<uint8_t>(request.data));
                break;
            default:
                nv::error("Iox: Unknown request type %d\n", static_cast<int>(request.type));
                break;
        }
    }
}

bool Task::send_i2c_request(ipchandler::Id    src_id,
                            uint8_t           address,
                            ipchandler::Id    i2c_ipchandler_id,
                            uint8_t           write_length,
                            uint16_t          read_length,
                            ipc::Queue::Item& item)
{
    // Check for queue full error injection
    if constexpr (nv::ipc::EnableI2CErrorInjection) {
        if (nv::i2c::should_inject_error(
                i2c_ipchandler_id,
                address,
                0,
                static_cast<uint8_t>(nv::i2c::ProtocolType::I2cPca9555))) {
            // Check if this is a queue full error injection
            const auto port  = static_cast<nv::i2c::Port>(address - nv::iox::Task::IoxBaseAddr);
            const auto index = nv::i2c::port_to_error_injection_index(
                port, static_cast<uint8_t>(nv::i2c::ProtocolType::I2cPca9555));
            const auto& configs = nv::i2c::get_error_injection_configs();
            if (index < configs.size()
                && configs.at(index).error_type
                       == static_cast<uint8_t>(nv::i2c::ErrorInjectionType::QueueFull)) {
                nv::info("I2C queue full error injected for port %d (virtual)\n",
                         static_cast<int>(port));
                return false;  // Simulate queue full by returning false
            }
        }
    }
    (void)i2c_ipchandler_id;
    // Create request
    Task::Request request{};
    request.type = Task::RequestType::I2cRequest;

    // Set up I2C request data
    auto* i2c_request = std::bit_cast<nv::i2c::I2cRequest*>(static_cast<void*>(request.data));
    i2c_request->address      = address;
    i2c_request->write_length = write_length;
    i2c_request->read_length  = read_length;
    i2c_request->src_id       = static_cast<uint8_t>(src_id);

    // Copy write data if any
    if (write_length > 0) {
        if (write_length > i2c_request->write_buffer.size()) {
            nv::info("Write length %d exceeds buffer size %d\n",
                     write_length,
                     i2c_request->write_buffer.size());
            return false;
        }
        std::copy_n(item.begin(), write_length, i2c_request->write_buffer.begin());
    }

    // Create queue item with the full request size
    auto request_item = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&request),
                                             sizeof(Task::Request));

    nv::ipc::Queue& queue = nv::ipc::Queue::make(nv::ipc::QueueId::Iox);

    // Send to IPC handler
    auto status = queue.send(request_item);

    if (status != nv::ipc::Queue::Status::Ok) {
        nv::info("Failed to send virtual I2C request (%d)\n", status);
        return false;
    }

    return true;
}

bool Task::send_vrgpio_request(uint8_t                  address,
                               Operation                operation,
                               std::span<const uint8_t> pins,
                               std::span<const uint8_t> vals,
                               bool                     trigger_nsm_event)
{
    // Create request
    Task::Request request{};
    request.type        = Task::RequestType::VrGpioRequest;
    auto* vrGpioRequest = std::bit_cast<Task::VrGpioRequest*>(static_cast<void*>(request.data));

    // CodeRabbit's suggestion
    if (pins.size() == 0 || pins.size() > 16) {
        nv::error("Invalid pin count: %d\n", pins.size());
        return false;
    }
    if (operation == Operation::Write && pins.size() != vals.size()) {
        nv::error("Pin count mismatch for write operation\n");
        return false;
    }
    if (operation == Operation::Read && vals.size() != 0) {
        nv::error("No values shall be provided for read operation\n");
        return false;
    }

    // copy address, pins and vals to request
    vrGpioRequest->ioxAddr = address;
    vrGpioRequest->ioxOper = static_cast<uint8_t>(operation);
    // coverity[cert_int31_c_violation] - no data lost
    vrGpioRequest->pinSize = static_cast<uint8_t>(pins.size());

    memcpy(&vrGpioRequest->pinVals[0], pins.data(), pins.size());
    memcpy(&vrGpioRequest->pinVals[0] + pins.size(), vals.data(), vals.size());

    if (trigger_nsm_event) {
        nv::mctp::Nsm::VirtualGpioEventTrigger(pins, vals);
    }

    // Create queue item with the full request size
    auto request_item = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&request),
                                             sizeof(Task::Request));

    nv::ipc::Queue& queue  = nv::ipc::Queue::make(nv::ipc::QueueId::Iox);
    auto            status = (sys::ipc::is_in_isr() ? queue.send_isr(request_item)
                                                    : queue.send(request_item));

    if (status != nv::ipc::Queue::Status::Ok) {
        nv::error("Failed to send virtual GPIO request with err=%d\n", status);
        return false;
    }

    return true;
}

void Task::handle_vrgpio_request(std::span<uint8_t> buffer)
{
    if constexpr (IoxNum == 0) {
        return;
    }

    using namespace nv::iox;
    Task::Request request{};
    auto* vrGpioRequest = std::bit_cast<Task::VrGpioRequest*>(static_cast<void*>(request.data));
    std::memcpy(vrGpioRequest, buffer.data(), buffer.size());

    // extract address, number of pins, pins and vals
    const auto addr = vrGpioRequest->ioxAddr;
    const auto npin = vrGpioRequest->pinSize;
    const auto pins = std::span<uint8_t>(&vrGpioRequest->pinVals[0], npin);
    const auto vals = std::span<uint8_t>(&vrGpioRequest->pinVals[0] + npin, npin);

    // check if address is valid
    const uint8_t offset = (addr >= IoxBaseAddr) ? (addr - IoxBaseAddr) : 0xff;
    // coverity[unsigned_compare] - IoxNum is not 0 once compiled
    if (offset >= IoxNum) {
        nv::error("Invalid address addr=%d, offset=%d\n", addr, offset);
        return;
    }

    // read target iox's gpio values
    uint8_t lval = 0, hval = 0;
    ioexp.at(offset).read_reg(Register::InputPort0, lval, offset);
    ioexp.at(offset).read_reg(Register::InputPort1, hval, offset);

    uint32_t val = (static_cast<uint32_t>(hval) << 8) | static_cast<uint32_t>(lval);

    // extract operation
    auto operation = static_cast<Operation>(vrGpioRequest->ioxOper);

    /**
     * if operation is read, return the gpio values to user
     *
     * @note: so far no MCU tasks need to read virtual GPIO values
     *        so for simplicity, we just return here
     */
    if (operation == Operation::Read) {
        return;
    }

    // assemble pins and vals
    for (size_t i = 0; i < npin; i++) {
        const auto pin = pins[i];
        const auto lvl = vals[i] & 0x01;

        constexpr uint8_t maxPin = 15;
        if (pin > maxPin) {
            nv::error("Invalid virtualpin %d\n", pin);
            return;
        }

        val &= ~(1 << pin);
        val |= (lvl << pin);
    }

    // write to target iox's gpio values
    constexpr uint8_t mask = 0xff;
    if (nv::iox::Status::Ok
        != ioexp.at(offset).write_reg(
            Register::OutputPort0, static_cast<uint8_t>((val >> 0) & mask), offset)) {
        nv::error("Iox Write virtual GPIO failed\n");
        return;
    }
    if (nv::iox::Status::Ok
        != ioexp.at(offset).write_reg(
            Register::OutputPort1, static_cast<uint8_t>((val >> 8) & mask), offset)) {
        nv::error("Iox Write virtual GPIO failed\n");
        return;
    }
    sync_virtual_gpio_shadow();
}

void Task::handle_i2c_request(std::span<uint8_t> buffer)
{
    if constexpr (IoxNum == 0) {
        return;
    }

    using namespace nv::i2c;
    nv::i2c::I2cRequest request{};
    const size_t        RequestSize = std::min(sizeof(nv::i2c::I2cRequest), buffer.size());

    std::memcpy(&request, buffer.data(), RequestSize);
    const size_t WriteLength = std::min(static_cast<size_t>(request.write_length),
                                        request.write_buffer.size());
    const size_t ReadLength  = std::min(static_cast<size_t>(request.read_length),
                                       read_buffer.size());

    auto          status = nv::iox::Status::Ok;
    const uint8_t offset = ((request.address >= IoxBaseAddr) ? (request.address - IoxBaseAddr)
                                                             : 0xff);

    if constexpr (nv::ipc::EnableI2CErrorInjection) {
        if (nv::i2c::should_inject_error(
                static_cast<nv::i2c::Port>(offset), request.address, 0, 0x05)) {
            auto injected_status = nv::i2c::get_injected_error_status(
                static_cast<nv::i2c::Port>(offset), 0x05);

            // Send the error response back to USB if needed
            if (request.src_id == static_cast<uint8_t>(ipchandler::Id::Usb)) {
                // Check if this is USB queue full injection (Busy status)
                if (injected_status == nv::i2c::I2cStatus::Busy) {
                    // Skip USB call to simulate queue full
                    nv::info(
                        "IOX: USB queue full simulation - skipping to_usb call for port %d\n",
                        static_cast<uint8_t>(offset));
                    return;
                }

                std::span<uint8_t> item(read_buffer.data(), read_buffer.size());
                usb::Task::to_usb(
                    nv::ipchandler::Id::Iox, 0, item, static_cast<I2cStatus>(injected_status));
            }
            return;
        }
    }

    // coverity[unsigned_compare] - IoxNum is not 0 once compiled
    if ((offset >= IoxNum) || (WriteLength == 0 && ReadLength == 0)
        || (WriteLength > static_cast<size_t>(Register::Invalid))
        || (ReadLength > static_cast<size_t>(Register::Invalid))) {
        status = nv::iox::Status::Error;
    }
    else {
        const auto reg = static_cast<Register>(request.write_buffer.at(0));

        if (status == nv::iox::Status::Ok && WriteLength > 1) {  // single or bulk write
            for (size_t i = 0; i < (WriteLength - 1); ++i) {
                const auto data = request.write_buffer.at(i + 1);
                const auto regn = static_cast<uint8_t>(reg) + i;
                status = ioexp.at(offset).write_reg(static_cast<Register>(regn), data, offset);

                if (status != nv::iox::Status::Ok) {
                    nv::error("Iox Write failed\n");
                    break;
                }
            }
        }

        if (status == nv::iox::Status::Ok && ReadLength > 0) {  // single or bulk read
            for (size_t i = 0; i < ReadLength; ++i) {
                const auto regn = static_cast<uint8_t>(reg) + i;
                status          = ioexp.at(offset).read_reg(
                    static_cast<Register>(regn), read_buffer.at(i), offset);

                if (status != nv::iox::Status::Ok) {
                    nv::error("Iox Read failed\n");
                    break;
                }

                if (regn == static_cast<uint8_t>(Register::InputPort0)
                    || regn == static_cast<uint8_t>(Register::InputPort1)) {
                    apply_spoofing_to_input_port(offset, regn, read_buffer.at(i));
                }

                if (filter_en
                    && (regn == static_cast<uint8_t>(Register::InputPort0)
                        || regn == static_cast<uint8_t>(Register::InputPort1))) {
                    for (uint8_t bit = 0; bit < 8; ++bit) {
                        const auto& config = nv::ipc::IoxConfigs.at(offset).pinConfig.at(
                            regn * 8 + bit);
                        if (config.filter == FilterEnable::Enable) {
                            read_buffer.at(i) &= ~(1U << bit);
                            read_buffer.at(i) |= static_cast<uint8_t>(config.defaultVal) << bit;
                        }
                    }
                }
            }
        }
    }

    if (status != nv::iox::Status::Ok) {
        nv::error("Iox Error: Iox=%d, WriteLength=%d, ReadLength=%d, status=%d\n",
                  offset,
                  WriteLength,
                  ReadLength,
                  static_cast<uint8_t>(status));
    }
    else {
        sync_virtual_gpio_shadow();
    }

    if (request.src_id == static_cast<uint8_t>(ipchandler::Id::Usb)) {
        std::span<uint8_t> item(read_buffer.data(), read_buffer.size());
        usb::Task::to_usb(
            nv::ipchandler::Id::Iox, ReadLength, item, static_cast<I2cStatus>(status));
    }
}

void Task::handle_gpio_spoofing_update(std::span<uint8_t> buffer)
{
    auto* update = std::bit_cast<Task::GpioSpoofingUpdate*>(buffer.data());

    spoofingActive  = update->spoofingActive;
    numSpoofEntries = update->numEntries;
    for (uint8_t i = 0; i < update->numEntries && i < spoofEntries.size(); i++) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        spoofEntries.at(i).gpioIndex = update->entries[i].gpioIndex;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        spoofEntries.at(i).activated = update->entries[i].activated;
    }

    sync_virtual_gpio_shadow();
}

void Task::apply_spoofing_to_input_port(uint8_t iox_offset, uint8_t regn, uint8_t& data)
{
    if (!spoofingActive) {
        return;
    }

    const uint8_t pin_offset = (regn == static_cast<uint8_t>(Register::InputPort0)) ? 0U : 8U;

    for (uint8_t bit = 0; bit < 8; ++bit) {
        const auto& pin_cfg = nv::ipc::IoxConfigs.at(iox_offset).pinConfig.at(pin_offset + bit);
        const auto  port    = pin_cfg.port;
        const auto  pin     = pin_cfg.pin;

        // Only input GPIOs and virtual GPIOs are eligible for spoofing
        if (port != nv::gpio::vrPort) {
            nv::gpio::Direction dir = nv::gpio::Direction::Input;
            if (nv::gpio::Driver::getDirection(port, pin, dir) != nv::gpio::Status::Ok
                || dir != nv::gpio::Direction::Input) {
                continue;
            }
        }

        // Find the GPIO index in GpioSetup
        uint16_t gpio_index = UINT16_MAX;
        for (uint16_t g = 0; g < nv::ipc::GpioNum; ++g) {
            if (std::get<0>(nv::ipc::GpioSetup.at(g)) == port
                && std::get<1>(nv::ipc::GpioSetup.at(g)) == pin) {
                gpio_index = g;
                break;
            }
        }

        const uint8_t default_val = (pin_cfg.val == nv::gpio::GpioState::Low) ? 0U : 1U;

        bool spoofed = false;
        for (uint8_t i = 0; i < numSpoofEntries && i < spoofEntries.size(); ++i) {
            if (spoofEntries.at(i).activated && spoofEntries.at(i).gpioIndex == gpio_index) {
                data    = (data & ~(1U << bit)) | (((default_val ^ 1U) & 0x01U) << bit);
                spoofed = true;
                break;
            }
        }

        if (!spoofed) {
            // Spoofing active but GPIO not in list — return default value
            data = (data & ~(1U << bit)) | (default_val << bit);
        }
    }
}

bool Task::send_gpio_spoofing_update(bool                           spoofingActive,
                                     nv::mctp::GPIOSpoofingPayload& updateData)
{
    Task::Request request{};
    request.type = Task::RequestType::GpioSpoofingUpdate;

    // NOLINTNEXTLINE(*-reinterpret-cast)
    auto* update           = reinterpret_cast<Task::GpioSpoofingUpdate*>(request.data);
    update->spoofingActive = spoofingActive;
    update->numEntries     = updateData.gpio_spoofing_header.ei_gpio_number;

    // Copy entries from GPIOSpoofingPayload to Task::GpioSpoofingUpdate format
    const uint8_t numEntries = std::min(static_cast<uint8_t>(update->numEntries),
                                        static_cast<uint8_t>(nv::mctp::MaxGPIOSpoofingEntries));
    for (uint8_t i = 0; i < numEntries; i++) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        update->entries[i].gpioIndex = updateData.gpio_ei_entries[i].gpioIndex;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        update->entries[i].activated = updateData.gpio_ei_entries[i].activated;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        update->entries[i].reserved = 0;
    }

    auto request_item = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&request),
                                             sizeof(Task::Request));

    nv::ipc::Queue& queue  = nv::ipc::Queue::make(nv::ipc::QueueId::Iox);
    auto            status = queue.send(request_item);
    if (status != nv::ipc::Queue::Status::Ok) {
        return false;
    }

    return true;
}

void Task::sync_virtual_gpio_shadow()
{
    if constexpr (IoxNum == 0) {
        return;
    }

    for (size_t i = 0; i < IoxNum; ++i) {
        const auto& cfg = nv::ipc::IoxConfigs.at(i);
        for (uint8_t j = 0; j < pinNum; ++j) {
            const auto& pc = cfg.pinConfig.at(j);
            if (pc.port != nv::gpio::vrPort) {
                continue;
            }

            const Register reg = (j < 8U) ? Register::InputPort0 : Register::InputPort1;
            const uint8_t  bit = (j < 8U) ? j : static_cast<uint8_t>(j - 8U);
            uint8_t        val = 0;
            if (ioexp.at(i).read_reg(reg, val, static_cast<uint8_t>(i))
                != nv::iox::Status::Ok) {
                continue;
            }
            const auto level = static_cast<uint8_t>((val >> bit) & 0x01U);
            (void)nv::gpio::Driver::write_virtual_physical_gpio(pc.port, pc.pin, level);
        }
    }
}

bool Task::send_filter_update(bool enable)
{
    Task::Request request{};
    request.type = Task::RequestType::FilterUpdate;

    auto* payload = std::bit_cast<Task::FilterUpdateRequest*>(static_cast<void*>(request.data));
    payload->filterEnable = enable;

    auto request_item = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&request),
                                             sizeof(Task::Request));

    nv::ipc::Queue& queue  = nv::ipc::Queue::make(nv::ipc::QueueId::Iox);
    auto            status = (sys::ipc::is_in_isr() ? queue.send_isr(request_item)
                                                    : queue.send(request_item));
    if (status != nv::ipc::Queue::Status::Ok) {
        return false;
    }

    return true;
}

void Task::handle_filter_update(std::span<uint8_t> buffer)
{
    auto* payload = std::bit_cast<Task::FilterUpdateRequest*>(
        static_cast<void*>(buffer.data()));
    filter_en = payload->filterEnable;
}

}  // namespace nv::iox

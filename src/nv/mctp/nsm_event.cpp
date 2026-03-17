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

#include "nv/mctp/nsm_event.h"

#include <cstring>
#include <limits>
#include <span>

#include "nv/gpio/driver.h"
#include "nv/logger/common.h"
#include "nv/mctp/driver.h"
#include "nv/mctp/enums.h"
#include "nv/mctp/nsm.h"
#include "sys/ipc/driver.h"

namespace nv::mctp::nsm_event {

bool PrepareEventMessage(uint8_t                       nsmType,
                         uint8_t                       eventId,
                         const nv::mctp::MctpCmdData3& eventInfo,
                         nv::mctp::Nsm&                nsm,
                         nv::mctp::Client&             client,
                         nv::mctp::Packet&             event_msg)
{
    using namespace nv::mctp;

    // Validate nsmType before casting to enum
    if (nsmType >= static_cast<uint8_t>(NsmMsgType::Reserved)) {
        nv::error("Invalid NSM message type: %u\n", nsmType);
        return false;
    }

    const auto nsm_msg_type = static_cast<NsmMsgType>(nsmType);
    const auto event_id     = static_cast<uint8_t>(eventId);

    // Event not enable
    if (!nsm.is_event_source_enable(nsm_msg_type, event_id)) {
        nv::info("event not enable\n");
        return false;
    }

    // The global event generation setting is not PUSH
    if (!nsm.is_global_event_setting_push()) {
        return false;
    }

    nv::info("on_receive_event: msgType %d, eventId %d\n",
             static_cast<uint8_t>(nsm_msg_type),
             event_id);

    const EventLog event_log{};

    // Fill event message header, keep event_log all zeros and will fill in the event handling
    // below.
    nsm.fill_event_msg(event_log, event_msg);

    auto& nsm_event_msg = NsmEventMsg::from(event_msg);

    // Fill event id, data size and payload
    switch (nsm_msg_type) {
        case NsmMsgType::DeviceCapabilityDiscovery:
            ProcessType0Event(event_id, nsm_event_msg, eventInfo);
            break;
        default:
            return false;
            break;
            // case NsmMsgType::Firmware :
            // on_type6_event(static_cast<NsmFwEvent>(eventMctpCmd.eventId)); break;
    }

    // Check for overflow when adding data_size to packet_length
    const auto current_packet_length = event_msg.priv.packet_length;
    const auto data_size             = nsm_event_msg.data_size;
    if (current_packet_length > UINT16_MAX - data_size) {
        return false;
    }
    // coverity[cert_int30_c_violation] - Safe to add since we checked for overflow above
    event_msg.priv.packet_length = static_cast<uint16_t>(current_packet_length + data_size);

    const bool ackEnable = nsm.is_event_ack_enable(nsm_msg_type, event_id);

    // Copy to the event message cache if ack is enabled
    if (ackEnable) {
        auto status = nsm.cache_event_msg(event_msg);
        if (!status) {
            nv::error("Failed to cache event message - no available slots\n");
            // TODO: Add MCU flash log for this error.
        }
    }

    client = static_cast<Client>(event_msg.priv.packet_interface);

    return true;
}

void ProcessType0Event(uint8_t                       eventId,
                       nv::mctp::NsmEventMsg&        nsm_event_msg,
                       const nv::mctp::MctpCmdData3& eventInfo)
{
    nsm_event_msg.event_id = eventId;

    // Handle GPIO event
    if (eventId == static_cast<uint8_t>(nv::mctp::NsmDcdEvent::GpioEvent)) {
        GpioEventInfo gpioInfo{};
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
        std::memcpy(&gpioInfo, eventInfo.data(), sizeof(GpioEventInfo));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        ProcessGpioEvent(nsm_event_msg, gpioInfo.port, gpioInfo.gpioEventBits);
    }
}

bool ProcessGpioEvent(nv::mctp::NsmEventMsg& nsm_event_msg,
                      nv::gpio::GpioPort     port,
                      uint32_t               eventData)
{
    // Constants for GPIO event processing
    constexpr uint8_t GpioEventPayloadHeaderSize = sizeof(NsmEventGpioPayloadHeader);

    T0GpioEventPayload gpio_event_payload = {};
    gpio_event_payload.header.timestampLo = sys::ipc::get_os_ticks();
    gpio_event_payload.header.timestampHi = 0;

    auto event_count = 0;

    // Iterate through configured GPIO NSM events
    for (const auto& gpio : nv::ipc::GpioNsmEventSetup) {
        // Check if this GPIO belongs to the specified port
        if (gpio.port == port) {
            // Check if the corresponding bit is set in eventData
            const uint8_t pin = gpio.pin;
            if (pin < sys::gpio::PinsPerPort && (eventData & (1u << pin))) {
                gpio_event_payload.gpio_event_entries.at(event_count)
                    .gpioIndex = gpio.gpioSetupIndex;
                gpio_event_payload.gpio_event_entries.at(event_count)
                    .gpioValue = static_cast<uint8_t>(gpio.gpioAssertedValue);
                ++event_count;
                nv::info("GPIO Event: index %d, port %d, pin %d, value %d\n",
                         gpio.gpioSetupIndex,
                         port,
                         pin,
                         static_cast<uint8_t>(gpio.gpioAssertedValue));
            }
        }
    }

    gpio_event_payload.header.gpio_event_num = event_count;

    nsm_event_msg.data_size = sizeof(GpioEventEntry) * event_count + GpioEventPayloadHeaderSize;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    std::memcpy(nsm_event_msg.data, &gpio_event_payload, nsm_event_msg.data_size);

    nv::info("GPIO Event: count %d\n", event_count);
    return true;
}

}  // namespace nv::mctp::nsm_event

// GPIO Event Trigger implementation
void nv::mctp::Nsm::GpioEventTrigger(nv::gpio::GpioPort port,
                                     uint32_t           flag,
                                     GpioEventSource    gpioSource)
{
    uint8_t portIndex = port;
    if (port == nv::iox::vrPort) {
        static_assert(nv::ipc::GpioNsmEventMask.size() == sys::gpio::PortsNumber + 1,
                      "GpioNsmEventSetup size must be equal to nv::gpio::PortsNumber");
        portIndex = sys::gpio::PortsNumber;
    }

    // Check GPIO asserted vs. GPIOs to fire event.
    if (flag & nv::ipc::GpioNsmEventMask.at(portIndex)) {
        uint32_t eventGpioBits = 0;
        uint32_t gpioValue     = 0;
        if (gpioSource == PhysicalGpio) {
            auto status = nv::gpio::Driver::read_gpio_port(port, gpioValue);
            if (status == nv::gpio::Status::Ok) {
                eventGpioBits = (~(gpioValue ^ nv::ipc::GpioNsmEventAssertMask.at(portIndex)))
                              & flag & nv::ipc::GpioNsmEventMask.at(portIndex);
            }
        }
        else if (gpioSource == SpoofingGpio) {
            eventGpioBits = flag & nv::ipc::GpioNsmEventMask.at(portIndex);
        }
        else if (gpioSource == VirtualGpio) {
            eventGpioBits = flag & nv::ipc::GpioNsmEventAssertMask.at(portIndex)
                          & nv::ipc::GpioNsmEventMask.at(portIndex);
        }
        else {
            return;
        }

        // TODO: remove this after integration with HMC
        if (eventGpioBits != 0) {
            nv::info(
                "GPIO Event Trigger: port %d, Isrflag 0x%x,  gpioValue 0x%x, "
                "gpioNsmEventAssertMask 0x%x, gpioNsmEventMask 0x%x, eventGpioBits 0x%x\n",
                portIndex,
                flag,
                gpioValue,
                nv::ipc::GpioNsmEventAssertMask.at(portIndex),
                nv::ipc::GpioNsmEventMask.at(portIndex),
                eventGpioBits);

            // Ensure GpioEventInfo fits in MctpCmdData3 buffer
            static_assert(sizeof(GpioEventInfo) <= sizeof(Driver::MctpCmdData3),
                          "GpioEventInfo size exceeds MctpCmdData3 buffer size");

            // Prepare event info and serialize to byte array
            GpioEventInfo        gpioEventInfo{.port          = static_cast<uint32_t>(port),
                                               .gpioEventBits = eventGpioBits};
            Driver::MctpCmdData3 data3{};
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
            std::memcpy(data3.data(), &gpioEventInfo, sizeof(GpioEventInfo));

            Driver::mctp_send_cmd(Driver::CmdCode::NsmEventCmd,
                                  static_cast<uint8_t>(NsmMsgType::DeviceCapabilityDiscovery),
                                  static_cast<uint8_t>(NsmDcdEvent::GpioEvent),
                                  true,
                                  data3);
        }
    }
}

bool nv::mctp::Nsm::VirtualGpioEventTrigger(std::span<const uint8_t> vpins,
                                            std::span<const uint8_t> vals)
{
    if (vpins.size() != vals.size()) {
        return false;
    }

    constexpr uint8_t bitmapBits = std::numeric_limits<uint32_t>::digits;
    uint32_t          bitmap     = 0;

    for (size_t i = 0; i < vpins.size(); ++i) {
        const uint8_t pin = vpins[i];
        const uint8_t val = vals[i];

        if (pin >= bitmapBits) {
            return false;
        }

        if (val > 1U) {
            return false;
        }

        const uint32_t mask = (1U << pin);
        if (val == 1U) {
            bitmap |= mask;
        }
    }

    GpioEventTrigger(nv::iox::vrPort, bitmap, VirtualGpio);
    return true;
}

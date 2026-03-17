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

// Standard library includes
#include <array>
#include <cstdint>
#include <tuple>

// Project includes
#include "nv/gpio/common.h"
#include "nv/iox/common.h"
#include "nv/mctp/interface.h"
#include "sys/gpio/constant.h"

// Forward declarations to avoid circular dependency
// (nsm.h depends on config.h, which includes nsm_event.h)
namespace nv::mctp {
class Nsm;
struct NsmEventMsg;
// Type alias for MCTP command data3 field (8-byte array)
// Note: This is also defined in driver.h for consistency
using MctpCmdData3 = std::array<uint8_t, 8>;
}  // namespace nv::mctp

namespace nv::ipc {

// Structure to contain GPIO entry with asserted polarity
struct [[gnu::packed]] NsmEventGpio
{
    gpio::GpioPort      port;               // GPIO port
    gpio::GpioPin       pin;                // GPIO pin
    nv::gpio::GpioState gpioAssertedValue;  // Asserted GPIO polarity value
    uint8_t             gpioSetupIndex;     // Index in GpioSetup array
};

// Constexpr helper function to find GPIO index in GpioSetup array at compile time
// If GPIO is not found, accessing out-of-bounds will cause compile-time error
template<typename GpioSetupArray>
constexpr uint8_t
FindGpioSetupIndex(const GpioSetupArray& gpioSetup, gpio::GpioPort port, gpio::GpioPin pin)
{
    size_t found_index = gpioSetup.size();  // Default to invalid index

    for (size_t i = 0; i < gpioSetup.size(); ++i) {
        if (std::get<0>(gpioSetup[i]) == port && std::get<1>(gpioSetup[i]) == pin) {
            found_index = i;
            break;
        }
    }

    // If not found (found_index == gpioSetup.size()), this will access out of bounds
    // and cause a compile-time error showing the port/pin values
    return found_index < gpioSetup.size()
             ? static_cast<uint8_t>(found_index)
             : std::get<0>(gpioSetup[found_index]);  // Intentional
                                                     // out-of-bounds
                                                     // access for
                                                     // compile
                                                     // error
}

// Constexpr helper function to populate GPIO mask array by scanning GpioNsmEventSetup
// Uses PORT as array index, PIN as bit position
template<typename NsmEventGpioArray>
constexpr auto PopulateGpioNsmEventMaskArray(const NsmEventGpioArray& gpioNsmEventSetup)
{
    constexpr uint8_t                    maxPortNum = sys::gpio::PortsNumber;
    std::array<uint32_t, maxPortNum + 1> masks{};

    // Loop through each GPIO in GpioNsmEventSetup using range-based for loop
    for (const auto& gpio : gpioNsmEventSetup) {
        gpio::GpioPort port = gpio.port;
        gpio::GpioPin  pin  = gpio.pin;

        if (port < maxPortNum) {
            masks[port] |= (1U << pin);  // Set bit at position 'pin'
        }
        if (port == nv::iox::vrPort) {
            masks[maxPortNum] |= (1U << pin);  // Set bit at position 'pin'
        }
    }

    return masks;
}

// Constexpr helper function to populate GPIO assert value mask array
// Uses PORT as array index, PIN as bit position, sets bit based on gpioAssertedValue
template<typename NsmEventGpioArray>
constexpr auto PopulateGpioNsmEventAssertMaskArray(const NsmEventGpioArray& gpioNsmEventSetup)
{
    constexpr uint8_t                    maxPortNum = sys::gpio::PortsNumber;
    std::array<uint32_t, maxPortNum + 1> masks{};

    // Loop through each GPIO in GpioNsmEventSetup using range-based for loop
    for (const auto& gpio : gpioNsmEventSetup) {
        gpio::GpioPort port        = gpio.port;
        gpio::GpioPin  pin         = gpio.pin;
        auto           assertValue = gpio.gpioAssertedValue;

        if (port < maxPortNum && assertValue == nv::gpio::GpioState::High) {
            masks[port] |= (1U << pin);  // Set bit at position 'pin' if gpioAssertedValue is
                                         // High
        }
        if (port == nv::iox::vrPort && assertValue == nv::gpio::GpioState::High) {
            masks[maxPortNum] |= (1U << pin);
        }
    }

    return masks;
}

}  // namespace nv::ipc

namespace nv::mctp {

// NSM Event related data structures

enum class NsmEventState : uint8_t
{
    available = 0,
    pending   = 1,
};

struct [[gnu::packed]] NsmEventCache
{
    NsmEventState eventState;
    uint8_t       rsvd[3];
    Packet        eventMsg;
};

struct [[gnu::packed]] NsmEventGpioPayloadHeader
{
    uint32_t timestampLo;  // nanoseconds
    uint32_t timestampHi;  // nanoseconds
    uint16_t gpio_event_num;
};

struct [[gnu::packed]] GpioEventEntry
{
    uint16_t gpioIndex : 10;
    uint16_t reserved  : 5;
    uint16_t gpioValue : 1;
};

struct [[gnu::packed]] T0GpioEventPayload
{
    NsmEventGpioPayloadHeader                          header;
    std::array<GpioEventEntry, sys::gpio::PinsPerPort> gpio_event_entries{};
};

struct [[gnu::packed]] GpioEventInfo
{
    uint32_t port;           // GPIO port number
    uint32_t gpioEventBits;  // GPIO event bitmap (32 bits)
};

}  // namespace nv::mctp

namespace nv::mctp::nsm_event {

/**
 * @brief Process and prepare event message for forwarding
 * @param nsmType NSM message type
 * @param eventId Event ID
 * @param eventInfo Event generation information (8-byte array)
 * @param nsm Reference to NSM instance
 * @param client Output parameter for client to forward the message to
 * @param event_msg Output parameter for the processed event message
 * @return true if event was successfully processed, false otherwise
 */
bool PrepareEventMessage(uint8_t                       nsmType,
                         uint8_t                       eventId,
                         const nv::mctp::MctpCmdData3& eventInfo,
                         nv::mctp::Nsm&                nsm,
                         nv::mctp::Client&             client,
                         nv::mctp::Packet&             event_msg);

/**
 * @brief Process Type 0 (Device Capability Discovery) event
 * @param eventId Event ID to process
 * @param nsm_event_msg Reference to NSM event message to populate
 * @param eventInfo Event generation information (8-byte array)
 */
void ProcessType0Event(uint8_t                       eventId,
                       nv::mctp::NsmEventMsg&        nsm_event_msg,
                       const nv::mctp::MctpCmdData3& eventInfo);

/**
 * @brief Process GPIO event and populate NSM event message
 * @param nsm_event_msg Reference to NSM event message to populate
 * @param port GPIO port number
 * @param eventData GPIO event data (bit mask of triggered pins)
 * @return true if event was processed successfully, false otherwise
 */
bool ProcessGpioEvent(nv::mctp::NsmEventMsg& nsm_event_msg,
                      nv::gpio::GpioPort     port,
                      uint32_t               eventData);

}  // namespace nv::mctp::nsm_event

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

#include "nv/ahs/hssm.h"
#include "nv/gpio/driver.h"

namespace nv::ahs {

/**
 * @brief Constructs a hot swap controller with initial state values
 *
 * Initializes the hot swap controller with the specified pin configuration,
 * initial status values, and hot swap event pointer. Sets up GPIO pins
 * and immediately updates the state machine with the initial conditions.
 *
 * @param pin_config Pin configuration for the drive
 * @param pgood Initial power good status
 * @param prsntL Initial presence detection status
 * @param amberLed Initial amber LED status
 * @param blueLed Initial blue LED status
 * @param hotSwapEvent Pointer to hot swap event for notifications
 * @param driveNum Drive number identifier
 */
E1sHotSwap::E1sHotSwap(nv::nhp::E1sOutputPins pin_config,
                       bool                   pgood,
                       bool                   prsntL,
                       bool                   amberLed,
                       bool                   blueLed,
                       nv::ipc::Event*        hotSwapEvent,
                       uint8_t                driveNum)
: state(DriveDisabled)
, pinout(pin_config)
, _hotSwapEvent(hotSwapEvent)
, _driveNum(driveNum)
{
    // Initialize all pins to their default state
    init_pins();

    // Update state machine with initial conditions
    updateStateMachine(pgood, prsntL, amberLed, blueLed);

    nv::info("Initialized drive %d hot swap controller.\n", _driveNum);
}

/**
 * @brief Constructs a hot swap controller with default initial state
 *
 * Initializes the hot swap controller with the specified pin configuration
 * and hot swap event pointer. Uses default initial state values (drive not
 * present, power off) and sets up GPIO pins accordingly.
 *
 * @param pin_config Pin configuration for the drive
 * @param hotSwapEvent Pointer to hot swap event for notifications
 * @param driveNum Drive number identifier
 */
E1sHotSwap::E1sHotSwap(nv::nhp::E1sOutputPins pin_config,
                       nv::ipc::Event*        hotSwapEvent,
                       uint8_t                driveNum)
: state(DriveDisabled)
, pinout(pin_config)
, _hotSwapEvent(hotSwapEvent)
, _driveNum(driveNum)
{
    // Initialize all pins to their default state
    init_pins();

    // Update state machine with default initial state (drive not present)
    updateStateMachine(false, true, false, false);

    nv::info("Initialized drive %d hot swap controller.\n", _driveNum);
}

/**
 * @brief Default constructor for hot swap controller
 *
 * Creates an uninitialized hot swap controller instance with all member
 * variables default-constructed. This constructor is provided for compatibility
 * but should not be used for normal operation.
 */
E1sHotSwap::E1sHotSwap() : state(), pinout(), _hotSwapEvent(), _driveNum()
{
    // Default constructor - no initialization performed
}

/**
 * @brief Updates the hot swap state machine based on current conditions
 *
 * Evaluates the current drive state and transitions to appropriate states
 * based on presence detection, power good status, LED states, and timer completion.
 * The state machine implements the following logic:
 *
 * - DriveDisabled: Initial state when drive is not present
 * - WaitPgood: Power enabled, waiting for power good signal or timeout
 * - WaitClkStable: Clocks enabled, waiting for stabilization time
 * - DriveOn: Drive fully operational and under host control
 * - Fault: Fault detected, can retry after timeout
 *
 * After state transitions, updates all GPIO pins according to the new state.
 *
 * @param pgood Current power good status
 * @param prsntL Current presence detection status
 * @param amberLed Current amber LED status
 * @param blueLed Current blue LED status
 * @param timerDone true if a timer has expired, false otherwise
 */
void E1sHotSwap::updateStateMachine(
    bool pgood, bool prsntL, bool amberLed, bool blueLed, bool timerDone)
{
    switch (state) {
        case DriveDisabled: {
            // Check if drive has been inserted and power is available
            if (!prsntL && !pgood) {
                // Drive present but no power - enable power and wait for power good
                state = WaitPgood;
                // uncomment this to enable power fault state
                // startTimer();
            }
            else if (!prsntL && pgood) {
                // Drive present and power available - enable clocks and wait for stabilization
                state = WaitClkStable;
                startTimer();
            }
            else if (prsntL) {
                // Drive not present - stay in disabled state
                // stay
            }
            else {
                // coverity[dead_error_line] leaving this in case state conditions change
                nv::error("Unexpected state change\n");
            }
            break;
        }
        case WaitPgood: {
            // Waiting for power good signal after enabling power
            // Power fault detection is intentionally disabled
            // TODO: Enable when fault recovery is implemented
            timerDone = false;  // Override to disable fault timeout
            if (prsntL) {
                // Drive removed while waiting - return to disabled state
                state = DriveDisabled;
            }
            else if (!prsntL && pgood) {
                // Power good received - enable clocks and wait for stabilization
                state = WaitClkStable;
                startTimer();
            }
            else if (!prsntL && !pgood && timerDone) {
                // Power good timeout - transition to fault state
                // coverity[dead_error_line] leaving this if we want to enable fault
                state = Fault;
            }
            else if (!prsntL && !pgood && !timerDone) {
                // Still waiting for power good - stay in current state
                // stay
            }
            else {
                // coverity[dead_error_line] leaving this in case state conditions change
                nv::error("Unexpected state change\n");
            }
            break;
        }
        case WaitClkStable: {
            // Waiting for clock stabilization after enabling clocks
            if (prsntL) {
                // Drive removed while waiting - return to disabled state
                state = DriveDisabled;
            }
            else if (!prsntL && pgood && timerDone) {
                // Clock stabilization complete and power good - enable PCIe and give control to
                // host
                state = DriveOn;
            }
            else if (!prsntL && !pgood && timerDone) {
                // Power good lost during clock stabilization - transition to fault state
                state = Fault;
            }
            else if (!prsntL && !timerDone) {
                // Still waiting for clock stabilization - stay in current state
                // stay
            }
            else {
                // coverity[dead_error_line] leaving this in case state conditions change
                nv::error("Unexpected state change\n");
            }
            break;
        }
        case DriveOn: {
            // Drive is fully operational and under host control
            if (prsntL) {
                // Drive removed - return to disabled state
                state = DriveDisabled;
            }
            else if (!prsntL && !pgood) {
                // Power good lost - transition to fault state
                state = Fault;
            }
            else if (!prsntL && pgood) {
                // Drive present and power good - stay operational
                // stay
            }
            else {
                // coverity[dead_error_line] leaving this in case state conditions change
                nv::error("Unexpected state change\n");
            }
            break;
        }
        case Fault: {
            // Fault state - currently stays here forever

            // Could implement retry timer for automatic recovery
            /*
            if (prsntL && retryAfterFaultTimerDone) {
                state = DriveDisabled;
            }
            else if (!prsntL && pgood && retryAfterFaultTimerDone) {
                state = WaitClkStable;
                startTimer();
            }
            else if (!prsntL && !pgood && retryAfterFaultTimerDone) {
                state = WaitPgood;
                // startTimer();
            }
            else if (!prsntL && !retryAfterFaultTimerDone) {
                // stay
            }
            else {
                // coverity[dead_error_line] leaving this in case state conditions change
                nv::error("Unexpected state change\n");
            }
            */
            break;
        }
        default: {
            nv::error("Invalid HSSM state %d\n", state);
            break;
        }
    }

    // nv::info("Drive %d state: %x\n", _driveNum, state);  // DEBUG

    // Update all GPIO pins according to the new state
    update_pins(amberLed, blueLed);
}

/**
 * @brief Updates all pins according to the current hot swap state
 *
 * Sets the power, clock, reset, and LED pins based on the current
 * state of the hot swap state machine. Each state has specific pin
 * configurations to ensure proper drive operation and safety.
 *
 * @param amberLed Current amber LED status
 * @param blueLed Current blue LED status
 */
void E1sHotSwap::update_pins(bool amberLed, bool blueLed)
{
    switch (state) {
        case DriveDisabled: {
            // Hold everything off - safe state for drive removal
            set_pin(pinout.perst_l_port, pinout.perst_l_pin, Low);     // PCIe reset asserted
            set_pin(pinout.clk_en_l_port, pinout.clk_en_l_pin, High);  // Clocks disabled
            set_pin(pinout.pwrdis_port, pinout.pwrdis_pin, Low);  // Power disable not asserted
            set_pin(pinout.pwren_l_port, pinout.pwren_l_pin, High);  // Power enable disabled
            set_leds(false, false);                                  // LEDs off
            break;
        }
        case WaitPgood: {
            // Power enabled, waiting for power good signal
            set_pin(pinout.perst_l_port, pinout.perst_l_pin, Low);  // PCIe reset still asserted
            set_pin(pinout.clk_en_l_port, pinout.clk_en_l_pin, High);  // Clocks still disabled
            set_pin(pinout.pwrdis_port, pinout.pwrdis_pin, Low);  // Power disable not asserted
            set_pin(pinout.pwren_l_port, pinout.pwren_l_pin, Low);  // Power enable active
            // set_leds(amberLed, blueLed);                             // Could use actual LED
            // states
            set_leds(true, false);  // Amber LED on to indicate power
            break;
        }
        case WaitClkStable: {
            // Power and clock enabled, waiting for stabilization
            set_pin(pinout.perst_l_port, pinout.perst_l_pin, Low);  // PCIe reset still asserted
            set_pin(pinout.clk_en_l_port, pinout.clk_en_l_pin, Low);  // Clocks enabled
            set_pin(pinout.pwrdis_port, pinout.pwrdis_pin, Low);  // Power disable not asserted
            set_pin(pinout.pwren_l_port, pinout.pwren_l_pin, Low);  // Power enable active
            set_leds(amberLed, blueLed);                            // Use actual LED states
            break;
        }
        case DriveOn: {
            // Power, clock, and PCIe fully enabled
            set_pin(pinout.perst_l_port, pinout.perst_l_pin, HiZ);    // PCIe reset deasserted
            set_pin(pinout.clk_en_l_port, pinout.clk_en_l_pin, Low);  // Clocks enabled
            set_pin(pinout.pwrdis_port, pinout.pwrdis_pin, Low);  // Power disable not asserted
            set_pin(pinout.pwren_l_port, pinout.pwren_l_pin, High);  // Power enable disabled
                                                                     // (host control)
            set_leds(amberLed, blueLed);                             // Use actual LED states
            break;
        }
        case Fault: {
            // Fault state - hold everything off for safety
            set_pin(pinout.perst_l_port, pinout.perst_l_pin, Low);     // PCIe reset asserted
            set_pin(pinout.clk_en_l_port, pinout.clk_en_l_pin, High);  // Clocks disabled
            set_pin(pinout.pwrdis_port, pinout.pwrdis_pin, Low);  // Power disable not asserted
            set_pin(pinout.pwren_l_port, pinout.pwren_l_pin, High);  // Power enable disabled
            set_leds(amberLed, blueLed);                             // Use actual LED states
            break;
        }
        default: {
            nv::error("Invalid HSSM state %d\n", state);
            break;
        }
    }
}

/**
 * @brief Sets the tristate LED pin based on amber and blue LED target states
 *
 * Controls the LED pin state based on the desired amber and blue LED states.
 * The LED pin is tristate to allow for different LED configurations.
 * Implements the E.1s specification for LED control where only one LED
 * can be active at a time.
 *
 * @param amberLed Target amber LED state
 * @param blueLed Target blue LED state
 */
void E1sHotSwap::set_leds(bool amberLed, bool blueLed)
{
    // Convert 2-pin LED control to tristate pin control
    // (For some reason amber is active high, blue is active low)
    if (blueLed && !amberLed) {
        // Blue on, amber off - drive pin low
        set_pin(pinout.led_tristate_ctrl_port, pinout.led_tristate_ctrl_pin, Low);
    }
    else if (!blueLed && amberLed) {
        // Blue off, amber on - drive pin high
        set_pin(pinout.led_tristate_ctrl_port, pinout.led_tristate_ctrl_pin, High);
    }
    else if (!blueLed && !amberLed) {
        // Both off - set pin to high impedance
        set_pin(pinout.led_tristate_ctrl_port, pinout.led_tristate_ctrl_pin, HiZ);
    }
    else {
        // Both on - not allowed by E.1s spec (throws error)
        // Turn them off as a safety measure
        set_pin(pinout.led_tristate_ctrl_port, pinout.led_tristate_ctrl_pin, HiZ);
        nv::warn("Host VIOLATED E.1s spec by attempting to drive both LEDs.\n");
    }
}

/**
 * @brief Initializes all pins to their default state
 *
 * Sets all controlled pins to safe default values during initialization.
 * Typically sets power and clock pins to disabled state and LED pin to
 * high impedance for safety.
 */
void E1sHotSwap::init_pins()
{
    // Initialize all pins to safe default states
    set_pin(pinout.perst_l_port, pinout.perst_l_pin, HiZ);     // PCIe reset deasserted
    set_pin(pinout.clk_en_l_port, pinout.clk_en_l_pin, High);  // Clocks disabled
    set_pin(pinout.pwrdis_port, pinout.pwrdis_pin, Low);       // Power disable not asserted
    set_pin(pinout.pwren_l_port, pinout.pwren_l_pin, Low);     // Power enable asserted
    set_pin(pinout.led_tristate_ctrl_port, pinout.led_tristate_ctrl_pin, HiZ);  // LED pin high
                                                                                // impedance
}

/**
 * @brief Sets a GPIO pin to the specified state
 *
 * Controls a GPIO pin to be high, low, or high impedance based on
 * the PinState enumeration. For High and Low states, configures the pin
 * as output with the appropriate level. For HiZ state, configures the
 * pin as input to achieve high impedance.
 *
 * @param port GPIO port to control
 * @param pin GPIO pin to control
 * @param pin_state Desired pin state (High, Low, or HiZ)
 */
void E1sHotSwap::set_pin(nv::gpio::GpioPort port, nv::gpio::GpioPin pin, PinState pin_state)
{
    switch (pin_state) {
        case Low: {
            // Configure pin as output and drive low
            const nv::gpio::Status gpio_status = nv::gpio::Driver::init_pin(
                port, pin, nv::gpio::Direction::Output, nv::gpio::GpioState::Low);
            if (gpio_status != nv::gpio::Status::Ok) {
                nv::error("Error setting GPIO%d_%d to %d in HSSM\n", port, pin, pin_state);
            }
            break;
        }
        case High: {
            // Configure pin as output and drive high
            const nv::gpio::Status gpio_status = nv::gpio::Driver::init_pin(
                port, pin, nv::gpio::Direction::Output, nv::gpio::GpioState::High);
            if (gpio_status != nv::gpio::Status::Ok) {
                nv::error("Error setting GPIO%d_%d to %d in HSSM\n", port, pin, pin_state);
            }
            break;
        }
        case HiZ: {
            // Configure pin as input to achieve high impedance
            const nv::gpio::Status gpio_status = nv::gpio::Driver::init_pin(
                port, pin, nv::gpio::Direction::Input, nv::gpio::GpioState::Low);
            if (gpio_status != nv::gpio::Status::Ok) {
                nv::error("Error setting GPIO%d_%d to %d in HSSM\n", port, pin, pin_state);
            }
            break;
        }
        default: {
            nv::error("Setting invalid pin state %d for GPIO%d_%d\n", pin_state, port, pin);
            break;
        }
    }
}

/**
 * @brief Starts the hot swap timer for state transitions
 *
 * Initiates a timer that will trigger state machine updates when
 * waiting for power good or clock stabilization. Uses the hot swap
 * event system to set a bit corresponding to this drive number,
 * which will be checked by the main task loop.
 *
 * Note: Could use ctimer interrupt for this instead of event-based timing.
 */
void E1sHotSwap::startTimer()
{
    // Validate drive number is within event bit range
    if (_driveNum >= (sizeof(nv::ipc::Event::Bits) * 8)) {
        nv::error("Drive number %d exceeds event bit range\n", _driveNum);
        return;
    }

    // Set the event bit corresponding to this drive number to start timing
    if (_hotSwapEvent->set(static_cast<nv::ipc::Event::Bits>(0x1U << _driveNum))
        != nv::ipc::Event::Status::Ok) {
        nv::error("Error setting drive timer event bits\n");
    }
}

}  // namespace nv::ahs

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

#include <cstdint>

#include "nv/ahs/common.h"

#include "nv/ipc/event.h"

namespace nv::ahs {

/**
 * @brief E1s Hot Swap State Machine class
 *
 * The E1sHotSwap class manages the hot swap operations for individual e.1s drives.
 * It implements a state machine that handles drive insertion/removal, power sequencing,
 * clock stabilization, and fault detection. The class controls GPIO pins for power,
 * clock, reset, and LED indicators based on the current state.
 */
class E1sHotSwap
{
public:
    // Configs

    // Public Functions
    // ---------------------------------------------

    /**
     * @brief Constructs a hot swap controller with initial state values
     *
     * @param pin_config Pin configuration for the drive
     * @param pgood Initial power good status
     * @param prsntL Initial presence detection status
     * @param hotSwapEvent Pointer to hot swap event for notifications
     * @param driveNum Drive number identifier
     */
    E1sHotSwap(nv::nhp::E1sOutputPins pin_config,
               bool                   pgood,
               bool                   prsntL,
               nv::ipc::Event*        hotSwapEvent,
               uint8_t                driveNum);

    /**
     * @brief Constructs a hot swap controller with default initial state
     *
     * @param pin_config Pin configuration for the drive
     * @param hotSwapEvent Pointer to hot swap event for notifications
     * @param driveNum Drive number identifier
     */
    E1sHotSwap(nv::nhp::E1sOutputPins pin_config,
               nv::ipc::Event*        hotSwapEvent,
               uint8_t                driveNum);

    /**
     * @brief Default constructor for hot swap controller
     *
     * Creates an uninitialized hot swap controller instance.
     */
    E1sHotSwap();

    /**
     * @brief Updates the hot swap state machine based on current conditions
     *
     * Evaluates the current drive state and transitions to appropriate states
     * based on presence detection, power good status, LED states, and timer completion.
     * Controls GPIO pins according to the current state.
     *
     * @param pgood Current power good status
     * @param prsntL Current presence detection status
     * @param timerDone true if a timer has expired, false otherwise
     */
    void updateStateMachine(bool pgood, bool prsntL, bool perstL, bool timerDone = false);

private:
    // Member Constants
    // ---------------------------------------------

    /**
     * @brief Hot swap state enumeration
     *
     * Defines the possible states of the hot swap state machine for each drive.
     */
    enum HotSwapState
    {
        DriveDisabled, /**< Initial state: drive not present */
        WaitPgood,     /**< Power enabled, waiting for power good or timeout */
        WaitClkStable, /**< Clocks enabled, waiting for stabilization time */
        DriveOn,       /**< Drive present and control given to host */
        Fault          /**< Fault detected, can retry after timeout */
    };

    /**
     * @brief Pin state enumeration
     *
     * Defines the possible states for GPIO pins in the hot swap controller.
     */
    enum PinState
    {
        Low,  /**< Pin driven low */
        High, /**< Pin driven high */
        HiZ   /**< Pin in high impedance (tristate) */
    };

    // Helper Functions
    // ---------------------------------------------

    /**
     * @brief Updates all pins according to the current hot swap state
     *
     * Sets the power, clock, reset, and LED pins based on the current
     * state of the hot swap state machine.
     *
     * @param perstL Current PCIE PERST# status
     */
    void update_pins(bool perstL);

    /**
     * @brief Sets the tristate LED pin based on amber and blue LED target states
     *
     * Controls the LED pin state based on the desired amber and blue LED states.
     * The LED pin is tristate to allow for different LED configurations.
     *
     * @param amberLed Target amber LED state
     * @param blueLed Target blue LED state
     */
    void set_leds(bool amberLed, bool blueLed);

    /**
     * @brief Initializes all pins to their default state
     *
     * Sets all controlled pins to safe default values during initialization.
     * Typically sets power and clock pins to disabled state.
     */
    void init_pins();

    /**
     * @brief Sets a GPIO pin to the specified state
     *
     * Controls a GPIO pin to be high, low, or high impedance based on
     * the PinState enumeration.
     *
     * @param port GPIO port to control
     * @param pin GPIO pin to control
     * @param pin_state Desired pin state (High, Low, or HiZ)
     * @param reverse_polarity If true, the pin state will be reversed (Low -> High, High ->
     * Low, HiZ -> HiZ)
     */
    void set_pin(nv::gpio::GpioPort port,
                 nv::gpio::GpioPin  pin,
                 PinState           pin_state,
                 bool               reverse_polarity = false);

    /**
     * @brief Starts the hot swap timer for state transitions
     *
     * Initiates a timer that will trigger state machine updates when
     * waiting for power good or clock stabilization.
     */
    void startTimer();

    // Member Variables
    // ---------------------------------------------

    /** @brief Current state of the hot swap state machine */
    HotSwapState state;

    /** @brief Pin configuration for the drive */
    nv::nhp::E1sOutputPins pinout;

    /** @brief Pointer to hot swap event for notifications */
    nv::ipc::Event* _hotSwapEvent;

    /** @brief Drive number identifier for this instance */
    uint8_t _driveNum;
};

}  // namespace nv::ahs

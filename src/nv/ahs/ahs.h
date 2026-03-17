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

#include NV_IPC_CONFIG_H

#include "nv/emulation/pca9555.h"
#include "nv/ipc/event.h"
#include "nv/ahs/common.h"
#include "nv/ahs/hssm.h"
#include "nv/ahs/eeprom.h"
#include "sys/i2c/i2c_slave.h"
#include "sys/adc/adc.h"
#include <optional>

namespace nv::ahs {

/**
 * @brief AHS (Add-in Hot Swap) controller class for managing e.1s drive hot swap operations
 *
 * The AHS class provides functionality for managing hot swap operations of e.1s drives,
 * including GPIO interrupt handling, ADC monitoring, I2C communication with EEPROM
 * and IO expanders, and hot swap state machine management.
 */
class AHS
{
public:
    /**
     * @brief Configuration structure for AHS initialization
     *
     * Contains all necessary configuration parameters for setting up an AHS instance,
     * including I2C bus configuration, pin assignments, interrupt pins, and addresses.
     */
    struct AhsConfig
    {
        /** @brief I2C hardware peripheral port for communication */
        nv::i2c::Port i2c_bus;

        /** @brief Output pin configurations for each e.1s drive */
        std::array<nhp::E1sOutputPins, nhp::NumE1sDrives> output_pins;

        /** @brief Input pin configurations for each e.1s drive */
        std::array<nhp::E1sInputPins, nhp::NumE1sDrives> input_pins;

        /** @brief NHP interrupt pin port */
        nv::gpio::GpioPort interrupt_port;

        /** @brief NHP interrupt pin number */
        nv::gpio::GpioPin interrupt_pin;

        /** @brief AHS EEPROM I2C address */
        uint8_t eeprom_address;

        /** @brief AHS IO expander I2C address */
        uint8_t io_expander_address;

        /** @brief AHS instance number for identification */
        uint8_t ahsInstanceNum;
    };

    // Public Functions
    // ---------------------------------------------

    /**
     * @brief Constructs an AHS instance with the specified configuration
     *
     * @param config Configuration structure containing all AHS parameters
     * @param hotSwapEvent Pointer to the hot swap event for notification
     */
    AHS(const AhsConfig& config, nv::ipc::Event* hotSwapEvent);

    /**
     * @brief Default constructor for AHS instance
     *
     * Creates an uninitialized AHS instance. Member variables are default-constructed.
     */
    AHS();

    /**
     * @brief Handles GPIO interrupt events for input pin changes
     *
     * Processes GPIO interrupts from the presence detection pins and updates
     * the internal state accordingly. Triggers hot swap state machine updates.
     *
     * @param port GPIO port that generated the interrupt
     * @param pin GPIO pin that generated the interrupt
     * @param driveIndex Index of the drive associated with this interrupt
     */
    void gpio_interrupt(nv::gpio::GpioPort port, nv::gpio::GpioPin pin, uint8_t driveIndex);

    /**
     * @brief Handles ADC interrupt events for voltage monitoring
     *
     * Processes ADC interrupts to monitor power good status of drives.
     * Updates internal pgood state and triggers GPIO interrupt handling.
     *
     * @param peripheral ADC peripheral that generated the interrupt
     * @param driveIndex Index of the drive being monitored
     * @param value ADC reading value for voltage monitoring
     */
    void adc_interrupt(sys::adc::AdcPeripheral peripheral, uint8_t driveIndex, uint16_t value);

    /**
     * @brief Static callback for I2C slave driver communication
     *
     * Handles I2C read/write operations from the slave driver.
     * Routes operations to appropriate internal handlers.
     *
     * @param address I2C address being accessed
     * @param is_read true for read operations, false for write operations
     * @param buffer Buffer for data transfer
     * @param data_size Size of data being transferred
     * @param this_task_instance Pointer to the AHS instance
     * @param new_transaction true if this is a new I2C transaction
     */
    static void i2c_callback(uint8_t&                                           address,
                             bool                                               is_read,
                             std::array<uint8_t, sys::i2c::I2cSlaveBufferSize>& buffer,
                             size_t&                                            data_size,
                             void* this_task_instance,
                             bool  new_transaction);

    /**
     * @brief Handles hot swap timer interrupt events
     *
     * Called when hot swap timers expire to update state machines.
     *
     * @param driveIndex Index of the drive whose timer expired
     */
    void hotSwapTimerInterrupt(uint8_t driveIndex);

private:
    // Helper Functions
    // ---------------------------------------------

    /**
     * @brief Handles I2C write operations to PCA9555 IO expander
     *
     * @param address I2C address being written to
     * @param rx_buffer Buffer containing data to write
     * @param rx_data_size Size of data to write
     */
    void i2c_write(uint8_t&                                           address,
                   std::array<uint8_t, sys::i2c::I2cSlaveBufferSize>& rx_buffer,
                   size_t&                                            rx_data_size);

    /**
     * @brief Handles I2C read operations from PCA9555 IO expander
     *
     * @param address I2C address being read from
     * @param tx_buffer Buffer to store read data
     * @param tx_data_size Size of data read
     * @param new_transaction true if this is a new I2C transaction
     */
    void i2c_read(uint8_t&                                           address,
                  std::array<uint8_t, sys::i2c::I2cSlaveBufferSize>& tx_buffer,
                  size_t&                                            tx_data_size,
                  bool                                               new_transaction);

    /**
     * @brief Updates the interrupt pin state based on IO expander status
     *
     * Sets the interrupt pin high or low depending on whether any IO expander
     * is asserting an interrupt.
     */
    void updateInterruptPin();

    /**
     * @brief Updates the hot swap state machine for a specific drive
     *
     * @param driveIndex Index of the drive to update
     */
    void updateHotSwap(uint8_t driveIndex);

    // Member Constants
    // ---------------------------------------------

    /** @brief Pin mask for input pins to maintain GPIO direction synchronization */
    constexpr static uint16_t InputPinMask = AhsPrsnt0LBitmask | (AhsPrsnt0LBitmask << 8U);

    /** @brief Dummy data sent during I2C read errors */
    constexpr static uint8_t DummyData = 0xff;

    /** @brief Number of GPIO expanders for AHS instance */
    constexpr static uint8_t NumGpioExpanders = (nhp::NumE1sDrives + 1) / 2;

    // Member Variables
    // ---------------------------------------------

    /** @brief I2C bus port configuration */
    nv::i2c::Port i2c_bus;

    /** @brief I2C slave driver instance */
    sys::i2c::I2CSlaveDriver<AHS> i2c_driver;

    /** @brief Interrupt pin port */
    nv::gpio::GpioPort interrupt_l_port;

    /** @brief Interrupt pin number */
    nv::gpio::GpioPin interrupt_l_pin;

    /** @brief AHS EEPROM I2C address */
    std::array<uint8_t, NumGpioExpanders> ahs_eeprom_addresses;

    /** @brief AHS IO expander I2C address */
    std::array<uint8_t, NumGpioExpanders> ahs_io_expander_addresses;

    /** @brief Input pin configurations for each e.1s drive */
    std::array<nhp::E1sInputPins, nhp::NumE1sDrives> e1s_pin_in;

    /** @brief Output pin configurations for each e.1s drive */
    std::array<nhp::E1sOutputPins, nhp::NumE1sDrives> e1s_pin_out;

    /** @brief PCA9555 GPIO expander object for drive control */
    std::array<std::optional<emulation::Pca9555>, NumGpioExpanders> e1s_gpio_expanders;

    /** @brief EEPROM object for configuration storage */
    std::array<std::optional<EEPROM>, NumGpioExpanders> ahs_eeproms;

    /** @brief Hot swap state machine instances for each drive */
    std::array<std::optional<ahs::E1sHotSwap>, nhp::NumE1sDrives> e1s_hssms;

    /** @brief Power good status values for each drive */
    std::array<bool, nhp::NumE1sDrives> pgood_vals;

    /** @brief Presence detection values for each drive */
    std::array<bool, nhp::NumE1sDrives> prsntL_vals;

    /** @brief PCIE PERST# values for each drive */
    std::array<bool, nhp::NumE1sDrives> perstL_vals;
};

}  // namespace nv::ahs

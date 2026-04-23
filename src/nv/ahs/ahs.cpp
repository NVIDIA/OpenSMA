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

#include "nv/ahs/ahs.h"
#include "nv/gpio/driver.h"
#include "sys/adc/adc.h"
#include NV_IPC_CONFIG_H
#include "sys/common/system.h"
#include "nv/logger/common.h"
#include "nv/logger/log.h"

#ifndef ENABLE_PERST_MONITORING
constexpr static bool EnablePerstLMonitoring = false;
#else
constexpr static bool EnablePerstLMonitoring = ENABLE_PERST_MONITORING ? true : false;
#endif

// AHS Will support the following configuratrions:
// 1-channel/1-drive - PX90E (Orchid CX9)
// 1-channel/8-drive - Single VPP supporting all 8 drives (not likely to be used).
// 1-channel/4-drive - Single VPP supporting 4 drives (P9357-C00 testing only)
// 2-channel/4-drive - Dual VPP (MCIO0/MCIO2) supporting 4 drives each (P9357-C01)
// 4-channel/2-drive - Quad VPP (MCIO0/MCIO1/MCIO2/MCIO3) supporting 2 drives each
// (P9357-C00/B00??)
static_assert((nv::nhp::NumNhpInstances == 1U && nv::nhp::NumE1sDrives == 1U)
                  || (nv::nhp::NumNhpInstances == 1U && nv::nhp::NumE1sDrives == 4U)
                  || (nv::nhp::NumNhpInstances == 1U && nv::nhp::NumE1sDrives == 8U)
                  || (nv::nhp::NumNhpInstances == 2U && nv::nhp::NumE1sDrives == 4U)
                  || (nv::nhp::NumNhpInstances == 4U && nv::nhp::NumE1sDrives == 2U),
              "AHS Configuration Not Support.");

namespace nv::ahs {

/**
 * @brief Constructs an AHS instance with the specified configuration
 *
 * Initializes all member variables from the configuration, sets up GPIO pins,
 * initializes I2C devices (PCA9555 GPIO expander and EEPROM), creates hot swap
 * state machines for each drive, and starts the I2C slave driver.
 *
 * @param config Configuration structure containing all AHS parameters
 * @param hotSwapEvent Pointer to the hot swap event for notification
 */
AHS::AHS(const AhsConfig& config, nv::ipc::Event* hotSwapEvent)
: i2c_bus(config.i2c_bus)
, i2c_driver()
, interrupt_l_port(config.interrupt_port)
, interrupt_l_pin(config.interrupt_pin)
, ahs_eeprom_addresses()
, ahs_io_expander_addresses()
, e1s_pin_in(config.input_pins)
, e1s_pin_out(config.output_pins)
, pgood_vals{}
, pgood_valid_vals{}
, prsntL_vals{}
, perstL_vals{}
{
    // Initialize the I2C driver configuration
    static_assert((NumGpioExpanders * 2) <= sys::i2c::NumI2cTargetAddresses,
                  "NumGpioExpanders is greater than NumI2cTargetAddresses");

    std::array<uint8_t, sys::i2c::NumI2cTargetAddresses> target_addresses{};
    target_addresses.fill(0);
    for (uint8_t gpioIndex = 0; gpioIndex < NumGpioExpanders; gpioIndex++) {
        target_addresses.at(gpioIndex)                    = config.eeprom_address + gpioIndex;
        target_addresses.at(gpioIndex + NumGpioExpanders) = config.io_expander_address
                                                          + gpioIndex;
        ahs_eeprom_addresses.at(gpioIndex)      = config.eeprom_address + gpioIndex;
        ahs_io_expander_addresses.at(gpioIndex) = config.io_expander_address + gpioIndex;
    }

    // Initialize the I2C driver
    i2c_driver.bind(i2c_bus, target_addresses, this);

    // Initialize status arrays with default values
    pgood_vals.fill(false);
    pgood_valid_vals.fill(false);
    prsntL_vals.fill(true);
    perstL_vals.fill(true);

    // Configure the interrupt pin as output, initially high
    auto const status = nv::gpio::Driver::init_pin(interrupt_l_port,
                                                   interrupt_l_pin,
                                                   nv::gpio::Direction::Output,
                                                   nv::gpio::GpioState::High);

    if (status != nv::gpio::Status::Ok) {
        nv::error("Failed to initialize interrupt pin: port=%d, pin=%d\n",
                  interrupt_l_port,
                  interrupt_l_pin);
        nv::logger::error(
            logger::Event::AhsInvalidInterruptPin,
            {static_cast<uint8_t>(interrupt_l_port), static_cast<uint8_t>(interrupt_l_pin)});

        // Consider appropriate error recovery or assertion here
    }

    // Initialize the I2C devices
    for (uint8_t gpioIndex = 0; gpioIndex < NumGpioExpanders; gpioIndex++) {
        e1s_gpio_expanders.at(gpioIndex).emplace(0x0000, 0x0000, InputPinMask);
        auto const num_drives = (nhp::NumE1sDrives == 1U) ? IO_EXPANDER_ONE_SSD
                                                          : IO_EXPANDER_TWO_SSD;
        ahs_eeproms.at(gpioIndex).emplace(ahs_io_expander_addresses.at(gpioIndex), num_drives);
    }

    for (uint8_t bankDriveIndex = 0; bankDriveIndex < nhp::NumE1sDrives; bankDriveIndex++) {
        // coverity[cert_int31_c_violation] dont care about lost bits
        e1s_hssms.at(bankDriveIndex)
            .emplace(e1s_pin_out.at(bankDriveIndex),
                     hotSwapEvent,
                     (bankDriveIndex + (nv::nhp::NumE1sDrives * config.ahsInstanceNum)));

        // Set up GPIO interrupts for presence detection on the drive
        nv::gpio::Driver::init_interrupt(e1s_pin_in.at(bankDriveIndex).prsnt_l_port,
                                         e1s_pin_in.at(bankDriveIndex).prsnt_l_pin,
                                         nv::gpio::InterruptDetection::InterruptBothEdge,
                                         nv::gpio::InterruptSelect::InterruptSelect1);

        if (EnablePerstLMonitoring) {
            nv::gpio::Driver::init_interrupt(e1s_pin_in.at(bankDriveIndex).perst_l_port,
                                             e1s_pin_in.at(bankDriveIndex).perst_l_pin,
                                             nv::gpio::InterruptDetection::InterruptBothEdge,
                                             nv::gpio::InterruptSelect::InterruptSelect1);
        }

        // Trigger initial GPIO interrupt handling to establish baseline state
        gpio_interrupt(e1s_pin_in.at(bankDriveIndex).prsnt_l_port,
                       e1s_pin_in.at(bankDriveIndex).prsnt_l_pin,
                       bankDriveIndex);
    }

    // Start the I2C slave driver for communication
    i2c_driver.start();

    nv::info("Finished AHS initialization for %d drives\n", nhp::NumE1sDrives);
}

/**
 * @brief Default constructor for AHS instance
 *
 * Creates an uninitialized AHS instance with all member variables
 * default-constructed. This constructor is provided for compatibility
 * but should not be used for normal operation.
 */
AHS::AHS()
: i2c_bus()
, i2c_driver()
, interrupt_l_port()
, interrupt_l_pin()
, ahs_eeprom_addresses()
, ahs_io_expander_addresses()
, e1s_pin_in()
, e1s_pin_out()
, pgood_vals{}
, pgood_valid_vals{}
, prsntL_vals{}
, perstL_vals{}
{
    // Default constructor - no initialization performed
}

/**
 * @brief Handles GPIO interrupt events for input pin changes
 *
 * Processes GPIO interrupts from the presence detection pins. Reads the current
 * presence state, updates internal tracking arrays, updates the GPIO expander
 * input pins, and triggers hot swap state machine updates.
 *
 * @param port GPIO port that generated the interrupt (unused)
 * @param pin GPIO pin that generated the interrupt (unused)
 * @param driveIndex Index of the drive associated with this interrupt
 */
void AHS::gpio_interrupt([[maybe_unused]] nv::gpio::GpioPort port,
                         [[maybe_unused]] nv::gpio::GpioPin  pin,
                         uint8_t                             driveIndex)
{
    // Read the current presence detection state from the GPIO pin
    uint8_t          prsnt_l_temp = 0;
    nv::gpio::Status status = nv::gpio::Driver::read(e1s_pin_in.at(driveIndex).prsnt_l_port,
                                                     e1s_pin_in.at(driveIndex).prsnt_l_pin,
                                                     prsnt_l_temp);
    if (status != gpio::Status::Ok) {
        nv::error("Failed to read prsntL for drive %d\n", driveIndex);
        nv::logger::error(logger::Event::AhsInvalidInputPin,
                          {driveIndex,
                           static_cast<uint8_t>(e1s_pin_in.at(driveIndex).prsnt_l_port),
                           static_cast<uint8_t>(e1s_pin_in.at(driveIndex).prsnt_l_pin)});
        return;  // Don't update the value if read failed
    }
    prsntL_vals.at(driveIndex) = (prsnt_l_temp != 0);

    if (EnablePerstLMonitoring) {
        // Read the current PERST# state from the GPIO pin
        uint8_t perst_l_temp = 0;
        status               = nv::gpio::Driver::read(e1s_pin_in.at(driveIndex).perst_l_port,
                                        e1s_pin_in.at(driveIndex).perst_l_pin,
                                        perst_l_temp);
        if (status != gpio::Status::Ok) {
            nv::error("Failed to read perstL for drive %d\n", driveIndex);
            nv::logger::error(logger::Event::AhsInvalidInputPin,
                              {driveIndex,
                               static_cast<uint8_t>(e1s_pin_in.at(driveIndex).perst_l_port),
                               static_cast<uint8_t>(e1s_pin_in.at(driveIndex).perst_l_pin)});
            return;  // Don't update the value if read failed
        }
        perstL_vals.at(driveIndex) = (perst_l_temp != 0);
    }

    // Update presence detection in GPIO expander
    // coverity[cert_int31_c_violation] dont care about lost bits
    const uint8_t newPinVals = (static_cast<uint8_t>(prsntL_vals.at(driveIndex))
                                << AhsPrsnt0LBit);
    const uint8_t gpioIndex  = driveIndex / 2;
    const nv::emulation::Pca9555Bank gpioBank = (driveIndex % 2 == 0)
                                                  ? nv::emulation::Pca9555Bank::Zero
                                                  : nv::emulation::Pca9555Bank::One;
    e1s_gpio_expanders.at(gpioIndex)->update_input_pins(
        newPinVals, AhsPrsnt0LBitmask, gpioBank);

    // Update interrupt pin state and hot swap state machine
    updateInterruptPin();
    updateHotSwap(driveIndex);
}

/**
 * @brief Handles ADC interrupt events for voltage monitoring
 *
 * Processes ADC interrupts to monitor power good status of drives. Compares
 * the new ADC value against the power good threshold and updates the internal
 * pgood state. If the state changes, triggers GPIO interrupt handling to
 * update the hot swap state machine.
 *
 * @param peripheral ADC peripheral that generated the interrupt (unused)
 * @param driveIndex Index of the drive being monitored
 * @param value ADC reading value for voltage monitoring
 */
void AHS::adc_interrupt([[maybe_unused]] sys::adc::AdcPeripheral peripheral,
                        uint8_t                                  driveIndex,
                        uint16_t                                 value)
{
    // nv::info("Drive%d vmon: %x\n", driveIndex, value);  // DEBUG

    // Compare new power good status with previous value
    // NOLINTNEXTLINE: oldPgood is being initalized
    const bool oldPgood       = pgood_vals.at(driveIndex);
    pgood_vals.at(driveIndex) = value > nhp::PgoodThreshold;

    // If power good status changed, trigger GPIO interrupt handling
    if ((pgood_vals.at(driveIndex) != oldPgood) || !pgood_valid_vals.at(driveIndex)) {
        pgood_valid_vals.at(driveIndex) = true;
        gpio_interrupt(0U, 0U, driveIndex);
    }
}

/**
 * @brief Static callback for I2C slave driver communication
 *
 * Static wrapper function that routes I2C read/write operations to the
 * appropriate instance methods. This is required by the I2C slave driver
 * interface and handles the routing of operations based on read/write
 * direction.
 *
 * @param address I2C address being accessed
 * @param is_read true for read operations, false for write operations
 * @param buffer Buffer for data transfer
 * @param data_size Size of data being transferred
 * @param this_task_instance Pointer to the AHS instance
 * @param new_transaction true if this is a new I2C transaction
 */
void AHS::i2c_callback(uint8_t&                                           address,
                       bool                                               is_read,
                       std::array<uint8_t, sys::i2c::I2cSlaveBufferSize>& buffer,
                       size_t&                                            data_size,
                       void*                                              this_task_instance,
                       bool                                               new_transaction)
{
    // nv::info("i2c callback r/w:%d addr:%x\n", is_read, address);
    if (is_read) {
        // Route read operations to i2c_read method
        // NOLINTNEXTLINE: no way around this without function pointers
        static_cast<AHS*>(this_task_instance)
            ->i2c_read(address, buffer, data_size, new_transaction);
    }
    else {
        // Route write operations to i2c_write method
        // NOLINTNEXTLINE: no way around this without function pointers
        static_cast<AHS*>(this_task_instance)->i2c_write(address, buffer, data_size);
    }
}

/**
 * @brief Handles I2C read operations from PCA9555 IO expander
 *
 * Processes I2C read requests for both the EEPROM and IO expander addresses.
 * For EEPROM reads, handles normal reads. For IO expander
 * reads, reads the current pin states and updates the interrupt pin.
 *
 * @param address I2C address being read from
 * @param tx_buffer Buffer to store read data
 * @param tx_data_size Size of data read
 * @param new_transaction true if this is a new I2C transaction
 */
void AHS::i2c_read(uint8_t&                                           address,
                   std::array<uint8_t, sys::i2c::I2cSlaveBufferSize>& tx_buffer,
                   size_t&                                            tx_data_size,
                   bool                                               new_transaction)
{
    uint8_t reg_output = 0;
    for (uint8_t gpioIndex = 0; gpioIndex < NumGpioExpanders; gpioIndex++) {
        if (address == ahs_eeprom_addresses.at(gpioIndex)) {
            // Normal EEPROM read
            ahs_eeproms.at(gpioIndex)->i2c_read(reg_output, new_transaction);
            tx_buffer.at(0) = reg_output;
            tx_data_size    = 1;
            return;
        }
        if (address == ahs_io_expander_addresses.at(gpioIndex)) {
            // Read from IO expander and update interrupt pin state
            e1s_gpio_expanders.at(gpioIndex)->i2c_read(reg_output, new_transaction);
            tx_buffer.at(0) = reg_output;
            tx_data_size    = 1;
            updateInterruptPin();
            return;
        }
    }
    // Invalid address - return dummy data
    nv::error("Invalid address I2C read to AHS\n");
    nv::logger::error(logger::Event::AhsInvalidI2cReadAddress, {address});
    tx_buffer.at(0) = DummyData;
    tx_data_size    = 1;
}

/**
 * @brief Handles I2C write operations to PCA9555 IO expander
 *
 * Processes I2C write requests for both the EEPROM and IO expander addresses.
 * For EEPROM writes, forwards the data to the EEPROM object. For IO expander
 * writes, updates the expander and processes any pin state changes.
 *
 * @param address I2C address being written to
 * @param rx_buffer Buffer containing data to write
 * @param rx_data_size Size of data to write
 */
void AHS::i2c_write(uint8_t&                                           address,
                    std::array<uint8_t, sys::i2c::I2cSlaveBufferSize>& rx_buffer,
                    size_t&                                            rx_data_size)
{
    for (uint8_t gpioIndex = 0; gpioIndex < NumGpioExpanders; gpioIndex++) {
        if (address == ahs_eeprom_addresses.at(gpioIndex)) {
            // Write data to EEPROM
            // coverity[cert_int31_c_violation] datasize cant be more than 256
            ahs_eeproms.at(gpioIndex)->i2c_write(rx_buffer, rx_data_size);
            return;
        }
        else if (address == ahs_io_expander_addresses.at(gpioIndex)) {
            // Write data to IO expander
            // coverity[cert_int31_c_violation] datasize cant be more than 256
            e1s_gpio_expanders.at(gpioIndex)->i2c_write(rx_buffer, rx_data_size);
            updateInterruptPin();
            return;
        }
    }
    // Invalid address
    nv::error("Invalid address I2C write to AHS\n");
    nv::logger::error(logger::Event::AhsInvalidI2cWriteAddress, {address});
}

/**
 * @brief Handles hot swap timer interrupt events
 *
 * Called when hot swap timers expire to update the state machine for a
 * specific drive. Passes the current drive status values to the state
 * machine with timer completion flag set to true.
 *
 * @param driveIndex Index of the drive whose timer expired
 */
void AHS::hotSwapTimerInterrupt(uint8_t driveIndex)
{
    const bool perstL = EnablePerstLMonitoring ? perstL_vals.at(driveIndex) : true;
    e1s_hssms.at(driveIndex)
        ->updateStateMachine(
            pgood_vals.at(driveIndex), prsntL_vals.at(driveIndex), perstL, true);
}

void AHS::peripheral_recovery()
{
    i2c_driver.peripheral_recovery();
}

/**
 * @brief Updates the interrupt pin state based on IO expander status
 *
 * Checks if any IO expander is asserting an interrupt and sets the
 * interrupt pin accordingly. Sets the pin low if an interrupt is active,
 * high if no interrupts are active.
 */
void AHS::updateInterruptPin()
{
    // Enter a context-specific critical section
    const nv::common::ScopedCritical critical;

    for (uint8_t gpioIndex = 0; gpioIndex < NumGpioExpanders; gpioIndex++) {
        if (!e1s_gpio_expanders.at(gpioIndex)->get_interrupt_l_state()) {
            nv::gpio::Driver::write(interrupt_l_port, interrupt_l_pin, 0U);
            return;
        }
    }
    nv::gpio::Driver::write(interrupt_l_port, interrupt_l_pin, 1U);
}

/**
 * @brief Updates the hot swap state machine for a specific drive
 *
 * Calls the updateStateMachine method on the hot swap state machine
 * for the specified drive, passing the current status values.
 *
 * @param driveIndex Index of the drive to update
 */
void AHS::updateHotSwap(uint8_t driveIndex)
{
    if (!pgood_valid_vals.at(driveIndex)) {
        return;
    }
    const bool perstL = EnablePerstLMonitoring ? perstL_vals.at(driveIndex) : true;
    e1s_hssms.at(driveIndex)
        ->updateStateMachine(pgood_vals.at(driveIndex), prsntL_vals.at(driveIndex), perstL);
}

}  // namespace nv::ahs

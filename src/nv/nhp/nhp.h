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
#include "nv/gpio/common.h"
#include "nv/nhp/common.h"
#include "nv/nhp/hssm.h"
#include "sys/adc/adc.h"
#include "sys/i2c/i2c_slave.h"

// NHP checklist
// route pins
// enable interrupts for input pins
// add all ports/pins (perstL & pgood) and update interrupt irq array in sys/gpio/constant.h
// add port interrupt handler in gpio_interrupt_handler and add pin checks

// configure i2c bus addr0 to 0x08
// configure i2c bus addr1 to 0x77
// present configure i2c bus to addr0 - addr1 mode
// make sure enableAck is true

// set NumE1sDrives, MappingVersion, and NumOfPartitions in task.h

namespace nv::nhp {

class NHP
{
public:
    // Configs
    // ---------------------------------------------
    struct NhpConfig
    {
        // i2c hw peripheral
        nv::i2c::Port i2c_bus;

        // e.1s pinouts
        std::array<E1sOutputPins, NumE1sDrives> output_pins;
        std::array<E1sInputPins, NumE1sDrives>  input_pins;

        // nhp interrupt pin
        nv::gpio::GpioPort interrupt_port;
        nv::gpio::GpioPin  interrupt_pin;
    };

    // Public Functions
    // ---------------------------------------------

    NHP(const NhpConfig& config);
    NHP();

    // applies gpio pin change
    void gpio_interrupt(nv::gpio::GpioPort port, nv::gpio::GpioPin pin, uint8_t drive_num);

    // vmon adc interrupt
    void adc_interrupt(sys::adc::AdcPeripheral peripheral, uint8_t driveIndex, uint16_t value);

    // callback for i2c slave driver - do not use
    static void i2c_callback(uint8_t&                                                   address,
                             bool                                                       is_read,
                             std::array<uint8_t, sys::i2c::I2CSlaveDriver::BufferSize>& buffer,
                             size_t& data_size,
                             void*   this_task_instance,
                             bool    new_transaction);

private:
    // Helper Functions
    // ---------------------------------------------
    // reads from PCA9555
    void i2c_write(uint8_t&                                                   address,
                   std::array<uint8_t, sys::i2c::I2CSlaveDriver::BufferSize>& rx_buffer,
                   size_t&                                                    rx_data_size);

    // reads from PCA9555
    void i2c_read(uint8_t&                                                   address,
                  std::array<uint8_t, sys::i2c::I2CSlaveDriver::BufferSize>& tx_buffer,
                  size_t&                                                    tx_data_size,
                  bool                                                       new_transaction);

    // helper function that pushes the interrupt expander pin value to the output pin
    void push_interrupt_expander_pin();

    // helper function that propagates changes from gpio expander to hot swap SM and interrupt
    // expander
    void propagate_gpio_expander_change(uint8_t drive_num);

    // reads input pins for that drive
    // returns the pin state for prsntL and pgood
    void read_input_pins(uint8_t drive_index, bool& prsnt_l, bool& pgood);

    // helper function that converts a i2c address to the drive index for the
    // PCA9555 converts i2c address to e.1s drive gpio expander index
    // returns false if no index exists
    bool i2c_address_to_drive_index(uint8_t address, uint8_t& drive_index);

    // Member Constants
    // ---------------------------------------------

    // i2c address for the interrupt gpio expander
    constexpr static uint8_t InterruptAggregatorAddress = 0x20;
    // first i2c address for the e.1s drive gpio expander (drive index 0)
    constexpr static uint8_t E1sGpioExpanderAddress = 0x21;

    // For initializing the NHP config register in the interrupt gpio expander
    // first 8 bits inside are nhp reserved last 8 bits are interrupts
    constexpr static uint8_t  InterruptAggregatorReservedBits = 8U;
    constexpr static uint16_t InterruptGpioXpndrInitVal       = 0xff00 | MappingVersion
                                                        | (NumOfPartitions << 6U);
    // required to be inputs to not go out of sync with actual GPIO direction
    constexpr static uint16_t InputPinMask = NhpPrsnt0LBitmask | NhpPwrGdBitmask;

    // interrupt gpio expander needs to be all inputs
    constexpr static uint16_t InterruptRequiredInputs  = 0xffff;
    constexpr static uint16_t InterruptRequiredOutputs = 0x0000;

    // dummy data to send during i2c read errors
    constexpr static uint8_t DummyData = 0xff;

    // Member Variables
    // ---------------------------------------------
    // i2c driver config
    nv::i2c::Port                    i2c_bus;
    sys::i2c::I2CSlaveDriver::Config i2c_driver_config;
    sys::i2c::I2CSlaveDriver         i2c_driver;

    // nhp interrupt pin
    nv::gpio::GpioPort interrupt_l_port;
    nv::gpio::GpioPin  interrupt_l_pin;

    // gpio expander for aggregating the interrupts
    emulation::Pca9555 interrupt_l_aggregator;

    // i/o pins for each drive
    std::array<E1sInputPins, NumE1sDrives>  e1s_pin_in;
    std::array<E1sOutputPins, NumE1sDrives> e1s_pin_out;

    // the PCA9555 gpio expander object for each e1s drive
    std::array<emulation::Pca9555, NumE1sDrives> e1s_gpio_expanders;

    // hot swap controller state machine per drive
    std::array<NhpE1sHotSwap, NumE1sDrives> e1s_hssms;

    // for tracking voltage values for pgood
    std::array<bool, NumE1sDrives> pgood_vals;
};

}  // namespace nv::nhp

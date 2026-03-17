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
#include "nv/vpp/common.h"
#include "nv/vpp/hssm.h"
#include "sys/i2c/i2c_slave.h"
#include "sys/adc/adc.h"

namespace nv::vpp {

class VPP
{
public:
    // Configs
    // ---------------------------------------------
    struct VppConfig
    {
        // i2c hw peripheral
        nv::i2c::Port i2c_bus;

        // e.1s pinouts
        std::array<nhp::E1sOutputPins, nhp::NumE1sDrives> output_pins;
        std::array<nhp::E1sInputPins, nhp::NumE1sDrives>  input_pins;

        // nhp interrupt pin
        nv::gpio::GpioPort interrupt_port;
        nv::gpio::GpioPin  interrupt_pin;

        uint8_t vppInstanceNum;
    };
    // Public Functions
    // ---------------------------------------------
    VPP(const VppConfig& config, nv::ipc::Event* hotSwapEvent);
    VPP();

    // applies input pin changes
    void gpio_interrupt(nv::gpio::GpioPort port, nv::gpio::GpioPin pin, uint8_t driveIndex);

    // vmon adc interrupt
    void adc_interrupt(sys::adc::AdcPeripheral peripheral, uint8_t driveIndex, uint16_t value);

    // callback for i2c slave driver - do not use
    static void i2c_callback(uint8_t&                                           address,
                             bool                                               is_read,
                             std::array<uint8_t, sys::i2c::I2cSlaveBufferSize>& buffer,
                             size_t&                                            data_size,
                             void* this_task_instance,
                             bool  new_transaction);

    void hotSwapTimerInterrupt(uint8_t driveIndex);

private:
    // Helper Functions
    // ---------------------------------------------
    // reads from PCA9555
    void i2c_write(uint8_t&                                           address,
                   std::array<uint8_t, sys::i2c::I2cSlaveBufferSize>& rx_buffer,
                   size_t&                                            rx_data_size);

    // reads from PCA9555
    void i2c_read(uint8_t&                                           address,
                  std::array<uint8_t, sys::i2c::I2cSlaveBufferSize>& tx_buffer,
                  size_t&                                            tx_data_size,
                  bool                                               new_transaction);

    void updateInterruptPin();

    void updateHotSwap(uint8_t driveIndex);

    // helper function that converts a i2c address to the index for the
    // PCA9555 converts i2c address to e.1s drive gpio expander index
    // returns false if no index exists
    bool i2c_address_to_gpio_index(uint8_t address, uint8_t& gpioIndex);

    // gives the pca9555 index and bank for that drive
    void drive_to_gpio_index_bank(uint8_t                     driveIndex,
                                  uint8_t&                    gpioIndex,
                                  nv::emulation::Pca9555Bank& bank);

    void
    gpio_index_to_drive_indices(uint8_t gpioIndex, uint8_t& driveIndex0, uint8_t& driveIndex1);

    // Member Constants
    // ---------------------------------------------
    // first i2c address for the e.1s drive gpio expander (drive index 0)
    constexpr static uint8_t E1sGpioExpanderAddress = 0x20;

    // required to be inputs to not go out of sync with actual GPIO direction
    constexpr static uint16_t InputPinMask = VppPrsntLBitmask | (VppPrsntLBitmask << 8U);

    // dummy data to send during i2c read errors
    constexpr static uint8_t DummyData = 0xff;

    // Member Variables
    // ---------------------------------------------
    // i2c driver
    nv::i2c::Port                 i2c_bus;
    sys::i2c::I2CSlaveDriver<VPP> i2c_driver;

    // interrupt pin
    nv::gpio::GpioPort interrupt_l_port;
    nv::gpio::GpioPin  interrupt_l_pin;

    // i/o pins for each drive
    std::array<nhp::E1sInputPins, nhp::NumE1sDrives>  e1s_pin_in;
    std::array<nhp::E1sOutputPins, nhp::NumE1sDrives> e1s_pin_out;

    // the PCA9555 gpio expander object for each e1s drive
    constexpr static uint8_t NumGpioExpanders = (nhp::NumE1sDrives + 1) / 2;
    std::array<emulation::Pca9555, NumGpioExpanders> e1s_gpio_expanders;

    // hot swap controller state machine per drive
    std::array<vpp::E1sHotSwap, nhp::NumE1sDrives> e1s_hssms;

    // saving pgood values for comparison
    std::array<bool, nhp::NumE1sDrives> pgood_vals;
    std::array<bool, nhp::NumE1sDrives> prsntL_vals;
    std::array<bool, nhp::NumE1sDrives> amberLED_vals;
    std::array<bool, nhp::NumE1sDrives> blueLED_vals;
};

}  // namespace nv::vpp

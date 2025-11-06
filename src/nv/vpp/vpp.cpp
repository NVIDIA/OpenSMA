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

#include "nv/vpp/vpp.h"
#include "nv/gpio/driver.h"
#include "sys/adc/adc.h"
#include NV_IPC_CONFIG_H

namespace nv::vpp {

VPP::VPP(const VppConfig& config, nv::ipc::Event* hotSwapEvent)
: i2c_bus(config.i2c_bus)
, i2c_driver_config({config.i2c_bus, i2c_callback, {}, this})
, i2c_driver()
, interrupt_l_port(config.interrupt_port)
, interrupt_l_pin(config.interrupt_pin)
, e1s_pin_in(config.input_pins)
, e1s_pin_out(config.output_pins)
, e1s_gpio_expanders{}
, e1s_hssms{}
, pgood_vals{}
, prsntL_vals{}
, amberLED_vals{}
, blueLED_vals{}
{
    // Initialize the I2C driver configuration
    static_assert(NumGpioExpanders <= sys::i2c::I2CSlaveDriver::NumI2cTargetAddresses,
                  "NumGpioExpanders is greater than NumI2cTargetAddresses");
    i2c_driver_config.target_addresses.fill(0);
    for (uint8_t gpioIndex = 0; gpioIndex < NumGpioExpanders; gpioIndex++) {
        i2c_driver_config.target_addresses.at(gpioIndex) = E1sGpioExpanderAddress + gpioIndex;
    }

    // Initialize the I2C driver
    i2c_driver = sys::i2c::I2CSlaveDriver(i2c_driver_config);

    pgood_vals.fill(false);
    prsntL_vals.fill(true);
    amberLED_vals.fill(false);
    blueLED_vals.fill(false);

    const auto status = nv::gpio::Driver::init_pin(interrupt_l_port,
                                                   interrupt_l_pin,
                                                   nv::gpio::Direction::Output,
                                                   nv::gpio::GpioState::High);
    if (status != nv::gpio::Status::Ok) {
        nv::error("Failed to initialize interrupt pin: port=%d, pin=%d\n",
                  interrupt_l_port,
                  interrupt_l_pin);
        // Consider appropriate error recovery or assertion here
    }

    for (uint8_t gpioIndex = 0; gpioIndex < NumGpioExpanders; gpioIndex++) {
        e1s_gpio_expanders.at(gpioIndex) = emulation::Pca9555(0x0000, 0x0000, InputPinMask);

        uint8_t bank0DriveIndex = 0, bank1DriveIndex = 0;
        gpio_index_to_drive_indices(gpioIndex, bank0DriveIndex, bank1DriveIndex);

        // coverity[cert_int31_c_violation] dont care about lost bits
        e1s_hssms.at(bank0DriveIndex) = E1sHotSwap(
            e1s_pin_out.at(bank0DriveIndex),
            hotSwapEvent,
            (bank0DriveIndex + (nv::nhp::NumE1sDrives * config.vppInstanceNum)));
        // coverity[cert_int31_c_violation] dont care about lost bits
        e1s_hssms.at(bank1DriveIndex) = E1sHotSwap(
            e1s_pin_out.at(bank1DriveIndex),
            hotSwapEvent,
            (bank1DriveIndex + (nv::nhp::NumE1sDrives * config.vppInstanceNum)));

        nv::gpio::Driver::init_interrupt(e1s_pin_in.at(bank0DriveIndex).prsnt_l_port,
                                         e1s_pin_in.at(bank0DriveIndex).prsnt_l_pin,
                                         nv::gpio::InterruptDetection::InterruptBothEdge,
                                         nv::gpio::InterruptSelect::InterruptSelect1);
        nv::gpio::Driver::init_interrupt(e1s_pin_in.at(bank1DriveIndex).prsnt_l_port,
                                         e1s_pin_in.at(bank1DriveIndex).prsnt_l_pin,
                                         nv::gpio::InterruptDetection::InterruptBothEdge,
                                         nv::gpio::InterruptSelect::InterruptSelect1);

        gpio_interrupt(e1s_pin_in.at(bank0DriveIndex).prsnt_l_port,
                       e1s_pin_in.at(bank0DriveIndex).prsnt_l_pin,
                       bank0DriveIndex);
        gpio_interrupt(e1s_pin_in.at(bank1DriveIndex).prsnt_l_port,
                       e1s_pin_in.at(bank1DriveIndex).prsnt_l_pin,
                       bank1DriveIndex);
    }
    i2c_driver.start();

    nv::info("Finished VPP initialization for %d drives\n", nhp::NumE1sDrives);
}

VPP::VPP()
: i2c_bus()
, i2c_driver_config()
, i2c_driver()
, interrupt_l_port()
, interrupt_l_pin()
, e1s_pin_in()
, e1s_pin_out()
, e1s_gpio_expanders{}
, e1s_hssms{}
, pgood_vals{}
, prsntL_vals{}
, amberLED_vals{}
, blueLED_vals{}
{
    //
}

void VPP::gpio_interrupt([[maybe_unused]] nv::gpio::GpioPort port,
                         [[maybe_unused]] nv::gpio::GpioPin  pin,
                         uint8_t                             driveIndex)
{
    // only prsntL is a gpio
    uint8_t                prsnt_l_temp = 0;
    const nv::gpio::Status status       = nv::gpio::Driver::read(
        e1s_pin_in.at(driveIndex).prsnt_l_port,
        e1s_pin_in.at(driveIndex).prsnt_l_pin,
        prsnt_l_temp);
    if (status != gpio::Status::Ok) {
        nv::error("Failed to read prsntL for drive %d\n", driveIndex);
        return;  // Don't update the value if read failed
    }
    prsntL_vals.at(driveIndex) = (prsnt_l_temp != 0);

    // updates prsntL in gpio expander
    // coverity[cert_int31_c_violation] dont care about lost bits
    const uint8_t              newPinVals = (static_cast<uint8_t>(prsntL_vals.at(driveIndex))
                                << VppPrsntLBit);
    uint8_t                    gpioIndex  = 0;
    nv::emulation::Pca9555Bank gpioBank   = nv::emulation::Pca9555Bank::Zero;
    drive_to_gpio_index_bank(driveIndex, gpioIndex, gpioBank);
    e1s_gpio_expanders.at(gpioIndex).update_input_pins(newPinVals, VppPrsntLBitmask, gpioBank);

    updateInterruptPin();
    updateHotSwap(driveIndex);
}

void VPP::adc_interrupt([[maybe_unused]] sys::adc::AdcPeripheral peripheral,
                        uint8_t                                  driveIndex,
                        uint16_t                                 value)
{
    // nv::info("Drive%d vmon: %x\n", driveIndex, value);  // DEBUG

    // NOLINTNEXTLINE: oldPgood is being initalized
    const bool oldPgood       = pgood_vals.at(driveIndex);
    pgood_vals.at(driveIndex) = value > nhp::PgoodThreshold;
    if (pgood_vals.at(driveIndex) != oldPgood) {
        gpio_interrupt(0U, 0U, driveIndex);
    }
}

void VPP::i2c_callback(uint8_t&                                                   address,
                       bool                                                       is_read,
                       std::array<uint8_t, sys::i2c::I2CSlaveDriver::BufferSize>& buffer,
                       size_t&                                                    data_size,
                       void* this_task_instance,
                       bool  new_transaction)
{
    // nv::info("i2c callback r/w:%d addr:%x\n", is_read, address);
    if (is_read) {
        // NOLINTNEXTLINE: no way around this without function pointers
        static_cast<VPP*>(this_task_instance)
            ->i2c_read(address, buffer, data_size, new_transaction);
    }
    else {
        // NOLINTNEXTLINE: no way around this without function pointers
        static_cast<VPP*>(this_task_instance)->i2c_write(address, buffer, data_size);
    }
}
void VPP::i2c_read(uint8_t&                                                   address,
                   std::array<uint8_t, sys::i2c::I2CSlaveDriver::BufferSize>& tx_buffer,
                   size_t&                                                    tx_data_size,
                   bool                                                       new_transaction)
{
    uint8_t reg_output = 0;
    uint8_t gpioIndex  = 0;
    if (i2c_address_to_gpio_index(address, gpioIndex)) {
        e1s_gpio_expanders.at(gpioIndex).i2c_read(reg_output, new_transaction);
        tx_buffer.at(0) = reg_output;
        tx_data_size    = 1;

        updateInterruptPin();
    }
    else {
        nv::error("Invalid address I2C read to NHP\n");
        tx_buffer.at(0) = DummyData;
        tx_data_size    = 1;
    }
}
void VPP::i2c_write(uint8_t&                                                   address,
                    std::array<uint8_t, sys::i2c::I2CSlaveDriver::BufferSize>& rx_buffer,
                    size_t&                                                    rx_data_size)
{
    uint8_t gpioIndex = 0;
    if (i2c_address_to_gpio_index(address, gpioIndex)) {
        // coverity[cert_int31_c_violation] datasize cant be more than 256
        e1s_gpio_expanders.at(gpioIndex).i2c_write(rx_buffer, rx_data_size);

        uint8_t bank0DriveIndex = 0, bank1DriveIndex = 0;
        gpio_index_to_drive_indices(gpioIndex, bank0DriveIndex, bank1DriveIndex);

        uint8_t bank0Reg = 0;
        uint8_t bank1Reg = 0;
        e1s_gpio_expanders.at(gpioIndex).get_pin_states(bank0Reg,
                                                        nv::emulation::Pca9555Bank::Zero);
        e1s_gpio_expanders.at(gpioIndex).get_pin_states(bank1Reg,
                                                        nv::emulation::Pca9555Bank::One);
        // bool bank0DriveAmberLedOld = amberLED_vals.at(driveIndexBank0);
        // bool bank0DriveBlueLedOld = blueLED_vals.at(driveIndexBank0);
        // bool bank1DriveAmberLedOld = amberLED_vals.at(driveIndexBank1);
        // bool bank1DriveBlueLedOld = blueLED_vals.at(driveIndexBank1);
        amberLED_vals.at(bank0DriveIndex) = bank0Reg & VppFaultedBitMask;
        blueLED_vals.at(bank0DriveIndex)  = bank0Reg & VppFaultedBitMask;

        updateHotSwap(bank0DriveIndex);
        updateHotSwap(bank1DriveIndex);

        updateInterruptPin();
    }
    else {
        nv::error("Invalid address I2C write to NHP\n");
    }
}

void VPP::hotSwapTimerInterrupt(uint8_t driveIndex)
{
    e1s_hssms.at(driveIndex)
        .updateStateMachine(pgood_vals.at(driveIndex),
                            prsntL_vals.at(driveIndex),
                            amberLED_vals.at(driveIndex),
                            blueLED_vals.at(driveIndex),
                            true);
}

void VPP::updateInterruptPin()
{
    for (uint8_t gpioIndex = 0; gpioIndex < NumGpioExpanders; gpioIndex++) {
        if (!e1s_gpio_expanders.at(gpioIndex).get_interrupt_l_state()) {
            nv::gpio::Driver::write(interrupt_l_port, interrupt_l_pin, 0U);
            return;
        }
    }
    nv::gpio::Driver::write(interrupt_l_port, interrupt_l_pin, 1U);
}

void VPP::updateHotSwap(uint8_t driveIndex)
{
    e1s_hssms.at(driveIndex)
        .updateStateMachine(pgood_vals.at(driveIndex),
                            prsntL_vals.at(driveIndex),
                            amberLED_vals.at(driveIndex),
                            blueLED_vals.at(driveIndex));
}

bool VPP::i2c_address_to_gpio_index(uint8_t address, uint8_t& gpioIndex)
{
    // coverity[cert_int31_c_violation] dont care about lost bits
    gpioIndex = address - E1sGpioExpanderAddress;
    if (gpioIndex >= NumGpioExpanders) {
        return false;
    }
    return true;
}

void VPP::drive_to_gpio_index_bank(uint8_t                     driveIndex,
                                   uint8_t&                    gpioIndex,
                                   nv::emulation::Pca9555Bank& bank)
{
    gpioIndex = driveIndex / 2;
    bank      = (driveIndex % 2 == 0) ? nv::emulation::Pca9555Bank::Zero
                                      : nv::emulation::Pca9555Bank::One;
}

void VPP::gpio_index_to_drive_indices(uint8_t  gpioIndex,
                                      uint8_t& driveIndex0,
                                      uint8_t& driveIndex1)
{
    // coverity[cert_int31_c_violation] dont care about lost bits
    driveIndex0 = gpioIndex * 2;
    // coverity[cert_int31_c_violation] dont care about lost bits
    driveIndex1 = driveIndex0 + 1;
}

}  // namespace nv::vpp
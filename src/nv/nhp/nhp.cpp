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

#include "nv/nhp/nhp.h"

#include "nv/gpio/common.h"
#include "nv/gpio/driver.h"
#include "nv/nv.h"
#include "sys/adc/adc.h"

namespace nv::nhp {

NHP::NHP(const NhpConfig& config)
: i2c_bus(config.i2c_bus)
, i2c_driver_config(sys::i2c::I2CSlaveDriver::Config{
      config.i2c_bus, i2c_callback, is_address_valid_callback, this})
, i2c_driver(i2c_driver_config)
, interrupt_l_port(config.interrupt_port)
, interrupt_l_pin(config.interrupt_pin)
, interrupt_l_aggregator(
      InterruptGpioXpndrInitVal, InterruptRequiredOutputs, InterruptRequiredInputs)
, e1s_pin_in(config.input_pins)
, e1s_pin_out(config.output_pins)
, e1s_gpio_expanders()
, e1s_hssms{}
, pgood_vals{}
{
    pgood_vals.fill(false);
    nv::gpio::Driver::init_pin(interrupt_l_port,
                               interrupt_l_pin,
                               nv::gpio::Direction::Output,
                               nv::gpio::GpioState::High);

    for (uint8_t i = 0; i < NumE1sDrives; i++) {
        nv::gpio::Driver::init_interrupt(e1s_pin_in.at(i).prsnt_l_port,
                                         e1s_pin_in.at(i).prsnt_l_pin,
                                         nv::gpio::InterruptDetection::InterruptBothEdge,
                                         nv::gpio::InterruptSelect::InterruptSelect1);

        bool pgood   = false;
        bool prsnt_l = true;
        read_input_pins(i, prsnt_l, pgood);
        // coverity[cert_int31_c_violation] forced to do this by our gpio api
        const uint16_t InitialPinConfig = 0x0000U
                                        | static_cast<uint16_t>(static_cast<uint16_t>(prsnt_l)
                                                                << NhpPrsnt0LBit)
                                        | static_cast<uint16_t>(static_cast<uint16_t>(pgood)
                                                                << NhpPwrGdBit);

        e1s_gpio_expanders.at(i) = emulation::Pca9555(InitialPinConfig, 0x0000, InputPinMask);

        e1s_hssms.at(i) = NhpE1sHotSwap(e1s_pin_out.at(i), InitialPinConfig);
    }

    i2c_driver.start();

    nv::info("Finished NHP initialization for %d drives\n", NumE1sDrives);
}

NHP::NHP()
: i2c_bus()
, i2c_driver_config()
, i2c_driver()
, interrupt_l_port()
, interrupt_l_pin()
, interrupt_l_aggregator()
, e1s_pin_in()
, e1s_pin_out()
, e1s_gpio_expanders()
, e1s_hssms{}
, pgood_vals{}
{
    //
}

void NHP::gpio_interrupt([[maybe_unused]] nv::gpio::GpioPort port,
                         [[maybe_unused]] nv::gpio::GpioPin  pin,
                         uint8_t                             drive_num)
{
    bool prsnt_l = false;
    bool pgood   = false;
    read_input_pins(drive_num, prsnt_l, pgood);

    // coverity[cert_int31_c_violation] forced to do this by our gpio api
    const uint16_t PinUpdateValue = 0x0000U
                                  | static_cast<uint16_t>(static_cast<uint16_t>(prsnt_l)
                                                          << NhpPrsnt0LBit)
                                  | static_cast<uint16_t>(static_cast<uint16_t>(pgood)
                                                          << NhpPwrGdBit);
    e1s_gpio_expanders.at(drive_num).update_input_pins(PinUpdateValue, InputPinMask);

    propagate_gpio_expander_change(drive_num);
}

void NHP::adc_interrupt([[maybe_unused]] sys::adc::AdcPeripheral peripheral,
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

bool NHP::is_address_valid_callback(uint8_t& address, void* this_task_instance)
{
    // NOLINTNEXTLINE: no way around this with task base class
    return static_cast<NHP*>(this_task_instance)->is_valid_address(address);
}
bool NHP::is_valid_address(const uint8_t& address)
{
    uint8_t drive_index = 0;
    if ((address == InterruptAggregatorAddress)
        || (i2c_address_to_drive_index(address, drive_index))) {
        return true;
    }
    // invalid address
    return false;
}

void NHP::i2c_callback(uint8_t&                                                   address,
                       bool                                                       is_read,
                       std::array<uint8_t, sys::i2c::I2CSlaveDriver::BufferSize>& buffer,
                       size_t&                                                    data_size,
                       void* this_task_instance,
                       bool  new_transaction)
{
    // nv::info("i2c callback r/w:%d addr:%x\n", is_read, address);
    if (is_read) {
        // NOLINTNEXTLINE: no way around this task base class
        static_cast<NHP*>(this_task_instance)
            ->i2c_read(address, buffer, data_size, new_transaction);
    }
    else {
        // NOLINTNEXTLINE: no way around this with task base class
        static_cast<NHP*>(this_task_instance)->i2c_write(address, buffer, data_size);
    }
}
void NHP::i2c_read(uint8_t&                                                   address,
                   std::array<uint8_t, sys::i2c::I2CSlaveDriver::BufferSize>& tx_buffer,
                   size_t&                                                    tx_data_size,
                   bool                                                       new_transaction)
{
    uint8_t reg_output  = 0;
    uint8_t drive_index = NumE1sDrives;

    if (address == InterruptAggregatorAddress) {
        interrupt_l_aggregator.i2c_read(reg_output, new_transaction);
        tx_buffer.at(0) = reg_output;
        tx_data_size    = 1;

        push_interrupt_expander_pin();
    }
    else if (i2c_address_to_drive_index(address, drive_index)) {
        e1s_gpio_expanders.at(drive_index).i2c_read(reg_output, new_transaction);
        tx_buffer.at(0) = reg_output;
        tx_data_size    = 1;

        propagate_gpio_expander_change(drive_index);
    }
    else {
        nv::error("Invalid address I2C read to NHP\n");
        tx_buffer.at(0) = DummyData;
        tx_data_size    = 1;
    }
}
void NHP::i2c_write(uint8_t&                                                   address,
                    std::array<uint8_t, sys::i2c::I2CSlaveDriver::BufferSize>& rx_buffer,
                    size_t&                                                    rx_data_size)
{
    uint8_t drive_index = 0;

    if (address == InterruptAggregatorAddress) {
        // coverity[cert_int31_c_violation] dont care about lost bits
        interrupt_l_aggregator.i2c_write(rx_buffer, rx_data_size);

        push_interrupt_expander_pin();
    }
    else if (i2c_address_to_drive_index(address, drive_index)) {
        // coverity[cert_int31_c_violation] dont care about lost bits
        e1s_gpio_expanders.at(drive_index).i2c_write(rx_buffer, rx_data_size);

        propagate_gpio_expander_change(drive_index);
    }
    else {
        nv::error("Invalid address I2C write to NHP\n");
    }
}

void NHP::push_interrupt_expander_pin()
{
    // coverity[cert_int31_c_violation] forced to do this by our gpio api
    const nv::gpio::Status GpioStatus = nv::gpio::Driver::write(
        interrupt_l_port, interrupt_l_pin, interrupt_l_aggregator.get_interrupt_l_state());
    if (GpioStatus != nv::gpio::Status::Ok) {
        nv::error("Error %x writing gpio port/pin %x/%x\n",
                  GpioStatus,
                  interrupt_l_port,
                  interrupt_l_pin);
    }
}

void NHP::propagate_gpio_expander_change(uint8_t drive_num)
{
    // NOLINTNEXTLINE: control_register is modified in get_pin_states
    uint16_t control_register = 0x0;
    e1s_gpio_expanders.at(drive_num).get_pin_states(control_register);

    e1s_hssms.at(drive_num).update_control_register(control_register);

    // shifts by 8 since interrupts start on bit 8
    // coverity[cert_int31_c_violation] dont care about lost bits
    const uint16_t CurrentIntLBitMask = 1U << static_cast<uint16_t>(
                                            drive_num + InterruptAggregatorReservedBits);
    // NOLINTNEXTLINE: IntL is initialized below
    // coverity[cert_int31_c_violation] dont care about lost bits
    const uint16_t IntL = static_cast<uint16_t>(
                              e1s_gpio_expanders.at(drive_num).get_interrupt_l_state())
                       << static_cast<uint16_t>(drive_num + InterruptAggregatorReservedBits);

    interrupt_l_aggregator.update_input_pins(IntL, CurrentIntLBitMask);

    push_interrupt_expander_pin();

    // debugging
    nv::info("Drive %d Ctrl Reg Change: 0x%x\n", drive_num, control_register);
}

void NHP::read_input_pins(uint8_t drive_index, bool& prsnt_l, bool& pgood)
{
    uint8_t                prsnt_l_temp = 0;
    const nv::gpio::Status status       = nv::gpio::Driver::read(
        e1s_pin_in.at(drive_index).prsnt_l_port,
        e1s_pin_in.at(drive_index).prsnt_l_pin,
        prsnt_l_temp);
    if (status != nv::gpio::Status::Ok) {
        nv::error("Failed to read prsntL for drive %d\n", drive_index);
    }
    else {
        prsnt_l = (prsnt_l_temp != 0);
    }

    pgood = pgood_vals.at(drive_index);
}

bool NHP::i2c_address_to_drive_index(uint8_t address, uint8_t& drive_index)
{
    if (address < E1sGpioExpanderAddress) {
        return false;
    }

    drive_index = address - E1sGpioExpanderAddress;

    if (drive_index >= NumE1sDrives) {
        return false;
    }

    return true;
}

}  // namespace nv::nhp
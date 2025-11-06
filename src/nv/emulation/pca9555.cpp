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

#include "nv/emulation/pca9555.h"
#include "nv/nv.h"

namespace nv::emulation {

Pca9555::Pca9555(uint16_t initial_input_values,
                 uint16_t required_output_mask,
                 uint16_t required_input_mask)
: _active_register(Input)
, _active_bank(Zero)
, _new_command(true)
, interrupt_low_vector({InterruptVectorDefault, InterruptVectorDefault})
// coverity[cert_int31_c_violation] dont care about lost bits
, state_at_last_interrupt_check({static_cast<uint8_t>(initial_input_values),
                                 static_cast<uint8_t>(initial_input_values >> 8U)})
// coverity[cert_int31_c_violation] dont care about lost bits
, _gpio_pin_state({static_cast<uint8_t>(initial_input_values),
                   static_cast<uint8_t>(initial_input_values >> 8U)})
, _gpio_output({OutputValueInit, OutputValueInit})
, _gpio_inversion_enable({InversionEnableInit, InversionEnableInit})
// coverity[cert_int31_c_violation] dont care about lost bits
, _gpio_direction_config(
      {static_cast<uint8_t>(
           DirectionInit & static_cast<uint8_t>(~static_cast<uint8_t>(required_output_mask))),
       static_cast<uint8_t>(
           DirectionInit
           & static_cast<uint16_t>(~static_cast<uint8_t>(required_output_mask >> 8U)))})
// coverity[cert_int31_c_violation] dont care about lost bits
, _required_outputs({static_cast<uint8_t>(required_output_mask),
                     static_cast<uint8_t>(required_output_mask >> 8U)})
// coverity[cert_int31_c_violation] dont care about lost bits
, _required_inputs({static_cast<uint8_t>(required_input_mask),
                    static_cast<uint8_t>(required_input_mask >> 8U)})
{
    nv::info("Finished PCA9555 initialization\n");
}

Pca9555::Pca9555()
: _active_register(Input)
, _active_bank(Zero)
, _new_command(true)
, interrupt_low_vector()
, state_at_last_interrupt_check()
, _gpio_pin_state()
, _gpio_output()
, _gpio_inversion_enable()
, _gpio_direction_config()
, _required_outputs()
, _required_inputs()
{
    return;
}

void Pca9555::i2c_read(uint8_t& return_data, bool new_transaction)
{
    // if (new i2c transaction) reset to register and bank to read to INPUT0
    if (!_new_command && new_transaction) {
        _active_register = Input;
        _active_bank     = Zero;
        // nv::info("rst cmd\n");
    }
    _new_command = false;

    switch (_active_register) {
        case Input: {
            return_data = _gpio_pin_state.at(_active_bank)
                        ^ _gpio_inversion_enable.at(_active_bank);
            // reset interrupts for that bank
            interrupt_low_vector.at(_active_bank) = InterruptVectorDefault;
            next_bank(_active_bank);
            break;
        }
        case Output: {
            return_data = _gpio_output.at(_active_bank);
            next_bank(_active_bank);
            break;
        }
        case Invert: {
            return_data = _gpio_inversion_enable.at(_active_bank);
            next_bank(_active_bank);
            break;
        }
        case Direction: {
            return_data = _gpio_direction_config.at(_active_bank);
            next_bank(_active_bank);
            break;
        }
        default: {
            nv::warn("Ignoring invalid PCA9555 read command Register: %x Bank: %x\n",
                     _active_register,
                     _active_bank);
            return_data = DummyData;
            break;
        }
    }
}

void Pca9555::i2c_write(std::array<uint8_t, sys::i2c::I2CSlaveDriver::BufferSize>& i2c_data,
                        uint8_t                                                    data_length)
{
    // FIRST BYTE IS COMMAND (register to write to)
    const auto CommandRegister = static_cast<Pca9555Register>(i2c_data.at(0) / 2);
    auto command_bank = static_cast<Pca9555Bank>(i2c_data.at(0) % 2);  // NOLINT: not const

    // IF NO WRITE DATA THAT REGISTER WILL BE READ ON NEXT RX
    // set to new command so nexxt read knows whether to set to default (INPUT0) or not
    if (data_length == 1) {
        _new_command     = true;
        _active_register = CommandRegister;
        _active_bank     = command_bank;
        // nv::info("set cmd %x %x\n", _active_register, _active_bank);
        return;
    }

    // FLIP FLOPS BETWEEN THE REGISTER PAIR for each byte written
    // Therefore ONLY CARE ABOUT LAST 2 or 1 BYTE(S)
    uint8_t start_index = 1;
    if (data_length > 3) {
        start_index = data_length - 2;
    }

    for (uint8_t i = start_index; i < data_length; i++) {
        const uint8_t Item = i2c_data.at(i);

        // nv::info("write Item:%x reg:%x bank:%x\n", Item, CommandRegister, command_bank);
        switch (CommandRegister) {
            case Input: {
                nv::warn("trying to write to PCA9555 input pin\n");
                return;
            }
            case Output: {
                // coverity[cert_int31_c_violation] using enums to not allow overflow
                _gpio_output.at(command_bank) = Item;
                propagate_output_or_direction_change(command_bank);
                next_bank(command_bank);
                break;
            }
            case Invert: {
                // coverity[cert_int31_c_violation] using enums to not allow overflow
                _gpio_inversion_enable.at(command_bank) = Item;
                next_bank(command_bank);
                break;
            }
            case Direction: {
                correct_direction_violations_and_apply(Item, command_bank);
                propagate_output_or_direction_change(command_bank);
                next_bank(command_bank);
                break;
            }
            default: {
                nv::warn("Ignoring invalid PCA9555 command %x\n", i2c_data.at(0));
                break;
            }
        }
    }
}

// Interact with task in 16bit register but i2c is 8 bit register pairs
void Pca9555::update_input_pins(uint16_t target_pin_values, uint16_t pin_mask)
{
    // coverity[cert_int31_c_violation] dont care about lost bits
    uint16_t direction_vector = (_gpio_direction_config[One] << 8U)
                              + _gpio_direction_config[Zero];
    if ((pin_mask & static_cast<uint16_t>(~direction_vector)) != 0) {
        nv::warn("Parent task tried to change PCA9555 output register\nDirection vector:%x",
                 direction_vector);
    }

    // Modify pin mask to only include pins configured as inputs
    pin_mask = pin_mask & direction_vector;

    // Update pin states: preserve current states for unmodified pins
    // - OldPinValues represents the current GPIO states
    // - NewPinValues computes the new states based on the mask and target values
    // coverity[cert_int31_c_violation] dont care about lost bits
    const uint16_t OldPinValues = static_cast<uint16_t>(_gpio_pin_state[One] << 8U)
                                | _gpio_pin_state.at(Zero);
    // coverity[cert_int31_c_violation] dont care about lost bits
    const uint16_t NewPinValues = static_cast<uint16_t>(OldPinValues
                                                        & static_cast<uint16_t>(~pin_mask))
                                | static_cast<uint16_t>(target_pin_values & pin_mask);

    // Split the updated 16-bit value back into the two 8-bit banks
    // coverity[cert_int31_c_violation] dont care about lost bits
    _gpio_pin_state.at(Zero) = NewPinValues;
    // coverity[cert_int31_c_violation] dont care about lost bits
    _gpio_pin_state.at(One) = NewPinValues >> 8U;

    update_interrupts(Zero);
    update_interrupts(One);
}
void Pca9555::update_input_pins(uint8_t target_pin_values, uint8_t pin_mask, Pca9555Bank bank)
{
    uint8_t direction_vector = _gpio_direction_config.at(bank);
    if ((pin_mask & static_cast<uint8_t>(~direction_vector)) != 0) {
        nv::warn("Parent task tried to change PCA9555 output register\nDirection vector:%x",
                 direction_vector);
    }

    // Modify pin mask to only include pins configured as inputs
    pin_mask = pin_mask & direction_vector;

    // Update pin states: preserve current states for unmodified pins
    // - OldPinValues represents the current GPIO states
    // - NewPinValues computes the new states based on the mask and target values
    const auto OldPinValues = static_cast<uint8_t>(_gpio_pin_state.at(bank));
    const auto NewPinValues = static_cast<uint8_t>(OldPinValues
                                                   & static_cast<uint8_t>(~pin_mask))
                            | static_cast<uint8_t>(target_pin_values & pin_mask);

    _gpio_pin_state.at(bank) = NewPinValues;

    update_interrupts(bank);
}

void Pca9555::get_pin_states(uint16_t& pin_states)
{
    pin_states = static_cast<uint16_t>(static_cast<uint16_t>(_gpio_pin_state.at(One)) << 8U)
               | static_cast<uint16_t>(_gpio_pin_state.at(Zero));
}
void Pca9555::get_pin_states(uint8_t& pin_states, Pca9555Bank bank)
{
    pin_states = _gpio_pin_state.at(bank);
}

void Pca9555::get_pin_directions(uint16_t& direction_vector)
{
    direction_vector = static_cast<uint16_t>(
                           static_cast<uint16_t>(_gpio_direction_config.at(One)) << 8U)
                     | static_cast<uint16_t>(_gpio_direction_config.at(Zero));
}

bool Pca9555::get_interrupt_l_state()
{
    return ((interrupt_low_vector.at(One) == InterruptVectorDefault)
            && (interrupt_low_vector.at(Zero) == InterruptVectorDefault));
}

// interrupts are active low
void Pca9555::propagate_output_or_direction_change(Pca9555Bank bank)
{
    // sets pin to output if direction is output
    // keeps pin at same state if direction is input
    _gpio_pin_state.at(bank) = static_cast<uint8_t>(
                                   _gpio_output.at(bank)
                                   & static_cast<uint8_t>(~_gpio_direction_config.at(bank)))
                             | static_cast<uint8_t>(_gpio_pin_state.at(bank)
                                                    & _gpio_direction_config.at(bank));

    update_interrupts(bank);
}

// Applies interrupt changes after output, direction, or input pin changes
// Made this very verbose with a lot of variables to be easier to read and verify logic
void Pca9555::update_interrupts(Pca9555Bank bank)
{
    // Tracking interrupts internally with active high to make it easier to read
    const uint8_t CurrentInterruptVector = ~interrupt_low_vector.at(bank);

    // Track what interrupts were active as we modify the current interrupt vector
    uint8_t new_interrupt_vector = CurrentInterruptVector;

    // tracks what pins changed state
    const uint8_t PinsThatChangedState = state_at_last_interrupt_check.at(bank)
                                       ^ _gpio_pin_state.at(bank);

    // Need to remove interrupt for pins that reverted to their old state and has an
    //     interrupt from before
    const uint8_t InterruptPinsThatRevertedState = CurrentInterruptVector
                                                 & PinsThatChangedState;
    new_interrupt_vector &= static_cast<uint8_t>(~InterruptPinsThatRevertedState);

    // Need to set interrupt for pins that changed from last state, is an input now, and
    //     didnt have an interrupt from before
    // this includes outputs that turned into inputs
    //     (but not inputs that turned into outputs)
    const uint8_t NewInterruptBits = static_cast<uint8_t>(PinsThatChangedState
                                                          & _gpio_direction_config.at(bank))
                                   & static_cast<uint8_t>(~CurrentInterruptVector);
    new_interrupt_vector |= NewInterruptBits;

    // saves current state of bits (for reverting interrupts next time)
    state_at_last_interrupt_check.at(bank) = _gpio_pin_state.at(bank);

    // converts to active low and applies to interrupt register
    interrupt_low_vector.at(bank) = ~new_interrupt_vector;
}

// should not violate the allowed output config value
void Pca9555::correct_direction_violations_and_apply(const uint8_t target_direction,
                                                     Pca9555Bank   bank)
{
    // overrides direction with required inputs
    // overrides direction with required outputs
    _gpio_direction_config.at(bank) = static_cast<uint8_t>(target_direction
                                                           | _required_inputs.at(bank))
                                    & static_cast<uint8_t>(~_required_outputs.at(bank));

    if (target_direction != _gpio_direction_config.at(bank)) {
        nv::error(
            "Direction write command violated allowed inputs/outputs in PCA9555.\nTarget: %x "
            "Required Inputs: %x Required Outputs: %x Modified New Register: %x\n",
            target_direction,
            _required_inputs.at(bank),
            _required_outputs.at(bank),
            _gpio_direction_config.at(bank));
    }
}

void Pca9555::next_bank(Pca9555Bank& bank)
{
    bank = (bank == Zero) ? One : Zero;  // goes to other side of bank for next read
}

}  // namespace nv::emulation

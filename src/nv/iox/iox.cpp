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

#include <cstring>

#include "iox.h"
#include "nv/common/debug.h"
#include "nv/logger/log.h"
#include "nv/gpio/driver.h"
#include "nv/nv.h"
#include "task.h"

namespace nv::iox {

// Static GPIO structure for CP2112 emulation - now as static member of Iox class
NV_SHARED_BSS cp2112_gpio_struct_t Iox::g_gpio = {};

std::array<std::array<uint8_t, Iox::PinsPerInstance>, Iox::NumInstance> Iox::ioxnmap{};

/** @note: make sure the direction definition is correct */
static_assert(std::to_underlying(nv::gpio::Direction::Input) == 0,
              "nv::gpio::Direction::Input must be 0");
static_assert(std::to_underlying(nv::gpio::Direction::Output) == 1,
              "nv::gpio::Direction::Output must be 1");

Iox::Iox(I2CAddress address, const std::array<PinConfig, pinNum>& configs)
: addr(address)
, pins(configs)
{
    /**
     * update Pca9555 internal registers based on pin configs
     *
     * @note: NOT handle changing gpio direction here, which should be
     *        done by the mcxn236 driver at the very beginning of main()
     *
     * @note: we assume the gpio direction is persistent throughout the program
     *        i.e., gpio direction never changes after the initial configuration
     */
    std::memset(regs.data(), 0x00, regNum);

    auto& input0    = regs.at(static_cast<uint8_t>(Register::InputPort0));
    auto& input1    = regs.at(static_cast<uint8_t>(Register::InputPort1));
    auto& output0   = regs.at(static_cast<uint8_t>(Register::OutputPort0));
    auto& output1   = regs.at(static_cast<uint8_t>(Register::OutputPort1));
    auto& polarity0 = regs.at(static_cast<uint8_t>(Register::Polarity0));
    auto& polarity1 = regs.at(static_cast<uint8_t>(Register::Polarity1));
    auto& config0   = regs.at(static_cast<uint8_t>(Register::Config0));
    auto& config1   = regs.at(static_cast<uint8_t>(Register::Config1));

    for (size_t i = 0; i < 8; ++i) {
        // get port config
        auto& loPortPin = pins.at(i + 0);
        auto& hiPortPin = pins.at(i + 8);

        // init non-privileged access for gpio pins
        if (loPortPin.port != nv::gpio::vrPort) {
            nv::gpio::Driver::init_nonpriv_access(loPortPin.port, loPortPin.pin);
        }
        if (hiPortPin.port != nv::gpio::vrPort) {
            nv::gpio::Driver::init_nonpriv_access(hiPortPin.port, hiPortPin.pin);
        }

        // run-time config gpio
        config_gpio(loPortPin);
        config_gpio(hiPortPin);

        /**
         * handle config register (gpio direction)
         *
         * @note: If a bit in this register is cleared (written with '0')
         *        the corresponding port pin is enabled as an output.
         *        If a bit in this register is set (written with '1')
         *        the corresponding port pin is enabled as an input with high-impedance output
         * driver
         *
         * @note: PCA9555 and MCXN236 has opposite direction definition
         *        PCA9555: output is 0, input is 1
         *        MCXN236: output is 1, input is 0
         *        thus, we need to invert the direction
         *
         * @note: for simplicity, we follow nv::gpio::Direction definition
         *        which is the same as the MCXN236 definition
         */
        // coverity[cert_int31_c_violation] - no lost bits at all
        config0 |= static_cast<uint8_t>((static_cast<uint8_t>(loPortPin.dir) & 0x01) << i);
        // coverity[cert_int31_c_violation] - no lost bits at all
        config1 |= static_cast<uint8_t>((static_cast<uint8_t>(hiPortPin.dir) & 0x01) << i);

        /**
         * handle input port register (gpio value)
         *
         * @note: input port register always has a valid value
         *        regardless of the gpio direction
         *        thus, read gpio directly
         *
         * @note: the value ALWAYS reflects the real gpio value
         *        unlike the value stored in the output port register
         */
        nv::gpio::GpioState lval = loPortPin.val;
        get_gpio(loPortPin.port, loPortPin.pin, lval);
        // coverity[cert_int31_c_violation] - no lost bits at all
        input0 |= static_cast<uint8_t>((static_cast<uint8_t>(lval) & 0x01) << i);

        nv::gpio::GpioState hval = hiPortPin.val;
        get_gpio(hiPortPin.port, hiPortPin.pin, hval);
        // coverity[cert_int31_c_violation] - no lost bits at all
        input1 |= static_cast<uint8_t>((static_cast<uint8_t>(hval) & 0x01) << i);

        /**
         * handle output port register (gpio value)
         *
         * This register is an output-only port.
         * It reflects the outgoing logic levels of the
         * pins defined as outputs by Registers 6 and 7.
         *
         * @note: Bit values in this register have no effect on pins defined
         *        as inputs.
         *
         * @note: Reads from this register reflect the value that is in the
         *        flip-flop controlling the output selection, NOT the actual pin value.
         */
        // coverity[cert_int31_c_violation] - no lost bits at all
        output0 |= static_cast<uint8_t>((static_cast<uint8_t>(loPortPin.val) & 0x01) << i);
        // coverity[cert_int31_c_violation] - no lost bits at all
        output1 |= static_cast<uint8_t>((static_cast<uint8_t>(hiPortPin.val) & 0x01) << i);

        if (loPortPin.dir == nv::gpio::Direction::Output) {
            set_gpio(loPortPin.port, loPortPin.pin, loPortPin.val);
        }
        if (hiPortPin.dir == nv::gpio::Direction::Output) {
            set_gpio(hiPortPin.port, hiPortPin.pin, hiPortPin.val);
        }

        /**
         * handle polarity register (gpio polarity)
         *
         * @note: This register is used to set the polarity of the GPIO pins.
         *        If a bit in this register is set (written with '1'),
         *        the corresponding pin is configured with a logic-inverted output.
         */
        polarity0 = 0x00;
        polarity1 = 0x00;
    }
}

Status Iox::access_reg_polarity(Operation op, Register reg, uint8_t& data)
{
    if (op == Operation::Read) {
        data = regs.at(static_cast<uint8_t>(reg));
    }
    else {
        regs.at(static_cast<uint8_t>(reg)) = data;
    }
    return Status::Ok;
}

/**
 * Access the config register
 *    1. read: read the config register
 *        @note: PCA9555 and MCXN236 has opposite direction definition
 *               PCA9555: output is 0, input is 1
 *               MCXN236: output is 1, input is 0
 *        @note: need to invert the direction as we are using MCXN236 definition
 *    2. write: write to the config register, always return Success.
 */
Status Iox::access_reg_config(Operation op, Register reg, uint8_t& data)
{
    if (op == Operation::Read) {
        data = ~regs.at(static_cast<uint8_t>(reg));
    }
    return Status::Ok;
}

Status Iox::access_reg_input_port(Operation op, Register reg, uint8_t& data, uint8_t num)
{
    if (op == Operation::Read) {
        const auto offset = (reg == Register::InputPort0) ? 0 : 8;

        // read from output port register in case there is virtual gpio
        data = regs.at((reg == Register::InputPort0)
                           ? static_cast<uint8_t>(Register::OutputPort0)
                           : static_cast<uint8_t>(Register::OutputPort1));

        // read gpio value if not virtual
        for (size_t i = 0; i < 8; ++i) {
            auto port = pins.at(i + offset).port;
            auto pin  = pins.at(i + offset).pin;

            nv::gpio::GpioState val{};
            if (port != nv::gpio::vrPort) {
                get_gpio(port, pin, val);
                // clear the bit in the data
                data &= ~(1 << i);
                // handle the stupid static analysis coverity warning
                const uint8_t _val = static_cast<uint8_t>(val) & 0x01;
                // coverity[cert_int31_c_violation] - no lost bits at all
                const uint8_t shifted_val  = _val << i;
                data                      |= shifted_val;
            }
        }

        // invert polarity
        data ^= (reg == Register::InputPort0)
                  ? regs.at(static_cast<uint8_t>(Register::Polarity0))
                  : regs.at(static_cast<uint8_t>(Register::Polarity1));

        // update the input register
        // TODO: remove the input register as it is not used ???
        regs.at(static_cast<uint8_t>(reg)) = data;

        /**
         * all the Iox instances share the same interrupt pin
         * so we can just pull up the interrupt pin here
         * whenever user read the input port register
         */
        // g_gpio.gpio_value |= (1U << num);
        set_gpio_bit(num, true);
    }
    else {
        nv::info("Pca9555 write input port reg is ignored\r\n");
    }
    return Status::Ok;
}

/**
 * Access the output port register
 *    1. read: read the output port register
 *    2. write: write to the output port register
 *        a. update the real gpio value only if the gpio is output
 *        b. update the output port register
 * @note:
 *   The output port register is an output-only port.
 *   It reflects the outgoing logic levels of the
 *   pins defined as outputs by Registers 6 and 7.
 *   Bit values in this register have no effect on pins defined as inputs.
 */
Status Iox::access_reg_output_port(Operation op, Register reg, uint8_t& data)
{
    if (op == Operation::Read) {
        data = regs.at(static_cast<uint8_t>(reg));
    }
    else {
        // write to the output port register
        regs.at(static_cast<uint8_t>(reg)) = data;

        // update the real gpio value only if the gpio is output
        const auto offset = (reg == Register::OutputPort0) ? 0 : 8;
        const auto config = (reg == Register::OutputPort0)
                              ? regs.at(static_cast<uint8_t>(Register::Config0))
                              : regs.at(static_cast<uint8_t>(Register::Config1));
        for (size_t i = 0; i < 8; ++i) {
            auto dir  = static_cast<nv::gpio::Direction>((config >> i) & 0x01);
            auto port = pins.at(i + offset).port;
            auto pin  = pins.at(i + offset).pin;
            if (dir == nv::gpio::Direction::Output) {
                set_gpio(port, pin, static_cast<nv::gpio::GpioState>((data >> i) & 0x01));
            }
        }
    }
    return Status::Ok;
}

Status Iox::access_reg(Operation op, Register reg, uint8_t& data, uint8_t num)
{
    auto status = Status::Ok;
    switch (reg) {
        // handle polarity register
        case Register::Polarity0:
        case Register::Polarity1: status = access_reg_polarity(op, reg, data); break;

        // handle config register
        case Register::Config0:
        case Register::Config1: status = access_reg_config(op, reg, data); break;

        // handle input port register
        case Register::InputPort0:
        case Register::InputPort1: status = access_reg_input_port(op, reg, data, num); break;

        // handle output port register
        case Register::OutputPort0:
        case Register::OutputPort1: status = access_reg_output_port(op, reg, data); break;

        // should not reach here
        default: status = Status::InvalidReg; break;
    }

    return status;
}

Status Iox::set_gpio(nv::gpio::GpioPort port, nv::gpio::GpioPin pin, nv::gpio::GpioState val)
{
    // return immediately if the gpio is virtual
    if (port == nv::gpio::vrPort) {
        return Status::Ok;
    }

    auto err = nv::gpio::Driver::write(port, pin, static_cast<uint8_t>(val));
    if (err != nv::gpio::Status::Ok) {
        return Status::InvalidWrite;
    }

    return Status::Ok;
}

Status Iox::get_gpio(nv::gpio::GpioPort port, nv::gpio::GpioPin pin, nv::gpio::GpioState& val)
{
    // Virtual GPIO has no hardware pin to read; preserve caller-supplied value
    if (port == nv::gpio::vrPort) {
        return Status::Ok;
    }

    uint8_t data = 0;
    auto    err  = nv::gpio::Driver::read(port, pin, data);
    if (err != nv::gpio::Status::Ok) {
        return Status::InvalidRead;
    }

    val = static_cast<nv::gpio::GpioState>(data);
    return Status::Ok;
}

Status Iox::config_gpio(const PinConfig& config)
{
    // get port and pin
    auto port = config.port;
    auto pin  = config.pin;

    // return immediately if the gpio is virtual
    if (port == nv::gpio::vrPort) {
        return Status::Ok;
    }

    // we are re-configuring the gpio
    // so always disable interrupt first
    nv::gpio::Driver::init_interrupt(port,
                                     pin,
                                     nv::gpio::InterruptDetection::InterruptDisabled,
                                     nv::gpio::InterruptSelect::InterruptSelect0);

    // config port
    nv::gpio::Driver::init_pin_cfg(port,
                                   pin,
                                   static_cast<nv::gpio::GpioPullDir>(config.pullDir),
                                   static_cast<nv::gpio::GpioPullStrength>(config.pullStrength),
                                   static_cast<nv::gpio::GpioOpenDrain>(config.openDrain));

    // config gpio
    if (nv::gpio::Driver::init_pin(port, pin, config.dir, config.val) != nv::gpio::Status::Ok) {
        nv::error("Failed to config gpio %d %d\n", port, pin);
        return Status::Error;
    }

    /** @note: interrupt is always enabled for input pin */
    if (config.dir == nv::gpio::Direction::Output) {
        return Status::Ok;
    }

    /** @note: we are emulating the PCA9555, so both-edge interrupt is always enabled for input
     * pin */
    nv::gpio::Driver::init_interrupt(port,
                                     pin,
                                     nv::gpio::InterruptDetection::InterruptBothEdge,
                                     nv::gpio::InterruptSelect::InterruptSelect0);

    return Status::Ok;
}

void Iox::iox_int_handler(nv::gpio::GpioPort port, uint32_t flag)
{
    // Check each bit in the flag to find which pins triggered interrupts
    for (uint32_t pin = 0; pin < PinsPerInstance; ++pin) {
        if (flag & (1U << pin)) {
            auto ioxn = Iox::get_ioxn(port, static_cast<nv::gpio::GpioPin>(pin));
            set_gpio_bit(ioxn, false);
        }
    }
}
// GPIO value management API implementations
void Iox::set_gpio_value(uint8_t value)
{
    g_gpio.gpio_value = value;
}

uint8_t Iox::get_gpio_value()
{
    return g_gpio.gpio_value;
}

void Iox::set_gpio_bit(uint8_t bit, bool set)
{
    if (set) {
        g_gpio.gpio_value |= (1U << bit);
    }
    else {
        g_gpio.gpio_value &= ~(1U << bit);
    }
}

bool Iox::get_gpio_bit(uint8_t bit)
{
    return (g_gpio.gpio_value & (1U << bit)) != 0;
}

}  // namespace nv::iox

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

#include <array>
#include <cstdint>

#include "nv/iox/common.h"

namespace nv::iox {
/**
 * @brief CP2112 GPIO structure for USB HID communication
 */
typedef struct cp2112_gpio_struct
{
    uint8_t gpio_value;
    uint8_t gpio_direction;
    uint8_t gpio_push_pull_open_drain;
    uint8_t gpio_special_function;
    uint8_t gpio_clock_divider;
} cp2112_gpio_struct_t;

/**
 * @brief PCA9555 Emulation Class
 */
class Iox
{
private:
    I2CAddress                    addr{};
    std::array<uint8_t, regNum>   regs{};
    std::array<PinConfig, pinNum> pins{};

    static constexpr uint32_t NumInstance     = 6;
    static constexpr uint32_t PinsPerInstance = 32;
    static std::array<std::array<uint8_t, PinsPerInstance>, NumInstance>
        ioxnmap;                         // [port][pin]
                                         // = ioxn
                                         // number
    static cp2112_gpio_struct_t g_gpio;  // Static GPIO structure for CP2112 emulation

public:
    Iox(){};
    Iox(I2CAddress address, const std::array<PinConfig, pinNum>& configs);

    /**
     * @brief Write to register
     *
     * @param reg Register address (0x00-0x07)
     * @param data Data to write
     *
     * @return n/a
     */
    Status write_reg(Register reg, uint8_t data, uint8_t num)
    {
        return access_reg(Operation::Write, reg, data, num);
    }

    /**
     * @brief Read from register
     *
     * @param reg Register address (0x00-0x07)
     * @return Data read from register
     */
    Status read_reg(Register reg, uint8_t& data, uint8_t num)
    {
        return access_reg(Operation::Read, reg, data, num);
    }

    /**
     * @brief handle interrupt
     */
    static void iox_int_handler(nv::gpio::GpioPort port, uint32_t flag);
    static auto get_ioxn(nv::gpio::GpioPort port, nv::gpio::GpioPin pin)
    {
        return ioxnmap.at(port).at(pin);
    };
    static void set_ioxn(nv::gpio::GpioPort port, nv::gpio::GpioPin pin, uint8_t ioxn)
    {
        ioxnmap.at(port).at(pin) = ioxn;
    };
    /**
     * @brief GPIO value management API
     */
    static void    set_gpio_value(uint8_t value);
    static uint8_t get_gpio_value();
    static void    set_gpio_bit(uint8_t bit, bool set);
    static bool    get_gpio_bit(uint8_t bit);

private:
    // access register
    Status access_reg(Operation op, Register reg, uint8_t& data, uint8_t num);
    // access register config
    Status access_reg_config(Operation op, Register reg, uint8_t& data);
    // access register polarity
    Status access_reg_polarity(Operation op, Register reg, uint8_t& data);
    // access register input port
    Status access_reg_input_port(Operation op, Register reg, uint8_t& data, uint8_t num);
    // access register output port
    Status access_reg_output_port(Operation op, Register reg, uint8_t& data);

    // access real GPIO
    Status get_gpio(nv::gpio::GpioPort port, nv::gpio::GpioPin pin, nv::gpio::GpioState& val);
    Status set_gpio(nv::gpio::GpioPort port, nv::gpio::GpioPin pin, nv::gpio::GpioState val);

    // config pin
    Status config_gpio(const PinConfig& config);
};

}  // namespace nv::iox

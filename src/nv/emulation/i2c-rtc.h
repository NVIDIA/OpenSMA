/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
#include <span>

#include "nv/common/literals.h"
#include "nv/emulation/emulation_device.h"
#include "sys/i2c/i2c_slave.h"

namespace nv::emulation {

using nv::operator""_bit;

/**
 * @brief I2C RTC Emulator
 * @details This class emulates an I2C RTC.
 *          The emulator always uses BCD and 24-hour mode.
 */
class I2cRtc final : public EmulationDevice
{
public:
    void i2c_read(uint8_t& return_data, bool new_transaction) override;

    void i2c_read(std::span<uint8_t> i2c_data, size_t& i2c_size, bool new_transaction) override;

    void i2c_read_virtual(std::span<uint8_t> i2c_data,
                          size_t&            i2c_size,
                          bool               new_transaction) override;

    void i2c_write(const std::array<uint8_t, sys::i2c::I2cSlaveBufferSize>& i2c_data,
                   uint8_t data_length) override;

    void i2c_write(std::span<const uint8_t> i2c_data) override;

    void i2c_write_virtual(std::span<const uint8_t> i2c_data) override;

    uint8_t get_seconds();
    uint8_t get_minutes();
    uint8_t get_hours();
    uint8_t get_day_of_week();
    uint8_t get_day_of_month();
    uint8_t get_month();
    uint8_t get_year();

private:
    /** @brief Decode a calendar field after removing reserved bits. */
    uint8_t decode_time_value(uint8_t value, uint8_t mask) const
    {
        const uint8_t masked = static_cast<uint8_t>(value & mask);
        return bcd_to_bin(masked);
    }

    /** @brief Compose the Status register */
    uint8_t get_status_register() const { return get_battery_voltage(); }

    /** @brief Encode the latest VBAT reading */
    static uint8_t get_battery_voltage();

    /**
     * @brief Convert a binary value in the range 0..99 to its packed BCD byte.
     */
    static constexpr uint8_t bin_to_bcd(uint8_t value)
    {
        return static_cast<uint8_t>(((value / 10U) << 4U) | (value % 10U));
    }

    /**
     * @brief Convert a packed BCD byte back to its binary value.
     */
    static constexpr uint8_t bcd_to_bin(uint8_t value)
    {
        return static_cast<uint8_t>(((value >> 4U) * 10U) + (value & 0x0FU));
    }

    constexpr static uint8_t YearWrap    = 100U;
    constexpr static uint8_t WeekDayMask = 0x07U;

    constexpr static uint8_t SecondsMinutesMask = 0x7FU;
    constexpr static uint8_t HoursValueMask     = 0x3FU;
    constexpr static uint8_t DayOfMonthMask     = 0x3FU;
    constexpr static uint8_t MonthMask          = 0x1FU;

    constexpr static uint8_t PartInfoValue = 0x02U;

    constexpr static uint32_t BatteryVoltageOffsetMv = 1500U;
    constexpr static uint32_t BatteryVoltageStepMv   = 200U;

    enum Register : uint8_t
    {
        Begin         = 0x00U,
        Seconds       = 0x00U,
        Minutes       = 0x02U,
        Hours         = 0x04U,
        DayOfTheWeek  = 0x06U,
        DayOfTheMonth = 0x07U,
        Month         = 0x08U,
        Year          = 0x09U,
        Control       = 0x0AU,
        Status        = 0x0BU,
        PartInfo      = 0x21U,
    };

    void i2c_write_block(std::span<const uint8_t> i2c_data, bool is_virtual);
    void i2c_read_byte(uint8_t& return_data, uint8_t register_address);
    void
    i2c_read_block(std::span<uint8_t> i2c_data, size_t& i2c_size, uint8_t register_address);

    enum ControlBit : uint8_t
    {
        Hour24Mode                  = 5_bit, /* HF */
        TimeRegistersWriteOwnership = 0_bit, /* TWO */
        ControlWritableMask         = TimeRegistersWriteOwnership,
    };

    enum StatusBit : uint8_t
    {
        BatteryVoltageMask = 2_bit | 1_bit | 0_bit,
    };

    uint8_t _current_register         = Begin;
    uint8_t _current_register_virtual = Begin;
    bool    _new_command              = true;
    bool    _new_command_virtual      = true;
    uint8_t _control_register         = Hour24Mode;
};

}  // namespace nv::emulation

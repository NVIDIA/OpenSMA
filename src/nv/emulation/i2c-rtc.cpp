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
#include "nv/emulation/i2c-rtc.h"

#include <algorithm>

#include "nv/volt_mon/vbat.h"
#include "sys/rtc/rtc.h"

namespace nv::emulation {

void I2cRtc::i2c_write(const std::array<uint8_t, sys::i2c::I2cSlaveBufferSize>& i2c_data,
                       uint8_t                                                  data_length)
{
    i2c_write(std::span<const uint8_t>{i2c_data}.first(data_length));
}

void I2cRtc::i2c_write(std::span<const uint8_t> i2c_data)
{
    if (i2c_data.empty()) {
        return;
    }

    if (i2c_data.size() == 1U) {
        // Pointer-set only; honor it on the next read transaction.
        _current_register = i2c_data[0];
        _new_command      = true;
        return;
    }

    i2c_write_block(i2c_data, false);
}

void I2cRtc::i2c_write_virtual(std::span<const uint8_t> i2c_data)
{
    if (i2c_data.empty()) {
        return;
    }

    if (i2c_data.size() == 1U) {
        // Pointer-set only; honor it on the next read transaction.
        _current_register_virtual = i2c_data[0];
        _new_command_virtual      = true;
        return;
    }

    i2c_write_block(i2c_data, true);
}

void I2cRtc::i2c_write_block(std::span<const uint8_t> i2c_data, bool is_virtual)
{
    const uint8_t start_register = i2c_data[0];

    const auto        register_data = i2c_data.subspan(1U);
    const std::size_t end_register  = static_cast<std::size_t>(start_register)
                                   + register_data.size() - 1U;
    const bool two            = (_control_register & TimeRegistersWriteOwnership) != 0U;
    const bool can_write_time = is_virtual ? two : !two;
    const bool contains_time  = start_register <= Year
                            && (register_data.size() > 1U
                                || (start_register != Seconds + 1U
                                    && start_register != Minutes + 1U
                                    && start_register != Hours + 1U));

    if (can_write_time && contains_time) {
        // Time register write access is controller by TWO control register bit
        sys::rtc::datetime_t datetime{};
        sys::rtc::Driver::get_datetime(datetime);

        if (Seconds >= start_register && Seconds <= end_register) {
            const uint8_t value = register_data[Seconds - start_register];
            datetime.second     = decode_time_value(value, SecondsMinutesMask);
        }
        if (Minutes >= start_register && Minutes <= end_register) {
            const uint8_t value = register_data[Minutes - start_register];
            datetime.minute     = decode_time_value(value, SecondsMinutesMask);
        }
        if (Hours >= start_register && Hours <= end_register) {
            const uint8_t value = register_data[Hours - start_register];
            datetime.hour       = decode_time_value(value, HoursValueMask);
        }
        if (DayOfTheWeek >= start_register && DayOfTheWeek <= end_register) {
            const uint8_t value       = register_data[DayOfTheWeek - start_register];
            uint8_t       day_of_week = decode_time_value(value, WeekDayMask);
            // Day of week is unused by software, some drivers use it as scratchpad
            // Allow any value 0-7, wrap around to 0 at 7
            if (day_of_week == WeekDayMask) {
                day_of_week = 0U;
            }
            datetime.weekDay = day_of_week;
        }
        if (DayOfTheMonth >= start_register && DayOfTheMonth <= end_register) {
            const uint8_t value = register_data[DayOfTheMonth - start_register];
            datetime.day        = decode_time_value(value, DayOfMonthMask);
        }
        if (Month >= start_register && Month <= end_register) {
            const uint8_t value = register_data[Month - start_register];
            datetime.month      = decode_time_value(value, MonthMask);
        }
        if (Year >= start_register && Year <= end_register) {
            const uint8_t value        = register_data[Year - start_register];
            const auto    century_base = static_cast<uint16_t>((datetime.year / YearWrap)
                                                            * YearWrap);
            datetime.year = static_cast<uint16_t>(century_base + bcd_to_bin(value));
        }

        sys::rtc::Driver::set_datetime(datetime);
    }

    if (is_virtual) {
        // Non-time registers are only writable through the virtual port (BMC)
        if (Control >= start_register && Control <= end_register) {
            const uint8_t value = register_data[Control - start_register];
            _control_register   = static_cast<uint8_t>((value & ControlWritableMask)
                                                     | Hour24Mode);
        }
    }
}

void I2cRtc::i2c_read(uint8_t& return_data, bool new_transaction)
{
    if (!_new_command && new_transaction) {
        _current_register = Begin;
    }
    _new_command = false;

    i2c_read_byte(return_data, _current_register);
    _current_register = static_cast<uint8_t>(_current_register + 1U);
}

void I2cRtc::i2c_read_byte(uint8_t& return_data, uint8_t register_address)
{
    switch (register_address) {
        case Seconds      : return_data = bin_to_bcd(get_seconds()); break;
        case Minutes      : return_data = bin_to_bcd(get_minutes()); break;
        case Hours        : return_data = bin_to_bcd(get_hours()); break;
        case DayOfTheWeek : return_data = bin_to_bcd(get_day_of_week()); break;
        case DayOfTheMonth: return_data = bin_to_bcd(get_day_of_month()); break;
        case Month        : return_data = bin_to_bcd(get_month()); break;
        case Year         : return_data = bin_to_bcd(get_year()); break;
        case Control      : return_data = _control_register; break;
        case Status       : return_data = get_status_register(); break;
        case PartInfo     : return_data = PartInfoValue; break;
        default           : return_data = 0x00U; break; /* Unmapped register. */
    }
}

void I2cRtc::i2c_read(std::span<uint8_t> i2c_data, size_t& i2c_size, bool new_transaction)
{
    if (!_new_command && new_transaction) {
        _current_register = Begin;
    }
    _new_command = false;

    i2c_read_block(i2c_data, i2c_size, _current_register);
    _current_register = static_cast<uint8_t>(_current_register + i2c_size);
}

void I2cRtc::i2c_read_virtual(std::span<uint8_t> i2c_data,
                              size_t&            i2c_size,
                              bool               new_transaction)
{
    if (!_new_command_virtual && new_transaction) {
        _current_register_virtual = Begin;
    }
    _new_command_virtual = false;

    i2c_read_block(i2c_data, i2c_size, _current_register_virtual);
    _current_register_virtual = static_cast<uint8_t>(_current_register_virtual + i2c_size);
}

void I2cRtc::i2c_read_block(std::span<uint8_t> i2c_data,
                            size_t&            i2c_size,
                            uint8_t            register_address)
{
    i2c_size = 0U;

    if (i2c_data.empty()) {
        return;
    }

    if (register_address != Begin) {
        i2c_read_byte(i2c_data[0], register_address);
        i2c_size = 1U;
        return;
    }

    sys::rtc::datetime_t datetime{};
    sys::rtc::Driver::get_datetime(datetime);

    if (i2c_data.size() > Seconds) {
        i2c_data[Seconds] = bin_to_bcd(datetime.second);
        i2c_size++;
    }
    if (i2c_data.size() > Seconds + 1U) {
        i2c_data[Seconds + 1U] = 0x00U;
        i2c_size++;
    }
    if (i2c_data.size() > Minutes) {
        i2c_data[Minutes] = bin_to_bcd(datetime.minute);
        i2c_size++;
    }
    if (i2c_data.size() > Minutes + 1U) {
        i2c_data[Minutes + 1U] = 0x00U;
        i2c_size++;
    }
    if (i2c_data.size() > Hours) {
        i2c_data[Hours] = bin_to_bcd(datetime.hour);
        i2c_size++;
    }
    if (i2c_data.size() > Hours + 1U) {
        i2c_data[Hours + 1U] = 0x00U;
        i2c_size++;
    }
    if (i2c_data.size() > DayOfTheWeek) {
        i2c_data[DayOfTheWeek] = bin_to_bcd(
            static_cast<uint8_t>(datetime.weekDay & WeekDayMask));
        i2c_size++;
    }
    if (i2c_data.size() > DayOfTheMonth) {
        i2c_data[DayOfTheMonth] = bin_to_bcd(datetime.day);
        i2c_size++;
    }
    if (i2c_data.size() > Month) {
        i2c_data[Month] = bin_to_bcd(datetime.month);
        i2c_size++;
    }
    if (i2c_data.size() > Year) {
        i2c_data[Year] = bin_to_bcd(static_cast<uint8_t>(datetime.year % YearWrap));
        i2c_size++;
    }
    if (i2c_data.size() > Control) {
        i2c_data[Control] = _control_register;
        i2c_size++;
    }
    if (i2c_data.size() > Status) {
        i2c_data[Status] = get_status_register();
        i2c_size++;
    }
}

uint8_t I2cRtc::get_seconds()
{
    sys::rtc::datetime_t datetime{};
    sys::rtc::Driver::get_datetime(datetime);
    return datetime.second;
}

uint8_t I2cRtc::get_minutes()
{
    sys::rtc::datetime_t datetime{};
    sys::rtc::Driver::get_datetime(datetime);
    return datetime.minute;
}

uint8_t I2cRtc::get_hours()
{
    sys::rtc::datetime_t datetime{};
    sys::rtc::Driver::get_datetime(datetime);
    return datetime.hour;
}

uint8_t I2cRtc::get_day_of_week()
{
    sys::rtc::datetime_t datetime{};
    sys::rtc::Driver::get_datetime(datetime);
    return static_cast<uint8_t>(datetime.weekDay & WeekDayMask);
}

uint8_t I2cRtc::get_day_of_month()
{
    sys::rtc::datetime_t datetime{};
    sys::rtc::Driver::get_datetime(datetime);
    return datetime.day;
}

uint8_t I2cRtc::get_month()
{
    sys::rtc::datetime_t datetime{};
    sys::rtc::Driver::get_datetime(datetime);
    return datetime.month;
}

uint8_t I2cRtc::get_year()
{
    sys::rtc::datetime_t datetime{};
    sys::rtc::Driver::get_datetime(datetime);
    return static_cast<uint8_t>(datetime.year % YearWrap);
}

uint8_t I2cRtc::get_battery_voltage()
{
    constexpr auto vbat_sensor = nv::ipc::voltage_monitor_config::vbat_get_sensor_config();
    if constexpr (vbat_sensor.sensor == nv::volt_mon::Sensor::Vbat) {
        uint32_t millivolts = 0U;
        if (nv::vbat::Vbat::inst().get_millivolts(millivolts) == nv::volt_mon::Status::Ok) {
            uint32_t level = 0U;
            if (millivolts > BatteryVoltageOffsetMv) {
                level = (millivolts - BatteryVoltageOffsetMv) / BatteryVoltageStepMv;
            }
            return static_cast<uint8_t>(std::min<uint32_t>(level, BatteryVoltageMask));
        }
    }
    return 0U;
}

}  // namespace nv::emulation

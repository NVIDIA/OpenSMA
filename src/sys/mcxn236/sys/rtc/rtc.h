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

#include <cstdint>

namespace sys::rtc {

/**
 * @brief Crystal tuning profile. Selects the per-crystal oscillator load caps + amplifier gain.
 */
enum class CrystalTuningProfile
{
    NoBatteryInternal16kFro,  // <- Will drift several mins/day.
    LFXTAL009678NoCaps,
    XTAL9H03270062WithCaps,
};

enum class Clockout
{
    Disabled,
    Hz32768,
    Hz1,
};

typedef struct datetime
{
    uint16_t year;    /*!< Range from 1984 to 2239.*/
    uint8_t  month;   /*!< Range from 1 to 12.*/
    uint8_t  day;     /*!< Range from 1 to 31 (depending on month).*/
    uint8_t  weekDay; /*!< Range from 0(Sunday) to 6(Saturday). */
    uint8_t  hour;    /*!< Range from 0 to 23.*/
    uint8_t  minute;  /*!< Range from 0 to 59.*/
    uint8_t  second;  /*!< Range from 0 to 59.*/
} datetime_t;

/**
 * @brief Driver for retreiving the battery domain RTC timer date and time. Follows POSIX/UNIX
 * time encoding in decimal and converts the underlying peripheral to this standard.
 */
class Driver
{
public:
    /**
     * @brief Initialize the IRTC using the project-configured crystal tuning profile. Call once
     * at boot, after BOARD_InitBootClocks() and before any get_datetime()/set_datetime().
     */
    static void init();

    /** @brief Configure the RTC clock output pin for SDK-supported frequencies. */
    static void set_clockout(Clockout clockout);

    static void get_datetime(datetime_t& datetime);

    static void set_datetime(const datetime_t& datetime);

    /**
     * @brief Program the alarm match time. Only hour, minute and second are matched;
     * the date fields are supplied internally to satisfy the peripheral.
     */
    static void set_alarm(const datetime_t& datetime);

    /** @brief Read back the programmed alarm match time. */
    static void get_alarm(datetime_t& datetime);

    /** @return true if the alarm has matched since its flag was last cleared. */
    static bool is_alarm_triggered();

    /** @brief Clear the latched alarm-match flag. */
    static void clear_alarm();

    /** @return true if a VBAT-domain power-on reset was detected. */
    static bool por_detected();

    /** @brief Clear the latched VBAT-domain power-on reset flag. */
    static void clear_por();

    /** @return true if the VBAT clock monitor detected a clock failure. */
    static bool clock_loss_detected();

    Driver() = delete;

private:
    /** @brief Use the inaccurate internal 16 kHz FRO; call init() if the board has a crystal.
     */
    static void init_no_crystal();
};

}  // namespace sys::rtc

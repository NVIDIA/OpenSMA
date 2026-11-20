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
#include "sys/rtc/rtc.h"

#include "fsl_clock.h"
#include "fsl_irtc.h"
#include "fsl_vbat.h"
#include "nv/logger/log.h"
#include "nv/nv.h"

#include NV_IPC_CONFIG_H

using namespace sys::rtc;

namespace {
constexpr bool kDebugLogging = false;
}  // namespace

void Driver::init()
{
    CLOCK_SetupClk16KClocking(kCLOCK_Clk16KToVbat | kCLOCK_Clk16KToMain);

    osc_32k_config_t osc_32k_config;
    CLOCK_GetDefaultOsc32KConfig(&osc_32k_config);
    osc_32k_config.initTrim = kVBAT_OscInitTrim500ms;
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    osc_32k_config.id = static_cast<osc32k_clk_gate_id_t>(kCLOCK_Osc32kToVbat
                                                          | kCLOCK_Osc32kToMain);

    int8_t  comp_integral = 0;
    uint8_t comp_fraction = 0U;

    // External crystal and battery.
    switch (sys::rtc::RtcCrystalTuningProfile) {
        case CrystalTuningProfile::LFXTAL009678NoCaps:
            /* IQD LFXTAL009678 : 32.768 kHz, CL 12.5 pF, ESR 70 kOhm. Measured 32766 Hz (~61
             * ppm slow).*/
            osc_32k_config.capEnable = true;
            osc_32k_config.xtalCap   = kVBAT_OscXtal8pFCap;
            osc_32k_config.extalCap  = kVBAT_OscExtal8pFCap;
            osc_32k_config.ampGain   = kVBAT_OscCoarseAdjustment18;
            comp_integral            = -2;
            comp_fraction            = 0U;
            break;
        case CrystalTuningProfile::XTAL9H03270062WithCaps:
            /* 9H03270062: 32.768 kHz, CL 12.5 pF, ESR 65 kOhm. The board supplies
             * 22 pF on each crystal pin, so do not add internal load capacitance. */
            osc_32k_config.capEnable = false;
            osc_32k_config.xtalCap   = kVBAT_OscXtal0pFCap;
            osc_32k_config.extalCap  = kVBAT_OscExtal0pFCap;
            osc_32k_config.ampGain   = kVBAT_OscCoarseAdjustment18;
            comp_integral            = 0;
            comp_fraction            = 0U;
            break;
        case CrystalTuningProfile::NoBatteryInternal16kFro:
        default                                           : init_no_crystal(); return;
    }

    CLOCK_SetupOsc32KClockingConfig(osc_32k_config);

    CLOCK_EnableClock(kCLOCK_Rtc0);
    RTC0->CTRL |= RTC_CTRL_CLK_SEL_MASK;

    const irtc_config_t rtc_config = {
        .alrmMatch          = kRTC_MatchSecMinHr,
        .clockSelect        = kIRTC_Clk32K,
        .disableClockOutput = true,
    };
    IRTC_Init(RTC0, &rtc_config);
    IRTC_ConfigClockOut(RTC0, kIRTC_ClkoutNo);

    // Overwrite compensation retained in the VBAT domain when tuning profiles change.
    (void)IRTC_SetWriteProtection(RTC0, false);
    IRTC_SetFineCompensation(RTC0, static_cast<uint8_t>(comp_integral), comp_fraction, false);
    (void)IRTC_SetWriteProtection(RTC0, true);

    const uint32_t vbat_flags = VBAT_GetStatusFlags(VBAT0);
    if (por_detected()) {
        nv::logger::error_no_wait(nv::logger::Event::RtcPorDetected,
                                  nv::logger::data_from_u32(vbat_flags),
                                  nv::logger::OutputDirection::Flash);
        clear_por();
    }
    if (clock_loss_detected()) {
        nv::logger::error_no_wait(nv::logger::Event::RtcClockLossDetected,
                                  nv::logger::data_from_u32(vbat_flags),
                                  nv::logger::OutputDirection::Flash);
    }

    if constexpr (kDebugLogging) {
        // Which rail the IRTC actually counts: CLK_SEL=1 -> 32.768 kHz crystal
        // (OSCCLKE), CLK_SEL=0 -> 16.384 kHz FRO16K (FROCLKE). A still-enabled
        // FRO16K (FRO_EN/FROCLKE) is leftover from the 16K build.
        const uint32_t rtc_ctrl = RTC0->CTRL;
        nv::info("RTC src: CLK_SEL=%d (%s) OSCCLKE=0x%x FROCLKE=0x%x FRO_EN=%d\n",
                 (rtc_ctrl & RTC_CTRL_CLK_SEL_MASK) ? 1 : 0,
                 (rtc_ctrl & RTC_CTRL_CLK_SEL_MASK) ? "32k XTAL" : "16k FRO",
                 static_cast<unsigned>(VBAT0->OSCCLKE),
                 static_cast<unsigned>(VBAT0->FROCLKE),
                 (VBAT0->FROCTLA & VBAT_FROCTLA_FRO_EN_MASK) ? 1 : 0);

        // Crystal oscillator health + config integrity (OSCCTLB omitted: it is just
        // the bitwise inverse of OSCCTLA).
        nv::info(
            "VBAT osc: STATUSA=0x%x OSC_RDY=%d POR_DET=%d CONFIG_DET=%d "
            "OSCCTLA=0x%x OSCLCKA=0x%x locked=%d\n",
            static_cast<unsigned>(vbat_flags),
            (vbat_flags & kVBAT_StatusFlagOsc32kReady) ? 1 : 0,
            (vbat_flags & kVBAT_StatusFlagPORDetect) ? 1 : 0,
            (vbat_flags & kVBAT_StatusFlagConfigDetect) ? 1 : 0,
            static_cast<unsigned>(VBAT0->OSCCTLA),
            static_cast<unsigned>(VBAT0->OSCLCKA),
            VBAT_CheckOsc32kSettingsLocked(VBAT0) ? 1 : 0);
    }
}

void Driver::init_no_crystal()
{
    // No crystal and vbat is tied to VCC.
    CLOCK_SetupClk16KClocking(kCLOCK_Clk16KToVbat | kCLOCK_Clk16KToMain);

    const irtc_config_t rtc_config = {
        .alrmMatch          = kRTC_MatchSecMinHr,
        .clockSelect        = kIRTC_Clk16K,
        .disableClockOutput = true,
    };
    IRTC_Init(RTC0, &rtc_config);
    IRTC_ConfigClockOut(RTC0, kIRTC_ClkoutNo);
}

void Driver::set_clockout(Clockout clockout)
{
    switch (clockout) {
        case Clockout::Disabled:
            IRTC_EnableClockOutputToPeripheral(RTC0, false);
            IRTC_ConfigClockOut(RTC0, kIRTC_ClkoutNo);
            break;
        case Clockout::Hz32768:
            IRTC_ConfigClockOut(RTC0, kIRTC_Clkout32kHz);
            IRTC_EnableClockOutputToPeripheral(RTC0, true);
            break;
        case Clockout::Hz1:
            IRTC_ConfigClockOut(RTC0, kIRTC_ClkoutFine1Hz);
            IRTC_EnableClockOutputToPeripheral(RTC0, true);
            break;
    }
}

bool Driver::por_detected()
{
    return (VBAT_GetStatusFlags(VBAT0) & kVBAT_StatusFlagPORDetect) != 0U;
}

void Driver::clear_por()
{
    VBAT_ClearStatusFlags(VBAT0, kVBAT_StatusFlagPORDetect);
}

bool Driver::clock_loss_detected()
{
    return (VBAT_GetStatusFlags(VBAT0) & kVBAT_StatusFlagClockDetect) != 0U;
}

void Driver::get_datetime(datetime_t& datetime)
{
    irtc_datetime_t nxp_datetime{};

    IRTC_GetDatetime(RTC0, &nxp_datetime);
    if ((IRTC_GetStatusFlags(RTC0) & kIRTC_BusErrFlag) != 0U) {
        // IRTC sets STATUS[INVAL_BIT] when datetime registers are invalid
        // IRTC_GetDatetime waits for STATUS[INVAL_BIT] to be cleared, but there is still a
        // small window before/during datetime register read where it can be asserted If that
        // happens, IRTC sets STATUS[BUS_ERR] - clear the bit and retry IRTC[INVAL_BIT] is
        // asserted every second, so one retry is enough
        (void)IRTC_SetWriteProtection(RTC0, false);
        IRTC_ClearStatusFlags(RTC0, kIRTC_BusErrFlag);
        (void)IRTC_SetWriteProtection(RTC0, true);
        IRTC_GetDatetime(RTC0, &nxp_datetime);
    }

    // NXP already follows POSIX time
    datetime.year    = nxp_datetime.year;
    datetime.month   = nxp_datetime.month;
    datetime.day     = nxp_datetime.day;
    datetime.weekDay = nxp_datetime.weekDay;
    datetime.hour    = nxp_datetime.hour;
    datetime.minute  = nxp_datetime.minute;
    datetime.second  = nxp_datetime.second;
}

void Driver::set_datetime(const datetime_t& datetime)
{
    // NXP already follows POSIX time
    const irtc_datetime_t nxp_datetime = {
        .year    = datetime.year,
        .month   = datetime.month,
        .day     = datetime.day,
        .weekDay = datetime.weekDay,
        .hour    = datetime.hour,
        .minute  = datetime.minute,
        .second  = datetime.second,
    };

    // IRTC calendar registers are write-protected; the SDK auto-relocks 2s
    // after the unlock sequence, but lock explicitly so writes after this
    // function returns cannot race the auto-lock window.
    (void)IRTC_SetWriteProtection(RTC0, false);
    (void)IRTC_SetDatetime(RTC0, &nxp_datetime);
    (void)IRTC_SetWriteProtection(RTC0, true);
}

void Driver::set_alarm(const datetime_t& datetime)
{
    // The IRTC was initialized with kRTC_MatchSecMinHr, so only hour/minute/second
    // participate in the match. IRTC_SetAlarm still range-checks the whole struct,
    // so borrow valid date fields from the running calendar.
    irtc_datetime_t now{};
    IRTC_GetDatetime(RTC0, &now);

    const irtc_datetime_t alarm = {
        .year    = now.year,
        .month   = now.month,
        .day     = now.day,
        .weekDay = now.weekDay,
        .hour    = datetime.hour,
        .minute  = datetime.minute,
        .second  = datetime.second,
    };

    (void)IRTC_SetWriteProtection(RTC0, false);
    (void)IRTC_SetAlarm(RTC0, &alarm);
    (void)IRTC_SetWriteProtection(RTC0, true);
}

void Driver::get_alarm(datetime_t& datetime)
{
    irtc_datetime_t alarm{};
    IRTC_GetAlarm(RTC0, &alarm);
    datetime.year    = alarm.year;
    datetime.month   = alarm.month;
    datetime.day     = alarm.day;
    datetime.weekDay = alarm.weekDay;
    datetime.hour    = alarm.hour;
    datetime.minute  = alarm.minute;
    datetime.second  = alarm.second;
}

bool Driver::is_alarm_triggered()
{
    return (IRTC_GetStatusFlags(RTC0) & kIRTC_AlarmFlag) != 0U;
}

void Driver::clear_alarm()
{
    // Status flags are write-1-to-clear and not write-protected.
    IRTC_ClearStatusFlags(RTC0, kIRTC_AlarmFlag);
}

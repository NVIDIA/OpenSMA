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

#include <cstdint>

namespace nv::mctp {

// Rack Power Smoothing Parameter IDs
enum class RackPwrSmoothParams : uint8_t
{
    EdppSoCTargetPrimary           = 0,
    EdppSocCriticalLow             = 1,
    EdppPrimaryKp                  = 2,
    EdppPrimaryKi                  = 3,
    EdppPrimaryKd                  = 4,
    EdppLowSocResidencyThreshold   = 5,
    EdppResidencyTargetSecondary   = 6,
    EdppSecondaryKp                = 7,
    EdppSecondaryKi                = 8,
    EdppSecondaryKd                = 9,
    IsinkSoCTargetPrimary          = 10,
    IsinkSoCCriticalHigh           = 11,
    IsinkPrimaryKp                 = 12,
    IsinkPrimaryKi                 = 13,
    IsinkPrimaryKd                 = 14,
    IsinkHighSoCResidencyThreshold = 15,
    IskinkResidencyTargetSecondary = 16,
    IsinkSecondaryKp               = 17,
    IsinkSecondaryKi               = 18,
    IsinkSecondaryKd               = 19,
    EdppResidencyEwmaTau           = 20,  // float32 seconds, wire as uint32_t bits
    IsinkResidencyEwmaTau          = 21,  // float32 seconds, wire as uint32_t bits
    EdppIntegralMin                = 22,
    EdppIntegralMax                = 23,
    EdppOutputMax                  = 24,
    IsinkIntegralMin               = 25,
    IsinkIntegralMax               = 26,
    IsinkOutputMax                 = 27,
    EdppPrimaryPidDtDivisor        = 28,  // uint32 1-1000; PID integral uses dt / value
    EdppSecondaryPidDtDivisor      = 29,
    IsinkPrimaryPidDtDivisor       = 30,
    IsinkSecondaryPidDtDivisor     = 31,
    MaxTuningParams                = 32,

    // RESERVED: Values 32-39 are reserved for future tuning parameters
    // This gap allows adding more regular params without breaking backward compatibility
    // for TestHook parameters

    // TestHook-only parameters (debug token protected) - START AT 40
    TestHookParamsStart      = 40,
    OverrideSoCInput         = 40,
    OverrideEdppOffsetOutput = 41,  // Percent override (0–100.00%); see OverrideParam.value /
                                    // 100
    OverrideIsinkOffsetOutput = 42,
    OverrideEdppDacRaw        = 43,  // Raw 12-bit DAC; if enabled wins vs param 41
    OverrideIsinkDacRaw       = 44,  // Raw 12-bit DAC; if enabled wins vs param 42
    MaxParamCount             = 45,
};

// Rack Power Smoothing Telemetry Types
enum class RackPwrSmoothTelemType : uint16_t
{
    SocTelemetryRunningSum = 0,
    PwrBrakeRunningSumMs   = 1,
    EdppOffsetRunningSum   = 2,
    ISinkOffsetRunningSum  = 3,
    SocRecentHistory       = 4,
    EdppRecentHistory      = 5,
    IsinkRecentHistory     = 6
};

// GetRackPowerSmoothingParam Response
struct [[gnu::packed]] NsmTFFGetRackPwrSmoothParamRes
{
    uint8_t  numParams;
    uint8_t  currentPresetId;
    uint16_t resvd;
    uint32_t paramArray[];
};

// SetRackPowerSmoothingParam Request (single-parameter format)
struct [[gnu::packed]] NsmTFFSetRackPwrSmoothParamReq
{
    uint8_t presetId;  // Which preset to modify (1-3, 0 is read-only)
    uint8_t paramId;   // Which parameter to modify (tuning 0-31; TestHook 40-44 per
                       // RackPwrSmoothParams)
    uint16_t resvd;
    uint32_t paramValue;  // SFXP22_10 encoded value or OverrideParam format
};

// GetDebugTelemetry Request
struct [[gnu::packed]] NsmTFFGetRackPwrSmoothTelemetryReq
{
    uint16_t telemetryType;
    uint16_t resvd;
};

// GetDebugTelemetry Response
struct [[gnu::packed]] NsmTFFGetRackPwrSmoothTelemetryRes
{
    uint16_t dataSizeBytes;  // Size of telemetryData[] in bytes
    uint16_t resvd;
    uint8_t  telemetryData[];
};

// ============================================================================
// ADC Calibration Test Mode Structures
// ============================================================================

// ADC Calibration Point (4 bytes)
// Stores expected DAC code and actual ADC readback for one voltage point
struct [[gnu::packed]] AdcCalibrationPoint
{
    uint16_t expected_dac_code;  // DAC code sent to external 16-bit DAC (0-65535)
    uint16_t actual_adc_code;    // Raw ADC code read back from MCU ADC (0-65535)
};

// Number of calibration points (26 points, 0V to 2.5V in 100mV increments)
constexpr uint8_t ADC_CALIBRATION_NUM_POINTS = 26;

// GetAdcCalibrationResults (0xA6) Response
struct [[gnu::packed]] NsmTFFGetAdcCalibResultsRes
{
    uint8_t             testComplete;    // 0=never run, 1=complete
    uint8_t             testInProgress;  // 0=idle, 1=currently running
    uint8_t             numPoints;       // Number of calibration points (always 26)
    uint8_t             resvd;
    AdcCalibrationPoint points[ADC_CALIBRATION_NUM_POINTS];  // 104 bytes (26 * 4)
};
// Total response size: 4 + 104 = 108 bytes

// AdcCalibSetLoopbackDacCode (0xA7) Request
struct [[gnu::packed]] NsmTFFAdcCalibSetLoopbackDacCodeReq
{
    uint16_t dac_code;  // Raw code to external loopback DAC (I2C), 0-65535
    uint16_t resvd;     // Set to 0
};

// GetPowerSmoothRawReadback (0xA8) — selector for which last raw code to return
enum class PwrSmoothRawReadbackId : uint8_t
{
    SocAdcRaw   = 0,  // Last SoC ADC sample (same source as calibration readback)
    EdppDacRaw  = 1,  // Last 12-bit code written to EDPP offset DAC (firmware shadow)
    IsinkDacRaw = 2,  // Last 12-bit code written to ISINK offset DAC (firmware shadow)
};

// GetPowerSmoothRawReadback (0xA8) Request
struct [[gnu::packed]] NsmTFFGetPowerSmoothRawReadbackReq
{
    uint8_t  readback_id;  // PwrSmoothRawReadbackId; 0–2 defined, rest reserved
    uint8_t  resvd_u8;     // Set to 0
    uint16_t resvd_u16;    // Set to 0
};

// GetPowerSmoothRawReadback (0xA8) Response
struct [[gnu::packed]] NsmTFFGetPowerSmoothRawReadbackRes
{
    uint16_t raw_code;  // SoC ADC raw or DAC raw (12-bit DAC codes use low bits only)
    uint16_t resvd;     // Set to 0
};

}  // namespace nv::mctp

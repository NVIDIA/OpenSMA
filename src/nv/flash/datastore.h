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

namespace nv::flash {

using Data = uint32_t;

enum class Key : uint32_t
{
    NpdsStart      = 0x0,
    NpdsBootReason = NpdsStart,
    NpdsFmcStatus,
    NpdsFmcAuthResult,
    NpdsExtTimestampLSB,
    NpdsExtTimestampMSB,
    NpdsExtTimestampTick,
    NpdsAllowInitBackgroundCopy,
    NpdsBackgroundCopyComponentId,  ///< Latest background-copy component ID for progress query
    NpdsBootTimeFromFmcEndToMctpReady,
    NpdsBootReasonOriginal,
    NpdsI2cDynamicAddrForOcpDevice0,
    NpdsI2cDynamicAddrForOcpDevice1,
    NpdsApplicationFaultMagic,
    NpdsCfsr,
    NpdsHfsr,
    NpdsWdtEventBits,
    NpdsDebugTokenAuthenticated,     ///< Debug token auth status: 0=not auth,
                                     ///< non-zero=token_type_bitmap
    NpdsDebugTokenSubtypeDebugFw,    ///< Subtype bitmap for FlashDebugFw token
    NpdsDebugTokenSubtypeMcuDebug,   ///< Subtype bitmap for MCU debug token
    NpdsDebugTokenSubtypeCpldDebug,  ///< Subtype bitmap for CPLD debug token
    NpdsDebugTokenFailCount,         ///< Debug token failure count for log throttling
    NpdsAp0FwStatus,                 ///< AP0 FW status
    NpdsAp1FwStatus,                 ///< AP1 FW status
    NpdsFmcNumericVersion,           ///< FMC numeric version (e.g. 3 for V3, 4 for V4)
    NpdsAp0ProvisionStatus,          ///< AP0 provision status
    NpdsEnd,
    NpdsInvalid,
    // here is for active ap fw authenticate data
    NpdsActiveApFwAuthenticateData,
    // here is for update ap fw authenticate data
    NpdsUpdateApFwAuthenticateData,
    PdsStart = 0x1000,
    // TBD: Replace it with real data field
    PdsBootableSlot0 = PdsStart,
    PdsBootableSlot1,
    PdsUpdateState,
    PdsUpdateSlot,
    PdscfpaCustomerLastUpdated,
    PdsL3CertificateProgramed,
    PdsDbgTokenNonceValid,      ///< indicates nonce in PDS is valid
    PdsDbgTokenNonce1,          ///< first chunk of 16 byte nonce
    PdsDbgTokenNonce2,          ///< second chunk of 16 byte nonce
    PdsDbgTokenNonce3,          ///< third chunk of 16 byte nonce
    PdsDbgTokenNonce4,          ///< last chunk of 16 byte nonce
    PdsBackgroundSetup,         ///< Background update setup 0: Default 1: enable, 2: disable
    PdsBackgroundSetupOneTime,  ///< Background update setup for one time boot only 0: not setup
                                ///< 2: disable 3: enable
    PdsOtpSimultaneousFuseAddress0,
    PdsOtpSimultaneousFuseAddress1,
    PdsOtpSimultaneousFuseAddress2,
    PdsOtpSimultaneousFuseAddress3,
    PdsOtpSimultaneousFuseAddress4,
    PdsOtpSimultaneousFuseAddress5,
    PdsOtpSimultaneousFuseAddress6,
    PdsOtpSimultaneousFuseAddress7,
    PdsOtpSimultaneousFuseAddress8,
    PdsOtpSimultaneousFuseAddress9,
    PdsOtpSimultaneousFuseAddress10,
    PdsOtpSimultaneousFuseAddress11,
    PdsOtpSimultaneousFuseAddress12,
    PdsOtpSimultaneousFuseAddress13,
    PdsOtpSimultaneousFuseAddress14,
    PdsOtpSimultaneousFuseAddress15,
    PdsOtpSimultaneousFuseAddress16,
    PdsOtpSimultaneousFuseAddress17,
    PdsOtpSimultaneousFuseAddress18,
    PdsOtpSimultaneousFuseAddress19,
    PdsOtpSimultaneousFuseAddress20,
    PdsOtpSimultaneousFuseAddress21,
    PdsOtpSimultaneousFuseAddress22,
    PdsOtpSimultaneousFuseAddress23,
    PdsOtpSimultaneousFuseAddress24,
    PdsOtpSimultaneousFuseAddress25,
    PdsOtpSimultaneousFuseAddress26,
    PdsOtpSimultaneousFuseAddress27,
    PdsOtpSimultaneousFuseAddress28,
    PdsOtpSimultaneousFuseAddress29,
    PdsOtpSimultaneousFuseAddress30,
    PdsOtpSimultaneousFuseAddress31,  // DDA ordinal number
    PdsOtpSimultaneousFuseAddress32,  // Efuse signature R
    PdsOtpSimultaneousFuseAddress33,  // Efuse signature R
    PdsOtpSimultaneousFuseAddress34,  // Efuse signature R
    PdsOtpSimultaneousFuseAddress35,  // Efuse signature R
    PdsOtpSimultaneousFuseAddress36,  // Efuse signature R
    PdsOtpSimultaneousFuseAddress37,  // Efuse signature R
    PdsOtpSimultaneousFuseAddress38,  // Efuse signature R
    PdsOtpSimultaneousFuseAddress39,  // Efuse signature R
    PdsOtpSimultaneousFuseAddress40,  // Efuse signature R
    PdsOtpSimultaneousFuseAddress41,  // Efuse signature R
    PdsOtpSimultaneousFuseAddress42,  // Efuse signature R
    PdsOtpSimultaneousFuseAddress43,  // Efuse signature R
    PdsOtpSimultaneousFuseAddress44,  // Efuse signature S
    PdsOtpSimultaneousFuseAddress45,  // Efuse signature S
    PdsOtpSimultaneousFuseAddress46,  // Efuse signature S
    PdsOtpSimultaneousFuseAddress47,  // Efuse signature S
    PdsOtpSimultaneousFuseAddress48,  // Efuse signature S
    PdsOtpSimultaneousFuseAddress49,  // Efuse signature S
    PdsOtpSimultaneousFuseAddress50,  // Efuse signature S
    PdsOtpSimultaneousFuseAddress51,
    PdsOtpSimultaneousFuseAddress52,
    PdsOtpSimultaneousFuseAddress53,
    PdsOtpSimultaneousFuseAddress54,
    PdsOtpSimultaneousFuseAddress55,
    PdsOtpSimultaneousFuseAddress56,
    PdsOtpSimultaneousFuseAddress57,
    PdsOtpSimultaneousFuseAddress58,
    PdsOtpSimultaneousFuseAddress59,
    PdsOtpSimultaneousFuseAddress60,
    PdsOtpSimultaneousFuseAddress61,
    PdsOtpSimultaneousFuseAddress62,
    PdsOtpSimultaneousFuseAddress63,  // Efuse signature S
    PdsOtpSimultaneousFuseAddress64,  // Efuse signature S
    PdsOtpSimultaneousFuseAddress65,  // Efuse signature S
    PdsOtpSimultaneousFuseAddress66,  // Efuse signature S
    PdsOtpSimultaneousFuseAddress67,  // Efuse signature S
    PdsOtpSimultaneousFuseAddress68,
    PdsOtpSimultaneousFuseAddress69,
    PdsOtpSimultaneousFuseAddress70,
    PdsOtpSimultaneousFuseAddress71,
    PdsOtpSimultaneousFuseAddress72,
    PdsOtpSimultaneousFuseAddress73,
    PdsOtpSimultaneousFuseAddress74,
    PdsOtpSimultaneousFuseAddress75,
    PdsOtpSimultaneousFuseAddress76,
    PdsOtpSimultaneousFuseAddress77,
    PdsOtpSimultaneousFuseAddress78,
    PdsOtpSimultaneousFuseAddress79,
    PdsOtpSimultaneousFuseAddress80,
    PdsOtpSimultaneousFuseAddress81,
    // SoC Power Smoothing persistent settings (set via NSM Type 5 SetDeviceModeSettingsV2)
    PdsSoCPowerSmoothEnabled,  ///< SoC Power Smoothing enabled: 0=disabled, 1=enabled
    PdsSoCPowerSmoothCurrentPresetIndex,  ///< Current preset index (0-3)
    PdsSoCPowerBrakeEnabled,              ///< Power brake enabled: 0=disabled, 1=enabled
    PdsMaxACPowerRampRate,  ///< Max AC power ramp rate (uint32_t, W/s; 0 = disabled)
    // ADC Calibration data (32 points: each uint32_t =
    // [expected_dac_code:16][actual_adc_code:16])
    // ADC Calibration data (26 points: 0V to 2.5V in 100mV steps)
    // Each uint32_t = [expected_dac_code:16][actual_adc_code:16]
    PdsAdcCalibrationComplete,  ///< Calibration test completed: 0=never run, 1=complete
    PdsAdcCalibrationData0,     ///< Calibration point 0  (0.0V)
    PdsAdcCalibrationData1,     ///< Calibration point 1  (0.1V)
    PdsAdcCalibrationData2,     ///< Calibration point 2  (0.2V)
    PdsAdcCalibrationData3,     ///< Calibration point 3  (0.3V)
    PdsAdcCalibrationData4,     ///< Calibration point 4  (0.4V)
    PdsAdcCalibrationData5,     ///< Calibration point 5  (0.5V)
    PdsAdcCalibrationData6,     ///< Calibration point 6  (0.6V)
    PdsAdcCalibrationData7,     ///< Calibration point 7  (0.7V)
    PdsAdcCalibrationData8,     ///< Calibration point 8  (0.8V)
    PdsAdcCalibrationData9,     ///< Calibration point 9  (0.9V)
    PdsAdcCalibrationData10,    ///< Calibration point 10 (1.0V)
    PdsAdcCalibrationData11,    ///< Calibration point 11 (1.1V)
    PdsAdcCalibrationData12,    ///< Calibration point 12 (1.2V)
    PdsAdcCalibrationData13,    ///< Calibration point 13 (1.3V)
    PdsAdcCalibrationData14,    ///< Calibration point 14 (1.4V)
    PdsAdcCalibrationData15,    ///< Calibration point 15 (1.5V)
    PdsAdcCalibrationData16,    ///< Calibration point 16 (1.6V)
    PdsAdcCalibrationData17,    ///< Calibration point 17 (1.7V)
    PdsAdcCalibrationData18,    ///< Calibration point 18 (1.8V)
    PdsAdcCalibrationData19,    ///< Calibration point 19 (1.9V)
    PdsAdcCalibrationData20,    ///< Calibration point 20 (2.0V)
    PdsAdcCalibrationData21,    ///< Calibration point 21 (2.1V)
    PdsAdcCalibrationData22,    ///< Calibration point 22 (2.2V)
    PdsAdcCalibrationData23,    ///< Calibration point 23 (2.3V)
    PdsAdcCalibrationData24,    ///< Calibration point 24 (2.4V)
    PdsAdcCalibrationData25,    ///< Calibration point 25 (2.5V)
    // Leak Detection thresholds (2 entries per slot, 4 slots)
    // Part0: [sensorId:16 | minLeak:16]
    // Part1: [maxLeak:16 | maxNormal:16]  (ADC counts, post-conversion)
    PdsLeakDetSlot0Part0,
    PdsLeakDetSlot0Part1,
    PdsLeakDetSlot1Part0,
    PdsLeakDetSlot1Part1,
    PdsLeakDetSlot2Part0,
    PdsLeakDetSlot2Part1,
    PdsLeakDetSlot3Part0,
    PdsLeakDetSlot3Part1,
    PdsNcsiMacAddrLow,
    PdsNcsiMacAddrHigh,
    PdsSoCThermBrakeEnabled,
    // Power smoothing voltage calibration (float32 as uint32_t; coeff_id 0-14)
    PdsPwrSmoothCalib0,   ///< SOC_ADC_Q_A
    PdsPwrSmoothCalib1,   ///< SOC_ADC_Q_B
    PdsPwrSmoothCalib2,   ///< SOC_ADC_Q_C
    PdsPwrSmoothCalib3,   ///< SOC_ADC_L_M
    PdsPwrSmoothCalib4,   ///< SOC_ADC_L_C
    PdsPwrSmoothCalib5,   ///< EDPP_DAC_Q_A
    PdsPwrSmoothCalib6,   ///< EDPP_DAC_Q_B
    PdsPwrSmoothCalib7,   ///< EDPP_DAC_Q_C
    PdsPwrSmoothCalib8,   ///< EDPP_DAC_L_M
    PdsPwrSmoothCalib9,   ///< EDPP_DAC_L_C
    PdsPwrSmoothCalib10,  ///< ISINK_DAC_Q_A
    PdsPwrSmoothCalib11,  ///< ISINK_DAC_Q_B
    PdsPwrSmoothCalib12,  ///< ISINK_DAC_Q_C
    PdsPwrSmoothCalib13,  ///< ISINK_DAC_L_M
    PdsPwrSmoothCalib14,  ///< ISINK_DAC_L_C
    PdsEnd,
    PdsInvalid,
};

struct [[gnu::packed]] NpdsEntry
{
    bool valid;
    Data data;
};

struct [[gnu::packed]] PdsEntry
{
    Data data;
};

constexpr uint32_t pds_index(Key key)
{
    return static_cast<uint32_t>(key) - static_cast<uint32_t>(Key::PdsStart);
};

constexpr uint32_t npds_index(Key key)
{
    return static_cast<uint32_t>(key) - static_cast<uint32_t>(Key::NpdsStart);
};

constexpr auto NpdsSize = npds_index(Key::NpdsEnd);

constexpr auto PdsSize = pds_index(Key::PdsEnd);

constexpr uint32_t PdsDigestSize = 32;  // TBD: Determine digest size after crypto lib ready
using NpdsDataArray              = std::array<NpdsEntry, NpdsSize>;
using PdsDataArray               = std::array<PdsEntry, PdsSize>;
using PdsDigest                  = std::array<uint8_t, PdsDigestSize>;

}  // namespace nv::flash

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
#include "nv/bootloader.h"
#include NV_IPC_CONFIG_H
#include "nv/common/preproc.h"
#include "nv/soc_pwr_smoothing/soc_pwr_smoothing.h"
#include "nv/soc_pwr_smoothing/power_manager.h"
#include "nv/soc_pwr_smoothing/presets.h"
#include "nv/ctimer/ctimer.h"
#include "sys/adc/adc.h"
#include "nv/common/system.h"
#include "nv/logger/log.h"
#include "nv/mctp/nsm_type_ff.h"
#include "nv/flash/flash.h"
#include "sys/lpcac/lpcac.h"
#include "sys/dac/dac.h"
#include "sys/i2c/utils.h"

#include <array>
#include <algorithm>
#include <bit>
#include <chrono>

#include <FreeRTOS.h>
#include <task.h>

using namespace std::chrono_literals;

namespace nv::soc_pwr_smoothing {

NV_SHARED_BSS PowerManager
    PowerSmoothing::power_manager{};  // NOLINT(*-non-const-global-variables)
NV_SHARED_BSS PresetManager
    PowerSmoothing::preset_manager{};  // NOLINT(*-non-const-global-variables)

namespace {

// ============================================================================
// Module-level state (not part of PowerSmoothing class)
// ============================================================================

// NOLINTBEGIN(*-avoid-non-const-global-variables)
constexpr bool SocPowerSmoothingDebugEnabled = false;
// NOLINTEND(*-avoid-non-const-global-variables)

// ============================================================================
// ADC Calibration Test Mode - I2C DAC Control
// ============================================================================

// AD5693 DAC constants
constexpr uint8_t Ad5693CmdWriteAndUpdate = 0x30;  // Write to and update DAC register
constexpr uint8_t ByteMaskLsb             = 0xFF;  // Mask for extracting LSB

/// @brief Write a 16-bit code to the external AD5693-compatible DAC
/// @param dac_code 16-bit DAC code (0-65535 maps to 0V-2.5V output)
/// @return true on success, false on I2C error
bool write_ad5693_dac(uint16_t dac_code)
{
    // AD5693 I2C protocol: [Command byte] [Data MSB] [Data LSB]
    // NOLINTNEXTLINE(misc-const-correctness) - i2c_write takes non-const span
    std::array<uint8_t, 3> buffer = {Ad5693CmdWriteAndUpdate,
                                     static_cast<uint8_t>(dac_code >> 8),
                                     static_cast<uint8_t>(dac_code & ByteMaskLsb)};
    auto status = sys::i2c::i2c_write(nv::soc_pwr_smoothing::AdcCalibDacI2cPort,
                                      nv::soc_pwr_smoothing::AdcCalibDacI2cAddr,
                                      buffer);
    return (status == nv::i2c::I2cStatus::Ok);
}

// ============================================================================
// ADC Calibration Test Mode - State Machine
// ============================================================================

// Calibration timing: 5ms per step
constexpr uint32_t ADC_CALIB_STEP_DELAY_MS = 5;

// DAC codes for 26 calibration voltage points (0V to 2.5V, 100mV increments)
// Formula: dac_code = (voltage / 2.5V) * 65535 = voltage * 26214
// External DAC: 16-bit, 0-2.5V full scale
// Signal path: DAC -> Op-amp (gain 4.322) -> Connector -> Divider (0.2492) -> ADC
constexpr std::array<uint16_t, nv::mctp::ADC_CALIBRATION_NUM_POINTS> CalibrationDacCodes = {
    0,      // Point 0:  0.0V
    2621,   // Point 1:  0.1V
    5243,   // Point 2:  0.2V
    7864,   // Point 3:  0.3V
    10486,  // Point 4:  0.4V
    13107,  // Point 5:  0.5V
    15729,  // Point 6:  0.6V
    18350,  // Point 7:  0.7V
    20972,  // Point 8:  0.8V
    23593,  // Point 9:  0.9V
    26214,  // Point 10: 1.0V
    28836,  // Point 11: 1.1V
    31457,  // Point 12: 1.2V
    34079,  // Point 13: 1.3V
    36700,  // Point 14: 1.4V
    39321,  // Point 15: 1.5V
    41943,  // Point 16: 1.6V
    44564,  // Point 17: 1.7V
    47186,  // Point 18: 1.8V
    49807,  // Point 19: 1.9V
    52428,  // Point 20: 2.0V
    55050,  // Point 21: 2.1V
    57671,  // Point 22: 2.2V
    60293,  // Point 23: 2.3V
    62914,  // Point 24: 2.4V
    65535,  // Point 25: 2.5V (full scale)
};

// ADC calibration points are handled as local buffers only.
using AdcCalibrationPoints = std::array<nv::mctp::AdcCalibrationPoint,
                                        nv::mctp::ADC_CALIBRATION_NUM_POINTS>;

// Forward declaration for flash save (implemented later)
void save_calibration_to_flash(const AdcCalibrationPoints& points);
bool load_calibration_from_flash(AdcCalibrationPoints& points);

/// @brief Save calibration results to flash (PDS)
void save_calibration_to_flash(const AdcCalibrationPoints& points)
{
    for (uint8_t i = 0; i < nv::mctp::ADC_CALIBRATION_NUM_POINTS; i++) {
        const auto&    pt     = points.at(i);
        const uint32_t packed = (static_cast<uint32_t>(pt.expected_dac_code) << 16)
                              | static_cast<uint32_t>(pt.actual_adc_code);
        const auto key = static_cast<nv::flash::Key>(
            static_cast<uint32_t>(nv::flash::Key::PdsAdcCalibrationData0) + i);
        (void)nv::flash::Flash::set_data(key, packed);
    }

    // Commit completion marker last so power loss cannot advertise partial data as complete.
    (void)nv::flash::Flash::set_data(nv::flash::Key::PdsAdcCalibrationComplete, 1U);
}

bool load_calibration_from_flash(AdcCalibrationPoints& points)
{
    points.fill(nv::mctp::AdcCalibrationPoint{0U, 0U});

    uint32_t flash_complete = 0;
    if (nv::flash::Flash::get_data(nv::flash::Key::PdsAdcCalibrationComplete, flash_complete)
        != nv::flash::Status::Ok) {
        flash_complete = 0;
    }

    constexpr uint16_t kWordMaskLsb = 0xFFFF;
    for (uint8_t i = 0; i < nv::mctp::ADC_CALIBRATION_NUM_POINTS; i++) {
        const auto key = static_cast<nv::flash::Key>(
            static_cast<uint32_t>(nv::flash::Key::PdsAdcCalibrationData0) + i);
        uint32_t packed = 0;
        if (nv::flash::Flash::get_data(key, packed) == nv::flash::Status::Ok) {
            points.at(i).expected_dac_code = static_cast<uint16_t>(packed >> 16);
            points.at(i).actual_adc_code   = static_cast<uint16_t>(packed & kWordMaskLsb);
        }
    }
    return flash_complete == 1U;
}

}  // anonymous namespace

// ============================================================================
// ADC Calibration Test Mode - Public API
// ============================================================================

void execute_adc_calibration()
{
    AdcCalibrationPoints points{};
    (void)nv::flash::Flash::set_data(nv::flash::Key::PdsAdcCalibrationComplete, 0U);

    for (auto& pt : points) {
        pt.expected_dac_code = 0;
        pt.actual_adc_code   = 0;
    }

    // Run all 26 calibration steps (blocking, ~130ms: 26 steps * 5ms). Per step: set DAC,
    // wait 5ms for settling, read ADC.

    for (uint8_t idx = 0; idx < nv::mctp::ADC_CALIBRATION_NUM_POINTS; idx++) {
        const uint16_t dac_code          = CalibrationDacCodes.at(idx);
        points.at(idx).expected_dac_code = dac_code;
        if (!write_ad5693_dac(dac_code)) {
            // Abort calibration if DAC I2C write fails to avoid persisting invalid points.
            nv::logger::error(nv::logger::Event::SocPwrSmoothingAdcCalibDacWriteFail,
                              nv::logger::data_from_two_u32(static_cast<uint32_t>(idx),
                                                            static_cast<uint32_t>(dac_code)));
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(ADC_CALIB_STEP_DELAY_MS));
        points.at(idx).actual_adc_code = PowerSmoothing::get_last_adc_raw();
    }
    write_ad5693_dac(0);  // Reset DAC to 0V

    save_calibration_to_flash(points);
}

void get_adc_calibration_results(nv::mctp::NsmTFFGetAdcCalibResultsRes& response)
{
    AdcCalibrationPoints points{};
    const bool           flash_loaded = load_calibration_from_flash(points);

    response.testComplete   = flash_loaded ? 1U : 0U;
    response.testInProgress = 0U;
    response.numPoints      = nv::mctp::ADC_CALIBRATION_NUM_POINTS;
    response.resvd          = 0U;

    static_assert(sizeof(response.points) == sizeof(points),
                  "ADC calibration point buffers must stay size-compatible");
    std::copy(points.begin(), points.end(), std::begin(response.points));
}

bool adc_calib_set_loopback_dac_code(uint16_t dac_code)
{
    return write_ad5693_dac(dac_code);
}

// ============================================================================
// PowerSmoothing Class Implementation
// ============================================================================

TelemetrySnapshot PowerSmoothing::get_telemetry_snapshot()
{
    const uint32_t ticks_us     = ctimer::Driver::read_ticks();
    const uint64_t timestamp_ms = static_cast<uint64_t>(ticks_us) / 1000;

    return TelemetrySnapshot{
        .timestamp_ms             = timestamp_ms,
        .soc_percent_filtered_sum = power_manager.telemetry_accumulators
                                        .soc_percent_filtered_sum,
        .pwr_brake_time_ms = power_manager.telemetry_accumulators.pwr_brake_time_ms,
        .edpp_offset_sum   = power_manager.telemetry_accumulators.edpp_offset_sum,
        .isink_offset_sum  = power_manager.telemetry_accumulators.isink_offset_sum,
    };
}

TelemetryHistorySnapshot PowerSmoothing::get_telemetry_history_snapshot()
{
    const uint32_t ticks_us     = ctimer::Driver::read_ticks();
    const uint64_t timestamp_ms = static_cast<uint64_t>(ticks_us) / 1000;

    TelemetryHistorySnapshot snapshot{
        .timestamp_ms = timestamp_ms,
        .sample_count = PowerManager::TelemetryHistory::BUFFER_SIZE,
    };

    // Reorder circular buffer from oldest to newest
    // write_index points to the next write position, so it's the oldest data
    const uint8_t start_idx = power_manager.telemetry_history.write_index;

    for (size_t i = 0; i < PowerManager::TelemetryHistory::BUFFER_SIZE; i++) {
        const uint8_t src_idx = (start_idx + i) % PowerManager::TelemetryHistory::BUFFER_SIZE;
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
        snapshot.soc_percent_filtered[i] = power_manager.telemetry_history
                                               .soc_percent_filtered[src_idx];
        snapshot.edpp_offset[i]  = power_manager.telemetry_history.edpp_offset[src_idx];
        snapshot.isink_offset[i] = power_manager.telemetry_history.isink_offset[src_idx];
        // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
    }

    return snapshot;
}

uint16_t PowerSmoothing::get_last_adc_raw()
{
    return power_manager.public_connectors.last_adc_raw;
}

uint16_t PowerSmoothing::get_last_edpp_dac_raw()
{
    return power_manager.public_connectors.last_edpp_dac_raw;
}

uint16_t PowerSmoothing::get_last_isink_dac_raw()
{
    return power_manager.public_connectors.last_isink_dac_raw;
}

// The controller runs every 100us. It is not feasible to increase the RTOS tick
// rate to support running this in a task as the context switch overhead would
// be too high. Instead, the controller is run in the ADC interrupt handler. The
// ADC measuring the state of charge signal must be configured to run
// continuously such that it performs a sample every 100us and raises an
// interrupt when the sample is ready. The performace of this function is
// critical due to the frequency that it is called. Thus, all functions called
// by it must implemented inline such that the use of the [[gnu::flatten]]
// attribute ensures that the functions are inlined. To ensure instruction
// fetching is not a bottleneck, the LPCAC is disabled during the execution of
// this function and the function is placed in the RAMX SRAM block. Note that
// this approach assumes core1 is not fetching instructions from the RAMX SRAM
// as well, or else there will be contention which will degrade performance. See
// the MCXNx4x datasheet for more details about the memory subsystem.

// NOLINTBEGIN
NV_SRAMX_CODE void PowerSmoothing::adc_isr()
{
    sys::lpcac::Lpcac::disable();
    const uint32_t current_tick = ctimer::Driver::read_ticks_inline();
    time_since_last_callback    = current_tick - last_callback_time;
    if (time_since_last_callback > 200) {
        callback_time_exceeded++;
    }
    last_callback_time = current_tick;

    // Apply pending preset change BEFORE running the algorithm
    // This ensures new parameters take effect immediately
    if (preset_manager.ApplyPending()) {
        preset_manager.ApplyPresetChange(power_manager);
    }

    // Process power smoothing calculation
    // Note: we assume data is ready since interrupt fires on FIFO watermark
    PowerSmoothing::power_manager.run_iteration();

    callback_exec_time = ctimer::Driver::read_ticks_inline() - current_tick;
    if (callback_exec_time > 25) {
        callback_exec_time_exceeded++;
    }
    sys::lpcac::Lpcac::enable();
}
// NOLINTEND

void PowerSmoothing::start_power_manager()
{
    nv::logger::info(nv::logger::Event::SocPwrSmoothingStarted,
                     nv::logger::data_from_u32(static_cast<uint32_t>(1)));
    sys::adc::ADC::trigger_read(SocAdcPeripheral, SocAdcInitialTriggerCommand);
}

void PowerSmoothing::SetPowerBrakePolicyState(PowerBrakeState state)
{
    power_manager.config.power_brake_policy.enabled = (state
                                                       == PowerBrakeState::PowerBrakeEnabled);
}

PowerBrakeState PowerSmoothing::GetPowerBrakePolicyState()
{
    return power_manager.config.power_brake_policy.enabled
             ? PowerBrakeState::PowerBrakeEnabled
             : PowerBrakeState::PowerBrakeDisabled;
}

void PowerSmoothing::SetThermBrakePolicyState(ThermBrakeState state)
{
    power_manager.config.therm_brake_policy.enabled = (state
                                                       == ThermBrakeState::ThermBrakeEnabled);
}

ThermBrakeState PowerSmoothing::GetThermBrakePolicyState()
{
    return power_manager.config.therm_brake_policy.enabled
             ? ThermBrakeState::ThermBrakeEnabled
             : ThermBrakeState::ThermBrakeDisabled;
}

void PowerSmoothing::SetOffsetPolicyState(ConstantPowerMode mode)
{
    const bool enabled = (mode == ConstantPowerMode::ConstantPowerModeOn);
    power_manager.config.isink_offset_policy.enabled = enabled;
    power_manager.config.edpp_offset_policy.enabled  = enabled;
}

ConstantPowerMode PowerSmoothing::GetOffsetPolicyState()
{
    // Both policies should have the same state, so we check either one
    return power_manager.config.isink_offset_policy.enabled
             ? ConstantPowerMode::ConstantPowerModeOn
             : ConstantPowerMode::ConstantPowerModeOff;
}

void PowerSmoothing::SetMaxACRampRate(float rate)
{
    power_manager.config.max_ac_ramp_rate = rate;

    // Automatically enable/disable offset policies based on ramp rate
    // Non-zero rate enables policies, zero rate disables them
    const bool enabled                               = (rate != 0.0f);
    power_manager.config.isink_offset_policy.enabled = enabled;
    power_manager.config.edpp_offset_policy.enabled  = enabled;
}

float PowerSmoothing::GetMaxACRampRate()
{
    return power_manager.config.max_ac_ramp_rate;
}

// ============================================================================
// Persistence Helper Functions (PDS read/write)
// Uses nv::flash::Flash::get_data() and set_data() for PDS access
// ============================================================================

namespace {

bool persist_key_value(nv::flash::Key key, nv::flash::Data value)
{
    return nv::flash::Flash::set_data(key, value) == nv::flash::Status::Ok;
}

bool load_key_value(nv::flash::Key key, nv::flash::Data& value)
{
    return nv::flash::Flash::get_data(key, value) == nv::flash::Status::Ok;
}

}  // namespace

// Persist SoC Power Smoothing enabled state to PDS
bool PowerSmoothing::PersistSoCPowerSmoothEnabled(bool enabled)
{
    const nv::flash::Data value = enabled ? 1 : 0;
    return persist_key_value(nv::flash::Key::PdsSoCPowerSmoothEnabled, value);
}

// Persist SoC Power Brake enabled state to PDS
bool PowerSmoothing::PersistSoCPowerBrakeEnabled(bool enabled)
{
    const nv::flash::Data value = enabled ? 1 : 0;
    return persist_key_value(nv::flash::Key::PdsSoCPowerBrakeEnabled, value);
}

// Persist current preset index to PDS
bool PowerSmoothing::PersistSoCPowerSmoothCurrentPresetIndex(uint8_t preset_id)
{
    const auto value = static_cast<nv::flash::Data>(preset_id);
    return persist_key_value(nv::flash::Key::PdsSoCPowerSmoothCurrentPresetIndex, value);
}

// Persist max AC power ramp rate to PDS (float stored as uint32_t via bit_cast)
bool PowerSmoothing::PersistMaxACPowerRampRate(float rate)
{
    const auto value = std::bit_cast<nv::flash::Data>(rate);
    return persist_key_value(nv::flash::Key::PdsMaxACPowerRampRate, value);
}

// Persist SoC Thermal Brake enabled state to PDS
bool PowerSmoothing::PersistSoCThermBrakeEnabled(bool enabled)
{
    const nv::flash::Data value = enabled ? 1 : 0;
    return persist_key_value(nv::flash::Key::PdsSoCThermBrakeEnabled, value);
}

// Load all persisted settings from PDS and apply them
// Called at startup after flash task is initialized
void PowerSmoothing::LoadPersistedSettings()
{
    nv::flash::Data value = 0;

    // Load SoC Power Smooth Enabled
    if (load_key_value(nv::flash::Key::PdsSoCPowerSmoothEnabled, value)) {
        const bool enabled = (value == 1);
        PowerSmoothing::SetOffsetPolicyState(enabled ? ConstantPowerMode::ConstantPowerModeOn
                                                     : ConstantPowerMode::ConstantPowerModeOff);
    }

    // Load SoC Power Brake Enabled
    if (load_key_value(nv::flash::Key::PdsSoCPowerBrakeEnabled, value)) {
        const bool enabled = (value == 1);
        PowerSmoothing::SetPowerBrakePolicyState(enabled ? PowerBrakeState::PowerBrakeEnabled
                                                         : PowerBrakeState::PowerBrakeDisabled);
    }

    // Load Max AC Power Ramp Rate
    if (load_key_value(nv::flash::Key::PdsMaxACPowerRampRate, value)) {
        const auto rate = std::bit_cast<float>(value);
        // Set ramp rate directly without triggering auto-enable logic
        // (the enabled state was already loaded above)
        power_manager.config.max_ac_ramp_rate = rate;
    }

    // Load Current Preset Index
    if (load_key_value(nv::flash::Key::PdsSoCPowerSmoothCurrentPresetIndex, value)) {
        const auto preset_id = static_cast<uint8_t>(value);
        if (preset_id < NUM_PRESETS) {
            // Switch to the persisted preset (will be applied at next ISR)
            PowerSmoothing::SwitchToPreset(preset_id);
        }
    }

    // Load Thermal Brake Enabled
    if (load_key_value(nv::flash::Key::PdsSoCThermBrakeEnabled, value)) {
        const bool enabled = (value == 1);
        PowerSmoothing::SetThermBrakePolicyState(enabled ? ThermBrakeState::ThermBrakeEnabled
                                                         : ThermBrakeState::ThermBrakeDisabled);
    }
}

// ============================================================================
// Preset Parameter Management API
// ============================================================================

bool PowerSmoothing::SetParameter(uint8_t preset_id, uint8_t param_id, uint32_t value)
{
    return preset_manager.SetParameter(preset_id, param_id, value);
}

void PowerSmoothing::GetPresetParameters(uint8_t                            preset_id,
                                         std::array<uint32_t, PARAM_COUNT>& new_params)
{
    preset_manager.GetPresetParameters(preset_id, new_params);
}

bool PowerSmoothing::SwitchToPreset(uint8_t preset_id)
{
    return preset_manager.SwitchToPreset(preset_id);
}

uint8_t PowerSmoothing::GetActivePresetId()
{
    return preset_manager.GetActivePresetId();
}

uint8_t PowerSmoothing::GetSupportedPresetBitmask()
{
    return preset_manager.GetSupportedPresetBitmask();
}

void PowerSmoothing::initialize()
{
    preset_manager.InitializeDefaultPreset();
    preset_manager.ClearPending();

    // Apply default preset to power_manager.config immediately
    power_manager.config = preset_manager.GetCfgForApply(DEFAULT_PRESET, power_manager.config);

    // Note: ADC sampling and persisted settings are loaded in periodic_update()
    // after scheduler is running and flash task is active
}

void PowerSmoothing::timer_callback([[maybe_unused]] ipc::Timer& timer)
{
    periodic_update();
}

void PowerSmoothing::periodic_update()
{
    static bool        initialized               = false;
    static bool        power_brake_asserted      = false;
    static bool        callback_exec_time_logged = false;
    static bool        callback_time_logged      = false;
    constexpr uint32_t exe_threshold             = 20;
    constexpr uint32_t time_threshold            = 20;

    // One-time initialization on first call (after scheduler and flash task are running)
    if (!initialized) {
        // Load persisted settings from flash (requires flash task to be running)
        LoadPersistedSettings();

        // Start ADC sampling (requires interrupts to be enabled)
        start_power_manager();

        initialized = true;
    }

    if (SocPowerSmoothingDebugEnabled) {
        nv::logger::info(nv::logger::Event::SocPwrSmoothingSocPercent,
                         nv::logger::data_from_u32(static_cast<uint32_t>(
                             power_manager.public_connectors.soc_percent_avg
                             >> sfxp32_0_to_sfxp22_10_lshift)));
        nv::logger::info(nv::logger::Event::SocPwrSmoothingAssertPowerBrake,
                         nv::logger::data_from_u32(static_cast<uint32_t>(
                             power_manager.public_connectors.assert_power_brake)));
        nv::logger::info(nv::logger::Event::SocPwrSmoothingEdppOffset,
                         nv::logger::data_from_u32(static_cast<uint32_t>(
                             power_manager.public_connectors.edpp_offset_avg
                             >> sfxp32_0_to_sfxp22_10_lshift)));
        nv::logger::info(nv::logger::Event::SocPwrSmoothingIsinkOffset,
                         nv::logger::data_from_u32(static_cast<uint32_t>(
                             power_manager.public_connectors.isink_offset_avg
                             >> sfxp32_0_to_sfxp22_10_lshift)));
        nv::logger::info(nv::logger::Event::SocPwrSmoothingTimeSinceLastCallback,
                         nv::logger::data_from_u32(PowerSmoothing::time_since_last_callback));
        nv::logger::info(nv::logger::Event::SocPwrSmoothingCallbackExecutionTime,
                         nv::logger::data_from_u32(PowerSmoothing::callback_exec_time));
        nv::logger::info(nv::logger::Event::SocPwrSmoothingEdppResidency,
                         nv::logger::data_from_u32(static_cast<uint32_t>(
                             power_manager.edpp_offset_policy.get_residency())));
        nv::logger::info(nv::logger::Event::SocPwrSmoothingIsinkResidency,
                         nv::logger::data_from_u32(static_cast<uint32_t>(
                             power_manager.isink_offset_policy.get_residency())));
    }

    // The following are single event loggers that are only logged once per event.
    if (power_manager.public_connectors.assert_power_brake && !power_brake_asserted) {
        power_brake_asserted = true;
        nv::logger::info(nv::logger::Event::SocPwrSmoothingAssertPowerBrake,
                         nv::logger::data_from_u32(static_cast<uint32_t>(1)));
    }
    else if (!power_manager.public_connectors.assert_power_brake && power_brake_asserted) {
        power_brake_asserted = false;
        nv::logger::info(nv::logger::Event::SocPwrSmoothingAssertPowerBrake,
                         nv::logger::data_from_u32(static_cast<uint32_t>(0)));
    }
    if (PowerSmoothing::callback_exec_time_exceeded > exe_threshold
        && !callback_exec_time_logged) {
        nv::logger::info(
            nv::logger::Event::SocPwrSmoothingCallbackExecutionTimeExceeded,
            nv::logger::data_from_u32(PowerSmoothing::callback_exec_time_exceeded));
        callback_exec_time_logged = true;
    }
    else {
        PowerSmoothing::callback_exec_time_exceeded = 0;
    }
    if (PowerSmoothing::callback_time_exceeded > time_threshold && !callback_time_logged) {
        // One callback time violation is expected on startup
        nv::logger::info(nv::logger::Event::SocPwrSmoothingCallbackTimeExceeded,
                         nv::logger::data_from_u32(PowerSmoothing::callback_time_exceeded));
        callback_time_logged = true;
    }
    else {
        PowerSmoothing::callback_time_exceeded = 0;
    }
}

}  // namespace nv::soc_pwr_smoothing

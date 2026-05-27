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

// The public interface for the SoC-based power smoothing module

#include <array>
#include <cstdint>
#include "nv/ipc/task.h"
#include "nv/mctp/nsm_type_ff.h"  // For NsmTFFGetAdcCalibResultsRes
#include "nv/soc_pwr_smoothing/runtime_cfg.h"
#include "nv/soc_pwr_smoothing/presets.h"
#include "nv/soc_pwr_smoothing/power_manager.h"

// Forward declaration for timer callback parameter
namespace nv::ipc {
class Timer;
}

namespace nv::soc_pwr_smoothing {

// Telemetry snapshot structure containing timestamp and accumulated values
struct TelemetrySnapshot
{
    uint64_t timestamp_ms;              // Timestamp in milliseconds from system start
    uint64_t soc_percent_filtered_sum;  // Accumulated SoC percent (integer percent values)
    uint64_t pwr_brake_time_ms;  // Accumulated power brake assertion time in milliseconds
    uint64_t edpp_offset_sum;    // Accumulated EDPP offset (integer percent values)
    uint64_t isink_offset_sum;   // Accumulated Isink offset (integer percent values)
};

// Telemetry history snapshot containing recent samples (oldest to newest)
struct TelemetryHistorySnapshot
{
    uint64_t timestamp_ms;  // Timestamp in milliseconds from system start
    uint32_t sample_count;  // Number of valid samples (always 100)

    // Samples ordered from oldest to newest (reordered from circular buffer)
    std::array<uint8_t, 100> soc_percent_filtered;  // Integer percent values (0-100)
    std::array<uint8_t, 100> edpp_offset;           // Integer percent values (0-100)
    std::array<uint8_t, 100> isink_offset;          // Integer percent values (0-100)
};

// ============================================================================
// PowerSmoothing Class - Main module interface
// ============================================================================

class PowerSmoothing
{
public:
    // ========================================================================
    // Initialization and Lifecycle
    // ========================================================================

    // Initialize the SoC Power Smoothing module
    // Call this once at startup before scheduler starts
    static void initialize();

    // Periodic telemetry logging function
    // Called by timer after scheduler is running
    static void periodic_update();

    // Timer callback that calls periodic_update()
    static void timer_callback(ipc::Timer& timer);

    // ========================================================================
    // Control Functions
    // ========================================================================

    // Triggers the ADC measuring the state of charge to run continuously
    // After each ADC conversion, an interrupt is raised and the power manager runs
    static void start_power_manager();

    // ADC interrupt service routine - called from ADC0_IRQHandler
    // This function runs the power smoothing algorithm on each ADC conversion
    [[gnu::flatten]] static void adc_isr();

    // Power Brake Policy control functions
    static void            SetPowerBrakePolicyState(PowerBrakeState state);
    static PowerBrakeState GetPowerBrakePolicyState();

    // Thermal Brake Policy control functions
    static void            SetThermBrakePolicyState(ThermBrakeState state);
    static ThermBrakeState GetThermBrakePolicyState();

    // Offset Policy control functions (both EDPP and ISINK use the same mode)
    static void              SetOffsetPolicyState(ConstantPowerMode mode);
    static ConstantPowerMode GetOffsetPolicyState();

    // Max AC Ramp Rate control (automatically enables/disables offset policies)
    static void  SetMaxACRampRate(float rate);
    static float GetMaxACRampRate();

    // ========================================================================
    // Persistence API (PDS read/write for settings that survive power cycles)
    // ========================================================================

    // Persist individual settings to PDS - called when settings are changed via NSM
    static bool PersistSoCPowerSmoothEnabled(bool enabled);
    static bool PersistSoCPowerBrakeEnabled(bool enabled);
    static bool PersistSoCPowerSmoothCurrentPresetIndex(uint8_t preset_id);
    static bool PersistMaxACPowerRampRate(float rate);
    static bool PersistSoCThermBrakeEnabled(bool enabled);

    // Load all persisted settings from PDS and apply them
    // Called at startup after flash task is initialized
    static void LoadPersistedSettings();

    // ========================================================================
    // Preset Parameter Management API
    // ========================================================================

    // Set a single parameter in a preset
    // Returns true on success, false if:
    // - preset_id is invalid or read-only (Preset 0)
    // - param_id is out of range
    // - value fails validation
    static bool SetParameter(uint8_t preset_id, uint8_t param_id, uint32_t value);

    // Get all parameters from a preset
    // Returns array of PARAM_COUNT parameters, or empty array if preset_id invalid
    static void GetPresetParameters(uint8_t                            preset_id,
                                    std::array<uint32_t, PARAM_COUNT>& new_params);

    // Switch to a different preset (applies at next ISR iteration)
    // Returns true on success, false if preset_id invalid or not initialized
    static bool SwitchToPreset(uint8_t preset_id);

    // Get currently active preset ID
    static uint8_t GetActivePresetId();

    // Get bitmask of supported/valid presets (bit N set = preset N is valid)
    // Preset 0 is always valid (default), presets 1-3 become valid when initialized
    static uint8_t GetSupportedPresetBitmask();

    // ========================================================================
    // Telemetry API
    // ========================================================================

    // Returns a snapshot of telemetry accumulators with current timestamp
    static TelemetrySnapshot get_telemetry_snapshot();

    // Returns a snapshot of telemetry history buffers with current timestamp
    // Data is reordered from circular buffer (oldest to newest)
    static TelemetryHistorySnapshot get_telemetry_history_snapshot();

    // Returns the last raw ADC code (updated by ISR, used for calibration)
    static uint16_t get_last_adc_raw();

    // Last 12-bit code written to EDPP / ISINK offset DACs (updated each control iteration)
    static uint16_t get_last_edpp_dac_raw();
    static uint16_t get_last_isink_dac_raw();

private:
    // ========================================================================
    // Internal State (previously global variables)
    // ========================================================================

    // Core power manager and preset manager (shared data for multicore access)
    static PowerManager  power_manager;
    static PresetManager preset_manager;

    // Timing and performance monitoring
    static inline uint32_t last_callback_time          = 0;
    static inline uint32_t time_since_last_callback    = 0;
    static inline uint32_t callback_exec_time          = 0;
    static inline uint32_t callback_time_exceeded      = 0;
    static inline uint32_t callback_exec_time_exceeded = 0;
};

// ============================================================================
// ADC Calibration Test Mode - Public API (non-member functions)
// ============================================================================

/// @brief Execute the full ADC calibration (blocking, ~130ms).
/// Resets state, then runs all 26 steps (0V to 2.5V, 100mV increments) with 5ms delays.
/// Results are saved to flash upon completion.
/// Must be called from a task with sufficient stack (e.g. MCTP task, not Tmr Svc).
void execute_adc_calibration();

/// @brief Get ADC calibration results
/// Populates response struct from flash-backed calibration data.
/// @param[out] response Response struct to populate
void get_adc_calibration_results(nv::mctp::NsmTFFGetAdcCalibResultsRes& response);

/// @brief Write raw DAC code to ADC calibration loopback DAC (I2C).
/// Does not delay for analog settling; host should wait before @ref
/// PowerSmoothing::get_last_adc_raw.
/// @return true on successful I2C write
bool adc_calib_set_loopback_dac_code(uint16_t dac_code);

}  // namespace nv::soc_pwr_smoothing

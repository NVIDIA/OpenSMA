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

#include NV_IPC_CONFIG_H
#include "nv/soc_pwr_smoothing/devices.h"
#include "nv/soc_pwr_smoothing/runtime_cfg.h"
#include "nv/soc_pwr_smoothing/offset_policy.h"
#include "nv/soc_pwr_smoothing/power_brake_policy.h"
#include "nv/soc_pwr_smoothing/thermal_brake_policy.h"
#include "nv/soc_pwr_smoothing/soc_sma_filter_ch.h"
#include "nv/soc_pwr_smoothing/debug_telemetry_sma_ch.h"

namespace nv::soc_pwr_smoothing {

class PowerManager
{
public:
    PowerManager() noexcept = default;

    void run_iteration();

    struct PublicConnectors
    {
        bool      soc_power_brake_directive;
        bool      therm_power_brake_applied;
        bool      therm_power_brake_directive;
        bool      assert_power_brake;
        SFXP22_10 soc_percent_avg;
        SFXP22_10 edpp_offset_avg;
        SFXP22_10 isink_offset_avg;
        uint16_t  last_adc_raw;        // Last raw SoC ADC code (calibration / readback)
        uint16_t  last_edpp_dac_raw;   // Last code written to EDPP offset DAC (shadow)
        uint16_t  last_isink_dac_raw;  // Last code written to ISINK offset DAC (shadow)
    };

    struct TelemetryAccumulators
    {
        uint64_t edpp_offset_sum;
        uint64_t isink_offset_sum;
        uint64_t soc_percent_filtered_sum;
        uint64_t pwr_brake_time_ms;  // Accumulated time power brake is asserted (in ms)
    };

    // Recent history buffers for debug telemetry
    // Stores 100 samples at 10ms intervals (1 second history)
    // Each sample is the average over 100 iterations (10ms window)
    struct TelemetryHistory
    {
        static constexpr size_t   BUFFER_SIZE   = 100;  // Number of samples
        static constexpr uint32_t SAMPLE_PERIOD = 100;  // Average every 100 iterations (10ms)

        std::array<uint8_t, BUFFER_SIZE> soc_percent_filtered;  // Integer percent values
                                                                // (0-100)
        std::array<uint8_t, BUFFER_SIZE> edpp_offset;   // Integer percent values (0-100)
        std::array<uint8_t, BUFFER_SIZE> isink_offset;  // Integer percent values (0-100)
        uint8_t                          write_index;   // Circular buffer write position (0-99)
        uint32_t sample_counter;  // Iteration counter for averaging (0-99)

        // Accumulators for computing 10ms average (reset after each sample period)
        uint32_t soc_sum;    // Sum of SoC values over sample period
        uint32_t edpp_sum;   // Sum of EDPP values over sample period
        uint32_t isink_sum;  // Sum of iSink values over sample period
    };

    RuntimeCfg            config{};
    PublicConnectors      public_connectors{};
    TelemetryAccumulators telemetry_accumulators{};
    TelemetryHistory      telemetry_history{};

    [[no_unique_address]] StateOfChargeDev                             state_of_charge_dev{};
    [[no_unique_address]] DacPercentDev<EdppDacPeripheral>             edpp_offset_dac_dev{};
    [[no_unique_address]] DacPercentDev<IsinkDacPeripheral>            isink_offset_dac_dev{};
    [[no_unique_address]] GpioInDev<McuThermWarnPort, McuThermWarnPin> mcu_thermal_warn_dev{};
    [[no_unique_address]] GpioOutDev<PwrBrakeGpioPort, PwrBrakeGpioPin>
                                              power_brake_gpio_dev{};  // active-low by default
    [[no_unique_address]] PowerBrakePolicy    power_brake_policy{config.power_brake_policy};
    [[no_unique_address]] ThermBrakePolicy    therm_brake_policy{config.therm_brake_policy};
    [[no_unique_address]] OffsetPolicy        isink_offset_policy{config.isink_offset_policy};
    [[no_unique_address]] OffsetPolicy        edpp_offset_policy{config.edpp_offset_policy};
    [[no_unique_address]] SocSmaFilterCh      soc_percent_sma_ch{};  // 400μs moving average
    [[no_unique_address]] DebugTelemetrySmaCh state_of_charge_sma_ch{};
    [[no_unique_address]] DebugTelemetrySmaCh edpp_offset_sma_ch{};
    [[no_unique_address]] DebugTelemetrySmaCh isink_offset_sma_ch{};
};

inline void PowerManager::run_iteration()
{
    SFXP22_10 soc_percent{};
    SFXP22_10 soc_percent_filtered{};
    SFXP22_10 soc_voltage_V{};
    SFXP22_10 edpp_offset{};
    SFXP22_10 isink_offset{};
    uint16_t  adc_raw{};

    mcu_thermal_warn_dev.evaluate({.assert = public_connectors.therm_power_brake_applied});
    therm_brake_policy.evaluate(
        {.pin_asserted      = public_connectors.therm_power_brake_applied,
         .apply_therm_brake = public_connectors.therm_power_brake_directive});

    state_of_charge_dev.evaluate({
        .percent       = soc_percent,
        .soc_voltage_V = soc_voltage_V,
        .adc_raw       = adc_raw,
    });

    // Store raw ADC code for calibration access
    public_connectors.last_adc_raw = adc_raw;

    // Override SoC voltage if test hook is enabled (param 20)
    // Then recalculate percent from the overridden voltage
    if (config.override_soc_input.enabled) {
        soc_voltage_V = config.override_soc_input.value;
        soc_percent   = StateOfChargeDev::soc_voltage_to_percent(soc_voltage_V);
    }

    // Apply 400μs moving average filter for offset policies
    soc_percent_sma_ch.evaluate({
        .soc_percent          = soc_percent,
        .soc_percent_filtered = soc_percent_filtered,
    });
    power_brake_policy.evaluate(
        {.soc_voltage_V      = soc_voltage_V,
         .assert_power_brake = public_connectors.soc_power_brake_directive});

    public_connectors.assert_power_brake = public_connectors.soc_power_brake_directive
                                        || public_connectors.therm_power_brake_directive;
    // Offset policies use filtered SoC for more stable control
    isink_offset_policy.evaluate<OffsetPolicy::Type::Isink>(
        {.soc_percent          = soc_percent_filtered,
         .power_brake_asserted = public_connectors.assert_power_brake,
         .offset               = isink_offset});

    edpp_offset_policy.evaluate<OffsetPolicy::Type::Edpp>(
        {.soc_percent          = soc_percent_filtered,
         .power_brake_asserted = public_connectors.assert_power_brake,
         .offset               = edpp_offset});

    state_of_charge_sma_ch.evaluate({.percent          = soc_percent_filtered,
                                     .percent_averaged = public_connectors.soc_percent_avg});

    edpp_offset_sma_ch.evaluate(
        {.percent = edpp_offset, .percent_averaged = public_connectors.edpp_offset_avg});

    isink_offset_sma_ch.evaluate(
        {.percent = isink_offset, .percent_averaged = public_connectors.isink_offset_avg});

    // Override EDPP offset output if test hook is enabled (param 41); param 43 DAC wins if
    // enabled
    if (config.override_edpp_offset.enabled) {
        edpp_offset = config.override_edpp_offset.value;
    }

    // Override ISINK offset output if test hook is enabled (param 42); param 44 DAC wins if
    // enabled
    if (config.override_isink_offset.enabled) {
        isink_offset = config.override_isink_offset.value;
    }

    edpp_offset_dac_dev.evaluate({.percent              = edpp_offset,
                                  .raw_override_enabled = config.override_edpp_dac_raw.enabled,
                                  .raw_override_dac     = config.override_edpp_dac_raw.dac_raw,
                                  .last_dac_raw         = public_connectors.last_edpp_dac_raw});

    isink_offset_dac_dev.evaluate(
        {.percent              = isink_offset,
         .raw_override_enabled = config.override_isink_dac_raw.enabled,
         .raw_override_dac     = config.override_isink_dac_raw.dac_raw,
         .last_dac_raw         = public_connectors.last_isink_dac_raw});

    power_brake_gpio_dev.evaluate({.assert = public_connectors.assert_power_brake});

    // Accumulate telemetry data (integer values, truncated)
    telemetry_accumulators.edpp_offset_sum += static_cast<uint64_t>(
        sfxp22_10_to_sfxp32_0(edpp_offset));
    telemetry_accumulators.isink_offset_sum += static_cast<uint64_t>(
        sfxp22_10_to_sfxp32_0(isink_offset));
    telemetry_accumulators.soc_percent_filtered_sum += static_cast<uint64_t>(
        sfxp22_10_to_sfxp32_0(soc_percent_filtered));

    // Accumulate power brake assertion time
    // run_iteration() is called every 100μs, so each iteration = 0.1ms
    if (public_connectors.assert_power_brake) {
        // Add 100μs = 0.1ms per iteration when power brake is asserted
        // To avoid floating point, we count in 100μs units and convert later
        // Every 10 iterations = 1ms
        static uint32_t pwr_brake_tick_count = 0;
        pwr_brake_tick_count++;
        if (pwr_brake_tick_count >= 10) {  // 10 * 100μs = 1ms
            telemetry_accumulators.pwr_brake_time_ms++;
            pwr_brake_tick_count = 0;
        }
    }

    // Accumulate values for 10ms averaging (every iteration)
    // Convert to integer percent values and accumulate
    telemetry_history.soc_sum += static_cast<uint32_t>(
        sfxp22_10_to_sfxp32_0(soc_percent_filtered));
    telemetry_history.edpp_sum  += static_cast<uint32_t>(sfxp22_10_to_sfxp32_0(edpp_offset));
    telemetry_history.isink_sum += static_cast<uint32_t>(sfxp22_10_to_sfxp32_0(isink_offset));

    // Compute and store average every 10ms (100 iterations)
    // Provides 1 second rolling history at 100Hz sample rate with 10ms averaging window
    if (++telemetry_history.sample_counter >= TelemetryHistory::SAMPLE_PERIOD) {
        const uint8_t idx = telemetry_history.write_index;

        // Store averaged values (divide by 100 iterations)
        telemetry_history.soc_percent_filtered[idx] = static_cast<uint8_t>(
            telemetry_history.soc_sum / TelemetryHistory::SAMPLE_PERIOD);
        telemetry_history.edpp_offset[idx] = static_cast<uint8_t>(
            telemetry_history.edpp_sum / TelemetryHistory::SAMPLE_PERIOD);
        telemetry_history.isink_offset[idx] = static_cast<uint8_t>(
            telemetry_history.isink_sum / TelemetryHistory::SAMPLE_PERIOD);

        // Reset accumulators and counter for next sample period
        telemetry_history.soc_sum        = 0;
        telemetry_history.edpp_sum       = 0;
        telemetry_history.isink_sum      = 0;
        telemetry_history.sample_counter = 0;

        // Advance circular buffer index
        telemetry_history.write_index = (idx + 1) % TelemetryHistory::BUFFER_SIZE;
    }
}

}  // namespace nv::soc_pwr_smoothing

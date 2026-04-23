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
#include "nv/ut/unittest.h"

#include "nv/common/fixed_point.h"
#include "nv/soc_pwr_smoothing/devices.h"
#include "nv/soc_pwr_smoothing/offset_policy.h"
#include "nv/soc_pwr_smoothing/power_brake_policy.h"

using namespace nv;
using namespace ut;
using namespace soc_pwr_smoothing;

namespace {

bool is_near(int32_t expected, int32_t actual, int32_t tolerance)
{
    int32_t delta = expected - actual;
    delta = delta < 0 ? -delta : delta;
    return delta <= tolerance;
}

} // namespace

TEST(project_prairie_test, StateOfChargeDevSocVoltageToPercent)
{
    // Test soc_voltage_to_percent: Linear mapping from 1.0V = 0% to 7.0V = 100%
    ensure::is_eq(StateOfChargeDev::soc_voltage_to_percent(to_sfxp22_10(1.0)), to_sfxp22_10(0));
    ensure::is_true(is_near(StateOfChargeDev::soc_voltage_to_percent(to_sfxp22_10(4.0)), to_sfxp22_10(50), 3));
    ensure::is_eq(StateOfChargeDev::soc_voltage_to_percent(to_sfxp22_10(7.0)), to_sfxp22_10(100));

    // Test clamping at boundaries
    ensure::is_eq(StateOfChargeDev::soc_voltage_to_percent(to_sfxp22_10(0.0)), to_sfxp22_10(0));  // Below min
    ensure::is_eq(StateOfChargeDev::soc_voltage_to_percent(to_sfxp22_10(10.0)), to_sfxp22_10(100));  // Above max
};

TEST(project_prairie_test, DacPercentDevPercentToDacRaw)
{
    ensure::is_eq(DacPercentDev<0>::percent_to_dac_raw(to_sfxp22_10(0)), 0);

    constexpr uint32_t dac_steps = 1U << sys::dac::Dac::ResolutionBits;
    constexpr uint32_t max_raw = (dac_steps - 1) * OvrmMaxDacOutputV / (sys::dac::Dac::MaxVoltage_mV / 1000.0);
    ensure::is_true(is_near(DacPercentDev<0>::percent_to_dac_raw(to_sfxp22_10(100)), max_raw, 1));
};

TEST(project_prairie_test, EnterAndExitPowerBreak)
{
    constexpr auto config = PowerBrakePolicy::RuntimeCfg{
        .entry_threshold = to_sfxp22_10(2),  // Voltage threshold to assert brake
        .exit_threshold = to_sfxp22_10(4),   // Voltage threshold to deassert brake
        .enabled = true
    };
    auto policy = PowerBrakePolicy(config);
    auto evaluate = [&](SFXP22_10 soc_voltage_V) -> bool {
        bool assert_power_brake = false;
        policy.evaluate({
            .soc_voltage_V = soc_voltage_V,
            .assert_power_brake = assert_power_brake
        });
        return assert_power_brake;
    };

    // Should not assert when above entry threshold (voltage higher means more charge)
    ensure::is_false(evaluate(config.entry_threshold + to_sfxp22_10(1)));

    // Should assert when voltage drops below entry threshold
    ensure::is_true(evaluate(config.entry_threshold - to_sfxp22_10(1)));

    // Should remain asserted while in hysteresis zone (between entry and exit thresholds)
    ensure::is_true(evaluate(config.entry_threshold + to_sfxp22_10(1)));

    // Should deassert when voltage rises above exit threshold
    ensure::is_false(evaluate(config.exit_threshold + to_sfxp22_10(1)));

    // Should remain deasserted while in hysteresis zone
    ensure::is_false(evaluate(config.exit_threshold - to_sfxp22_10(1)));
};

TEST(project_prairie_test, PowerBrakePolicyResetAfterDisable)
{
    auto config = PowerBrakePolicy::RuntimeCfg{
        .entry_threshold = to_sfxp22_10(2),  // Voltage threshold to assert brake
        .exit_threshold = to_sfxp22_10(4),   // Voltage threshold to deassert brake
        .enabled = true
    };
    bool assert_power_brake = false;
    auto policy = PowerBrakePolicy(config);

    // Assert power brake by going below entry threshold
    SFXP22_10 low_voltage = config.entry_threshold - to_sfxp22_10(1);
    policy.evaluate({low_voltage, assert_power_brake});
    ensure::is_true(assert_power_brake);

    // Disable and ensure power brake is deasserted
    config.enabled = false;
    policy.evaluate({low_voltage, assert_power_brake});
    ensure::is_false(assert_power_brake);

    // Re-enable and ensure power brake can be asserted again
    config.enabled = true;
    policy.evaluate({low_voltage, assert_power_brake});
    ensure::is_true(assert_power_brake);
};

TEST(project_prairie_test, ResidencyEwmaSaturates)
{
    const OffsetPolicy::ResidencySma::RuntimeCfg sma_cfg{};
    OffsetPolicy::ResidencySma                 ewma{sma_cfg};
    ensure::is_eq(ewma.evaluate(false), to_sfxp22_10(0));

    // EWMA should increase toward 100% with continuous true inputs
    SFXP22_10 prev_value = to_sfxp22_10(0);
    for (uint32_t i = 0; i < 100; ++i) {
        SFXP22_10 current = ewma.evaluate(true);
        ensure::is_false(current < prev_value);  // Should be monotonically increasing
        prev_value = current;
    }

    // After many iterations, should approach 100%
    for (uint32_t i = 0; i < 10000; ++i) {
        ewma.evaluate(true);
    }
    ensure::is_true(is_near(ewma.get_residency(), to_sfxp22_10(100), 10));

    // EWMA should decrease toward 0% with continuous false inputs
    prev_value = to_sfxp22_10(100);
    for (uint32_t i = 0; i < 100; ++i) {
        SFXP22_10 current = ewma.evaluate(false);
        ensure::is_false(current > prev_value);  // Should be monotonically decreasing
        prev_value = current;
    }

    // After many iterations, should approach 0%
    for (uint32_t i = 0; i < 10000; ++i) {
        ewma.evaluate(false);
    }
    ensure::is_true(is_near(ewma.get_residency(), to_sfxp22_10(0), 10));
};

TEST(project_prairie_test, EdppOffsetPolicyCritical)
{
    constexpr auto config = OffsetPolicy::RuntimeCfg{
        .enabled = true,
        .residency_pid = {}, // Default initialize so no residency offset is applied
        .critical_pid = {
            .target = to_sfxp22_10(20),
            .kp = to_sfxp22_10(100.0 / (20.0 - 10.0)),
            .ki = to_sfxp22_10(0),
            .kd = to_sfxp22_10(0),
        },
    };
    auto policy = OffsetPolicy(config);
    SFXP22_10 offset{};

    policy.evaluate<OffsetPolicy::Type::Edpp>({to_sfxp22_10(100), false, offset});
    ensure::is_eq(offset, to_sfxp22_10(0));

    policy.evaluate<OffsetPolicy::Type::Edpp>({to_sfxp22_10(20), false, offset});
    ensure::is_eq(offset, to_sfxp22_10(0));

    policy.evaluate<OffsetPolicy::Type::Edpp>({to_sfxp22_10(15), false, offset});
    ensure::is_eq(offset, to_sfxp22_10(50));

    policy.evaluate<OffsetPolicy::Type::Edpp>({to_sfxp22_10(10), false, offset});
    ensure::is_eq(offset, to_sfxp22_10(100));

    policy.evaluate<OffsetPolicy::Type::Edpp>({to_sfxp22_10(0), false, offset});
    ensure::is_eq(offset, to_sfxp22_10(100));
};

TEST(project_prairie_test, EdppOffsetPolicyResidency)
{
    constexpr auto config = OffsetPolicy::RuntimeCfg{
        .enabled = true,
        .residency_pid = {
            .target = to_sfxp22_10(1),
            .kp = to_sfxp22_10(-1),
            .ki = to_sfxp22_10(0),
            .kd = to_sfxp22_10(0),
            .integral_min = to_sfxp22_10(0),
            .integral_max = to_sfxp22_10(100),
        },
        .critical_pid = {}, // Default initialize so no critical offset is applied
        .residency_threshold = to_sfxp22_10(25),
    };
    auto policy = OffsetPolicy(config);
    SFXP22_10 offset{};

    // Run EWMA until it converges (use enough iterations for EWMA to settle)
    constexpr uint32_t max_iterations = 20000;
    for (uint32_t i = 0; i < max_iterations; ++i) {
        policy.evaluate<OffsetPolicy::Type::Edpp>({to_sfxp22_10(config.residency_threshold), false, offset});
        if (offset == (to_sfxp22_10(100) - config.residency_pid.target)) {
            break;
        }
        if (i < max_iterations - 1) {
            ensure::is_lt(offset, to_sfxp22_10(100) - config.residency_pid.target);
        }
    }
    // After convergence, offset should reach the expected value
    ensure::is_true(is_near(offset, to_sfxp22_10(100) - config.residency_pid.target, 10));
};

TEST(project_prairie_test, IsinkOffsetPolicyCritical)
{
    constexpr auto config = OffsetPolicy::RuntimeCfg{
        .enabled = true,
        .residency_pid = {}, // Default initialize so no residency offset is applied
        .critical_pid = {
            .target = to_sfxp22_10(80),
            .kp = to_sfxp22_10(100.0 / (80.0 - 90.0)),
            .ki = to_sfxp22_10(0),
            .kd = to_sfxp22_10(0),
        },
    };
    auto policy = OffsetPolicy(config);
    SFXP22_10 offset{};

    policy.evaluate<OffsetPolicy::Type::Isink>({to_sfxp22_10(0), false, offset});
    ensure::is_eq(offset, to_sfxp22_10(100));

    policy.evaluate<OffsetPolicy::Type::Isink>({to_sfxp22_10(80), false, offset});
    ensure::is_eq(offset, to_sfxp22_10(100));

    policy.evaluate<OffsetPolicy::Type::Isink>({to_sfxp22_10(85), false, offset});
    ensure::is_eq(offset, to_sfxp22_10(50));

    policy.evaluate<OffsetPolicy::Type::Isink>({to_sfxp22_10(90), false, offset});
    nv::info("offset_z: %d\n", offset);
    ensure::is_eq(offset, to_sfxp22_10(0));

    policy.evaluate<OffsetPolicy::Type::Isink>({to_sfxp22_10(100), false, offset});
    ensure::is_eq(offset, to_sfxp22_10(0));
};

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
#include "nv/common/fixed_point.h"
#include "nv/soc_pwr_smoothing/mpf.h"

#include <cstdint>

namespace nv::soc_pwr_smoothing {

// Policy to determine when power brake should be asserted based on SoC voltage.
// Uses hysteresis: asserts at low voltage (0.5V), deasserts at higher voltage (0.8V).
class PowerBrakePolicy : public mpf::FuncBlock
{
public:
    struct Ports
    {
        mpf::port::In<SFXP22_10> soc_voltage_V;
        mpf::port::Out<bool>     assert_power_brake;
    };

    struct RuntimeCfg
    {
        SFXP22_10 entry_threshold;  // Voltage threshold to assert brake (in volts, e.g., 0.5V)
        SFXP22_10 exit_threshold;  // Voltage threshold to deassert brake (in volts, e.g., 0.8V)
        bool      enabled;
    };

    PowerBrakePolicy(const RuntimeCfg& cfg) : _cfg(cfg) {}
    PowerBrakePolicy(RuntimeCfg&&) = delete;

    void evaluate(const Ports& ports)
    {
        if (_cfg.enabled) {
            run_policy(ports);
        }
        else {
            reset(ports);
        }
    }

    void run_policy(const Ports& ports)
    {
        // Hysteresis logic: maintain current state in the middle range
        if (ports.soc_voltage_V < _cfg.entry_threshold) {
            _asserted = true;
        }
        else if (ports.soc_voltage_V > _cfg.exit_threshold) {
            _asserted = false;
        }
        // else: maintain current _asserted state (hysteresis)

        ports.assert_power_brake = _asserted;
    }

    void reset(const Ports& ports)
    {
        _asserted                = false;
        ports.assert_power_brake = false;
    }

    // Public parameterless reset: resets internal state
    // Called when configuration parameters change
    void reset() { _asserted = false; }

private:
    const RuntimeCfg& _cfg;
    bool              _asserted{false};
};

}  // namespace nv::soc_pwr_smoothing

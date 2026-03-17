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

#include <array>
#include <algorithm>
#include <cstdint>

using namespace nv::common;

namespace nv::soc_pwr_smoothing {

// Policy to determine EDPp or ISINK offset.
class OffsetPolicy : public mpf::FuncBlock
{
public:
    // PID controller that can be used for PID
    class PidController
    {
    public:
        // Use inverted value to simplify fixed point math. Units are 1/s
        static constexpr uint32_t pid_radix = 10;
        static constexpr uint32_t dt_radix  = 31;
        static constexpr float    dt        = 100E-6;
        static constexpr SFXP32_0 dt_inv    = to_sfxp32_0(1.0f / dt);
        static constexpr SFXP1_31 dt_fp     = to_s32fxp<dt_radix>(dt);

        struct RuntimeCfg
        {
            SFXP22_10 target;
            SFXP22_10 kp;
            SFXP22_10 ki;
            SFXP22_10 kd;
            SFXP22_10 integral_min;
            SFXP22_10 integral_max;
            SFXP22_10 output_min;
            SFXP22_10 output_max;
        };

        PidController(const RuntimeCfg& cfg)
        : _cfg(cfg)
        , _integral(to_sfxp22_10(0))
        , _last_error(to_sfxp22_10(0))
        {}

        PidController(RuntimeCfg&&) = delete;

        SFXP22_10 evaluate(SFXP22_10 input)
        {
            // TODO: Determine constraints on PID configuration to ensure
            // no overflow

            const SFXP22_10 error = _cfg.target - input;

            _last_output = (_cfg.kp * error) >> pid_radix;

            const SFXP22_10 new_integral = _integral
                                         + (SFXP22_10)(((uint64_t)error * (uint64_t)dt_fp)
                                                       >> (uint64_t)(dt_radix - pid_radix));
            _integral     = std::clamp(new_integral, _cfg.integral_min, _cfg.integral_max);
            _last_output += (_cfg.ki * new_integral) >> pid_radix;

            const SFXP22_10 derivative  = (error - _last_error);
            _last_error                 = error;
            _last_output               += (_cfg.kd * derivative) >> pid_radix;

            _last_output = std::clamp(_last_output, _cfg.output_min, _cfg.output_max);

            return _last_output;
        }

        void reset()
        {
            _integral   = to_sfxp22_10(0);
            _last_error = to_sfxp22_10(0);
        }

    private:
        const RuntimeCfg& _cfg;
        SFXP22_10         _integral;
        SFXP22_10         _last_error;
        SFXP22_10         _last_output;
    };

    // Determines the percentage of time that the residency threshold was
    // crossed in the last `window_size` samples.
    // Uses bit-packing to store 1 bit per sample (32 samples per uint32_t).
    class ResidencySma
    {
    public:
        ResidencySma() = default;

        static constexpr uint32_t precision          = 31;  // 31 bits for the fractional part
        static constexpr float    ewma_tau           = 1;
        static constexpr float    loop_sample_period = 100E-6;
        static constexpr float    N                  = ewma_tau / loop_sample_period;
        static constexpr SFXP1_31
            ewma_alpha = (SFXP1_31)(2 / (1 + N) * (SFXP1_31)(1 << precision));  // 0.32U try to
                                                                                // align with
                                                                                // simple moving
                                                                                // average
        static constexpr SFXP1_31 one_minus_ewma_alpha = ((SFXP1_31)(1 << precision)
                                                          - ewma_alpha);

        // Returns the percentage of time that the residency threshold was
        // below the threshold in the last `window_size` samples.
        SFXP22_10 evaluate(bool is_past_threshold)
        {
            // Scale boolean to Q15 format: false → 0, true → (1 << 31) = 2147483648
            const SFXP1_31 input_scaled = is_past_threshold ? (1 << precision) : 0;

            // EWMA: Both multiplications produce Q62, shift right 31 to get Q31
            SFXP1_31 new_ewma = (SFXP1_31)(((uint64_t)ewma_alpha * (uint64_t)input_scaled)
                                           >> (uint64_t)precision);
            SFXP1_31 old_ewma = (SFXP1_31)(((uint64_t)one_minus_ewma_alpha * (uint64_t)_ewma)
                                           >> (uint64_t)precision);
            _ewma             = new_ewma + old_ewma;
            _residency        = sfxp1_31_to_sfxp22_10(_ewma) * 100;

            // Convert Q31 (0.0-1.0) to Q22_10 (0-100%)
            return _residency;
        }

        SFXP22_10 get_residency() const { return _residency; }

        void reset() { _ewma = 0; }

    private:
        // Running EWMA value in Q31 format
        SFXP1_31  _ewma;
        SFXP22_10 _residency;
    };

    struct Ports
    {
        mpf::port::In<SFXP22_10>  soc_percent;
        mpf::port::In<bool>       power_brake_asserted;
        mpf::port::Out<SFXP22_10> offset;
    };

    struct RuntimeCfg
    {
        bool                      enabled;
        PidController::RuntimeCfg residency_pid;
        PidController::RuntimeCfg critical_pid;
        SFXP22_10                 residency_threshold;
    };

    enum class Type
    {
        Isink,
        Edpp,
    };

    OffsetPolicy(const RuntimeCfg& cfg) : _cfg(cfg) {}

    OffsetPolicy(RuntimeCfg&&) = delete;

    // convert to percentage
    uint32_t get_residency() const { return _residency_sma.get_residency() >> 10; }

    // The policy type (ISINK or EDPp) must be specified since the residency
    // condition is flipped between the two types and the ISINK offset must be
    // flipped.
    template<Type type>
    void evaluate(const Ports& ports)
    {
        if (!_cfg.enabled) {
            // Policy disabled: both ISINK and EDPP offset = 0%
            reset_disabled<type>(ports);
        }
        else if (ports.power_brake_asserted) {
            // Power brake asserted: ISINK = 100%, EDPP = 0%
            reset_power_brake<type>(ports);
        }
        else {
            // Normal operation: run the policy
            run_policy<type>(ports);
        }
    }

    template<Type type>
    void run_policy(const Ports& ports)
    {
        // SoC is already clamped to [0%, 100%] by StateOfChargeDev
        const SFXP22_10 soc                         = ports.soc_percent;
        const bool      residency_threshold_crossed = type == Type::Isink
                                                        ? soc > _cfg.residency_threshold
                                                        : soc < _cfg.residency_threshold;
        const SFXP22_10 residency        = _residency_sma.evaluate(residency_threshold_crossed);
        const SFXP22_10 residency_offset = _residency_pid.evaluate(residency);
        const SFXP22_10 critical_offset  = _critical_pid.evaluate(soc);

        // Sum the offsets from the residency and critical PIDs and clamp the
        // result to 0-100%.
        const SFXP22_10 offset = std::clamp(
            critical_offset + residency_offset, to_sfxp22_10(0), to_sfxp22_10(100));
        ports.offset = type == Type::Isink ? to_sfxp22_10(100) - offset : offset;
    }

    template<Type type>
    void reset_power_brake(const Ports& ports)
    {
        // Power brake asserted: ISINK = 100%, EDPP = 0%
        _residency_pid.reset();
        _critical_pid.reset();
        _residency_sma.reset();
        ports.offset = type == Type::Isink ? to_sfxp22_10(100) : to_sfxp22_10(0);
    }

    template<Type type>
    void reset_disabled(const Ports& ports)
    {
        // Policy disabled: both ISINK and EDPP = 0%
        _residency_pid.reset();
        _critical_pid.reset();
        _residency_sma.reset();
        ports.offset = to_sfxp22_10(0);
    }

    void reset()
    {
        _residency_pid.reset();
        _critical_pid.reset();
        _residency_sma.reset();
    }

private:
    const RuntimeCfg& _cfg;
    PidController     _residency_pid{_cfg.residency_pid};
    PidController     _critical_pid{_cfg.critical_pid};
    ResidencySma      _residency_sma;
};

}  // namespace nv::soc_pwr_smoothing

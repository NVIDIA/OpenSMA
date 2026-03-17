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

#include NV_IPC_CONFIG_H
#include "nv/common/fixed_point.h"
#include "nv/soc_pwr_smoothing/mpf.h"

#include <cstdint>

namespace nv::soc_pwr_smoothing {

class ThermBrakePolicy : public mpf::FuncBlock
{
public:
    struct Ports
    {
        mpf::port::In<bool>  pin_asserted;
        mpf::port::Out<bool> apply_therm_brake;
    };

    struct RuntimeCfg
    {
        bool enabled;
    };

    ThermBrakePolicy(const RuntimeCfg& cfg) : _cfg(cfg) {}
    ThermBrakePolicy(RuntimeCfg&&) = delete;

    void evaluate(const Ports& ports)
    {
        ports.apply_therm_brake = _cfg.enabled && ports.pin_asserted;
    }

    void reset() { ; }

private:
    const RuntimeCfg& _cfg;
};
}  // namespace nv::soc_pwr_smoothing

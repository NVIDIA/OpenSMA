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
#include <assert.h>  // required by fsl_lpadc.h

#include "fsl_lpadc.h"

#include "nv/common/preproc.h"

#include NV_IPC_CONFIG_H

#define SYS_SENSOR_LPADC_BASE ADC0
#define SYS_SENSOR_VREF_BASE  VREF0

namespace sys::sensor {

class Driver
{
public:
    static constexpr IRQn                        Adc0Irq           = IRQn_Type::ADC0_IRQn;
    static constexpr uint32_t                    TempSensorChannel = 26;
    static constexpr uint32_t                    UserCmdId         = 1;
    static constexpr lpadc_sample_channel_mode_t ChannelMode = kLPADC_SampleChannelDiffBothSide;
    static constexpr lpadc_reference_voltage_source_t
                             VoltageReferenceSource = kLPADC_ReferenceVoltageAlt1;
    static constexpr bool    DoOffsetCalibration    = true;
    static constexpr int32_t OffsetValueA           = 0xAU;
    static constexpr int32_t OffsetValueB           = 0xAU;
    static constexpr bool    UseHighResolution      = true;
    static constexpr float   TempParameterA         = FSL_FEATURE_LPADC_TEMP_PARAMETER_A;
    static constexpr float   TempParameterB         = FSL_FEATURE_LPADC_TEMP_PARAMETER_B;
    static constexpr float   TempParameterAlpha     = FSL_FEATURE_LPADC_TEMP_PARAMETER_ALPHA;
    static constexpr float   AbsoluteZeroCelsius    = -273.15f;

    static void init();
    static bool get_current_temperature(float& result);

private:
};

};  // namespace sys::sensor

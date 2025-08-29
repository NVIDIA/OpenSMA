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
#include "sys/sensor/sensor.h"

#include "fsl_lpadc.h"
#include "fsl_vref.h"

#include "nv/common/debug.h"
#include "nv/common/preproc.h"
#include "nv/ctimer/ctimer.h"

NV_SHARED_BSS bool is_sensor_init = false;  // NOLINT(*-non-const-global-variables)

namespace {  // Anonymous namespace for internal linkage

template<class T>
static inline T minus(T a, T b)
{
    if (b > a) {
        return 0;
    }
    return (a - b);
}

}  // End of anonymous namespace

void sys::sensor::Driver::init()
{
    if (is_sensor_init == false) {
        nv::common::info("Sensor init\n");

        lpadc_conv_command_config_t g_lpadccommandconfigstruct = {};
        /* enable VREF */
        SPC0->ACTIVE_CFG1 |= 0x1u;

        vref_config_t vref_config;
        VREF_GetDefaultConfig(&vref_config);
        /* Initialize the VREF mode. */
        VREF_Init(SYS_SENSOR_VREF_BASE, &vref_config);
        /* Get a 1.8V reference voltage. */
        VREF_SetTrim21Val(SYS_SENSOR_VREF_BASE, 8U);

        lpadc_config_t              lpadc_config_struct;
        lpadc_conv_trigger_config_t lpadc_trigger_config_struct;

        /* Init ADC peripheral. */
        LPADC_GetDefaultConfig(&lpadc_config_struct);
        lpadc_config_struct.enableAnalogPreliminary = true;
        lpadc_config_struct.powerLevelMode          = kLPADC_PowerLevelAlt4;
        lpadc_config_struct.referenceVoltageSource  = VoltageReferenceSource;
        lpadc_config_struct.conversionAverageMode   = kLPADC_ConversionAverage128;
        lpadc_config_struct.FIFO0Watermark = FSL_FEATURE_LPADC_TEMP_SENS_BUFFER_SIZE - 1U;
        LPADC_Init(SYS_SENSOR_LPADC_BASE, &lpadc_config_struct);
        LPADC_DoResetFIFO0(SYS_SENSOR_LPADC_BASE);

        /* Do ADC calibration. */
        /* Request offset calibration. */
        if constexpr (DoOffsetCalibration) {
            LPADC_DoOffsetCalibration(SYS_SENSOR_LPADC_BASE);
        }
        else {
            LPADC_SetOffsetValue(SYS_SENSOR_LPADC_BASE, OffsetValueA, OffsetValueB);
        }
        /* Request gain calibration. */
        LPADC_DoAutoCalibration(SYS_SENSOR_LPADC_BASE);

        /* Set conversion CMD configuration. */
        LPADC_GetDefaultConvCommandConfig(&g_lpadccommandconfigstruct);
        g_lpadccommandconfigstruct.channelNumber       = TempSensorChannel;
        g_lpadccommandconfigstruct.sampleChannelMode   = ChannelMode;
        g_lpadccommandconfigstruct.sampleTimeMode      = kLPADC_SampleTimeADCK131;
        g_lpadccommandconfigstruct.hardwareAverageMode = kLPADC_HardwareAverageCount128;
        g_lpadccommandconfigstruct.loopCount = FSL_FEATURE_LPADC_TEMP_SENS_BUFFER_SIZE - 1U;
        g_lpadccommandconfigstruct.conversionResolutionMode = kLPADC_ConversionResolutionHigh;
        LPADC_SetConvCommandConfig(
            SYS_SENSOR_LPADC_BASE, UserCmdId, &g_lpadccommandconfigstruct);

        /* Set trigger configuration. */
        LPADC_GetDefaultConvTriggerConfig(&lpadc_trigger_config_struct);
        lpadc_trigger_config_struct.targetCommandId = UserCmdId;
        /* Configure the trigger0. */
        LPADC_SetConvTriggerConfig(SYS_SENSOR_LPADC_BASE, 0U, &lpadc_trigger_config_struct);

        /* Enable the watermark interrupt. */
        LPADC_EnableInterrupts(SYS_SENSOR_LPADC_BASE, kLPADC_FIFO0WatermarkInterruptEnable);

        // EnableIRQ(Adc0Irq);

        /* Eliminate the first two inaccurate results. */
        // LPADC_DoSoftwareTrigger(SYS_SENSOR_LPADC_BASE, 1U); /* 1U is trigger0 mask. */

        is_sensor_init = true;
    }
}

bool sys::sensor::Driver::get_current_temperature(float& result)
{
    lpadc_conv_result_t conv_result_struct = {};
    uint16_t            vbe1               = 0U;
    uint16_t            vbe8               = 0U;
    const uint32_t      ConvResultShift    = 3U;
    const float         ParameterSlope     = TempParameterA;
    const float         ParameterOffset    = TempParameterB;
    const float         ParameterAlpha     = TempParameterAlpha;
    float temperature = AbsoluteZeroCelsius; /* Absolute zero degree as the incorrect return
                                                value. */
    bool                           is_measure_done = false;
    constexpr nv::ctimer::NV_Ticks MaxRetriesTicks = 50000;
    constexpr nv::ctimer::NV_Ticks WaitTicks       = 50;

    init();

    LPADC_DoSoftwareTrigger(SYS_SENSOR_LPADC_BASE, 1U); /* 1U is trigger0 mask. */
    auto t1 = nv::ctimer::Driver::read_ticks();
    while ((minus<nv::ctimer::NV_Ticks>(nv::ctimer::Driver::read_ticks(), t1))
           < MaxRetriesTicks) {
        if (LPADC_GetStatusFlags(SYS_SENSOR_LPADC_BASE) & 1u) {
            is_measure_done = true;
            break;  // Exit the loop if the condition is met
        }
        nv::ctimer::Driver::delay_for_us(WaitTicks);
    }
    // auto t2 = nv::ctimer::Driver::read_ticks();
    // nv::common::info("time: %d\n", (t2 - t1));

    if (is_measure_done == false) {
        return is_measure_done;
    };

    if (true == LPADC_GetConvResult(SYS_SENSOR_LPADC_BASE, &conv_result_struct, (uint8_t)0)) {
        /* Read the 2 temperature sensor result. */
        vbe1 = conv_result_struct.convValue >> ConvResultShift;
        if (true
            == LPADC_GetConvResult(SYS_SENSOR_LPADC_BASE, &conv_result_struct, (uint8_t)0)) {
            vbe8 = conv_result_struct.convValue >> ConvResultShift;
            /* Final temperature = A*[alpha*(vbe8-vbe1)/(vbe8 + alpha*(vbe8-vbe1))] - B. */
            temperature = ParameterSlope
                            * (ParameterAlpha * ((float)vbe8 - (float)vbe1)
                               / ((float)vbe8 + ParameterAlpha * ((float)vbe8 - (float)vbe1)))
                        - ParameterOffset;
        }
    }

    // uint32_t intValue = static_cast<uint32_t>(temperature);
    // nv::common::info("intValue:0x%x\n", intValue);

    result = temperature;

    return is_measure_done;
}
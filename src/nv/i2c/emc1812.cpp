/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "nv/i2c/emc1812.h"
#include "nv/nv.h"

using namespace nv::i2c;

Emc1812::Emc1812(Port port, uint8_t address, nv::telemetry::TelemId item)
: TempSensor(port, address, item)
{}

I2cStatus Emc1812::get_local_high_temp(int8_t& temp_integer)
{
    uint8_t temp_high_byte = 0;

    // Read only high byte register (integer part)
    auto status = read_reg(Register::LocalTempHigh, temp_high_byte);
    if (status == I2cStatus::Ok) {
        // Convert high byte directly to signed integer temperature
        // coverity[cert_int31_c_violation]
        temp_integer = static_cast<int8_t>(temp_high_byte);
    }
    return status;
}

// The published NSM Spec of Set Thermal Threshold defines NvU8 for threshold to be set.
// So here just to align with that.
I2cStatus Emc1812::set_local_high_alert_threshold(int8_t threshold)
{
    // Write signed threshold directly
    // coverity[cert_int31_c_violation]
    return write_reg(Register::LocalTempHighLimit, static_cast<uint8_t>(threshold));
}

I2cStatus Emc1812::get_local_high_alert_threshold(int8_t& threshold)
{
    uint8_t threshold_byte = 0;
    auto    status         = read_reg(Register::LocalTempHighLimit, threshold_byte);
    if (status == I2cStatus::Ok) {
        // Convert back to signed int8_t
        // coverity[cert_int31_c_violation]
        threshold = static_cast<int8_t>(threshold_byte);
    }
    return status;
}

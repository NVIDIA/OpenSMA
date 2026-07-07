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

#include "nv/i2c/powersensor/device_manager.h"
#include "nv/i2c/powersensor/sensor.h"
#include "nv/i2c/powersensor/hscc_raa22800X.h"
#include "nv/i2c/powersensor/hscc_mp29540.h"
#include "nv/i2c/powersensor/hsc_lm5066i.h"
#include "nv/i2c/powersensor/hsc_mp5926.h"
#include "nv/i2c/powersensor/hsc_xpd712021.h"
#include "nv/gpio/driver.h"
#include "nv/nv.h"
#include "nv/logger/log.h"
#include NV_IPC_CONFIG_H

using namespace nv::i2c;
using namespace nv::i2c::power;

namespace nv::i2c::power {

// Weak default: when a project does not provide a board-strap reader, scan
// every HSC device column. Projects that have an East/West (or similar) strap
// must define a strong override of this symbol *inside* nv::i2c::power.
__attribute__((weak)) uint8_t read_hsc_gpio_strap()
{
    return SCAN_ALL_DEVICES;
}

}  // namespace nv::i2c::power

namespace {

// Compile-time contract for this project's HSC configuration. Each HSC type
// consumes its board-specific calibration differently, and none of it has a safe
// default, so every entry must carry the value it actually uses:
//   - MP5926: calibrates against Rsense and the CL strap at runtime, so both
//     must be set (rsense_milliohm > 0, cl_pin != HscClPin::Unset). Its power
//     coefficient is IC-fixed (driver default), so power_input_coeff may be left
//     empty.
//   - LM5066I / XPD712-021: Rsense is baked into power_input_coeff, so the coeff
//     must be valid (has_power_input_coeff(): m / exp_mult / mask all non-zero).
//     A zero m is doubly invalid — it is the runtime divisor in (Y*10^-R - b)/m.
//     Leaving it unset would silently fall back to the driver's 1 mΩ baseline.
// HSCC chips have no external Rsense and are not validated here.
constexpr bool hsc_config_is_valid()
{
    for (const auto& column : HscSensorList) {
        for (const auto& cfg : column) {
            switch (cfg.device_type) {
                case DeviceType::MP5926:
                    if (cfg.rsense_milliohm <= 0.0f || cfg.cl_pin == HscClPin::Unset) {
                        return false;
                    }
                case DeviceType::LM5066I:
                case DeviceType::XPD712021:
                    if (!has_power_input_coeff(cfg.power_input_coeff)) {
                        return false;
                    }
                    break;
                default: break;
            }
        }
    }
    return true;
}

static_assert(hsc_config_is_valid(),
              "Invalid HSC power-sensor config in this project's powersensor.h: MP5926 "
              "entries must set rsense_milliohm (> 0) and a CL-pin strap "
              "(HscClPin::Gnd or ::Vdd); LM5066I / XPD712021 entries must set a valid "
              "power_input_coeff (non-zero m / exp_mult / mask).");

}  // namespace

// ========== Multi-Device Identification Logic ==========
void DeviceManager::identify_power_sensors()
{
    // Reset device counters
    hsc_count_  = 0;
    hscc_count_ = 0;

    // ========== Scan HSC devices: for each device slot (col), find which variant (row) has it
    // ========== Outer loop: device index (col). read_hsc_gpio_strap(): 0xff = scan all
    // columns; else scan only that column. When HscVariantCount > 1 (e.g. East/West), always
    // scan all columns; single-variant may use GPIO column. coverity[dead_error_begin]
    const uint8_t hsc_fixed_col = read_hsc_gpio_strap();

    for (uint8_t hsc_col = 0; hsc_col < HscDeviceCount; hsc_col++) {
        if (hsc_fixed_col != SCAN_ALL_DEVICES && hsc_fixed_col != hsc_col) {
            continue;
        }
        for (uint8_t hsc_row = 0; hsc_row < HscVariantCount; hsc_row++) {
            const auto& sensor = HscSensorList.at(hsc_col).at(hsc_row);

            uint32_t mfr_id    = 0;
            uint64_t mfr_model = 0;
            auto status = read_mfr_info(POWER_SENSOR_PORT, sensor.address, mfr_id, mfr_model);

            if (status != I2cStatus::Ok) {
                continue;  // This variant doesn't have this device, try next variant
            }

            const bool verified = (mfr_id == sensor.mfr_id) && (mfr_model == sensor.mfr_model);

            if (verified) {
                add_hsc_device(sensor.address,
                               sensor.device_type,
                               sensor.power_input_coeff,
                               hsc_row,
                               sensor.temp_sensor_id,
                               sensor.power_sensor_id,
                               sensor.vout_sensor_id,
                               sensor.vin_sensor_id,
                               sensor.alert_sensor_id,
                               sensor.rsense_milliohm,
                               sensor.cl_pin);
                nv::info("Found HSC device: variant=%d, addr=0x%02X, type=%d\n",
                         hsc_row,
                         sensor.address,
                         static_cast<int>(sensor.device_type));
                break;  // This device slot is filled by this variant, move to next slot
            }
        }
    }
    for (uint8_t i = 0; i < hsc_count_; i++) {
        if (hsc_dispatch(i, [](auto& s) { return s.init(); }) != I2cStatus::Ok) {
            nv::logger::info(nv::logger::Event::I2cHscInitFailed, nv::logger::data_from_u32(i));
        }
    }

    // ========== Scan ALL HSCC devices ==========
    // coverity[dead_error_begin]
    for (uint8_t hscc_idx = 0; hscc_idx < HsccSensorList.size(); hscc_idx++) {
        const auto& sensor = HsccSensorList.at(hscc_idx);

        uint32_t mfr_id    = 0;
        uint64_t mfr_model = 0;
        auto     status = read_mfr_info(POWER_SENSOR_PORT, sensor.address, mfr_id, mfr_model);

        if (status != I2cStatus::Ok) {
            continue;  // Device not responding, try next
        }

        const bool verified = (mfr_id == sensor.mfr_id) && (mfr_model == sensor.mfr_model);

        if (verified) {
            // HSCC chips (RAA22800X, MP29540) use internal DCR/RDS(on) current
            // sensing with factory-trimmed coefficients, so no external Rsense
            // calibration is needed before adding them.
            add_hscc_device(sensor.address,
                            sensor.device_type,
                            sensor.power_input_coeff,
                            hscc_idx,
                            sensor.temp_sensor_id,
                            sensor.power_sensor_id,
                            sensor.vout_sensor_id,
                            sensor.vin_sensor_id,
                            sensor.alert_sensor_id);
            nv::info("Found HSCC device: index=%d, addr=0x%02X, type=%d\n",
                     hscc_idx,
                     sensor.address,
                     static_cast<int>(sensor.device_type));
            // Don't break - continue scanning for more devices
        }
    }

    // Initialize each detected HSCC device. For the MP29540 this unmasks OT warning/fault to
    // ALT_P# in SMBALERT_MASK1; for the RAA22800X the mask already defaults to unmasked.
    // Mirrors the HSC init loop above.
    for (uint8_t i = 0; i < hscc_count_; i++) {
        if (hscc_dispatch(i, [](auto& s) { return s.init(); }) != I2cStatus::Ok) {
            nv::info("HSCC init failed: index=%d\n", i);
        }
    }

    // Log summary
    if (hsc_count_ == 0) {
        nv::logger::info_wait(nv::logger::Event::I2cPowerSensorNotFound, {0x01});  // HSC not
                                                                                   // found
    }
    else {
        nv::info("Total HSC devices found: %d\n", hsc_count_);
    }

    if (hscc_count_ == 0) {
        nv::logger::info_wait(nv::logger::Event::I2cPowerSensorNotFound, {0x02});  // HSCC not
                                                                                   // found
    }
    else {
        nv::info("Total HSCC devices found: %d\n", hscc_count_);
    }
}

I2cStatus
DeviceManager::read_mfr_info(Port port, uint8_t address, uint32_t& mfr_id, uint64_t& mfr_model)
{
    using namespace nv::i2c;

    // Buffer size constants for PMBus block reads
    constexpr uint8_t MFR_ID_BUFFER_SIZE    = 8;   // MFR_ID buffer (typically 3-4 bytes)
    constexpr uint8_t MFR_MODEL_BUFFER_SIZE = 12;  // MFR_MODEL buffer (typically 4-8 bytes)

    // Bit shift constants for byte combining (Little Endian)
    constexpr uint8_t BYTE_1_SHIFT  = 8;
    constexpr uint8_t BYTE_2_SHIFT  = 16;
    constexpr uint8_t BYTE_3_SHIFT  = 24;
    constexpr uint8_t BITS_PER_BYTE = 8;

    // Use PowerSensor as generic PMBus reader
    PowerSensor reader(port, address);
    // ========== Read MFR_ID (PMBus 0x99) using Block Read ==========
    std::array<uint8_t, MFR_ID_BUFFER_SIZE> mfr_id_data = {};  // Buffer for MFR_ID data
    uint8_t                                 mfr_id_len  = sizeof(mfr_id_data);

    // WAR bug 6205336: read MFR_ID up to MAX_MFR_ID_ATTEMPTS times; require >= 2 ACKs.
    // Device must receive 2 ACKs to warm up MP5926 silicon; 2nd ACK's data is trusted.
    constexpr uint8_t MAX_MFR_ID_ATTEMPTS = 5;
    uint8_t           success_count       = 0;
    I2cStatus         status              = I2cStatus::Error;
    for (uint8_t attempt = 0; attempt < MAX_MFR_ID_ATTEMPTS; attempt++) {
        mfr_id_len = sizeof(mfr_id_data);
        status     = reader.read_block(PmbusReg::MFR_ID, mfr_id_data, mfr_id_len);

        if (status == I2cStatus::Ok && ++success_count >= 2) {
            break;  // 2nd ACK reached; data here is post-warmup
        }
    }
    if (success_count < 2) {
        return I2cStatus::Error;
    }

    // Parse MFR_ID based on actual byte count returned
    // PMBus Block Read returns characters in natural reading order (data[0] = first char)
    // Combine as Little Endian uint32_t (data[0] = LSB)
    // e.g., "MPS" -> data[0]='M', data[1]='P', data[2]='S' -> 0x0053504D
    if (mfr_id_len == 0) {
        // Empty MFR_ID (e.g., Renesas RAA228004/XPD712 default value)
        mfr_id = 0x00000000;
    }
    else if (mfr_id_len >= 3) {
        // PMBus MFR_ID: 3-4 bytes, stored as Little Endian
        mfr_id = static_cast<uint32_t>(mfr_id_data[0])
               | (static_cast<uint32_t>(mfr_id_data[1]) << BYTE_1_SHIFT)
               | (static_cast<uint32_t>(mfr_id_data[2]) << BYTE_2_SHIFT);

        if (mfr_id_len >= 4) {
            mfr_id |= (static_cast<uint32_t>(mfr_id_data[3]) << BYTE_3_SHIFT);
        }
    }
    else {
        return I2cStatus::Error;  // Invalid MFR_ID length
    }

    // ========== Read MFR_MODEL (PMBus 0x9A) using Block Read ==========
    std::array<uint8_t, MFR_MODEL_BUFFER_SIZE> mfr_model_data = {};  // Buffer for MFR_MODEL
    uint8_t                                    mfr_model_len  = sizeof(mfr_model_data);

    status = reader.read_block(PmbusReg::MFR_MODEL, mfr_model_data, mfr_model_len);
    if (status != I2cStatus::Ok) {
        return status;  // Block read failed
    }

    // Parse MFR_MODEL based on actual byte count returned
    // PMBus Block Read returns characters in natural reading order (data[0] = first char)
    // Combine as Little Endian uint64_t (data[0] = LSB)
    // e.g., "LM5066I\0" (8 bytes) -> 0x0049363630354D4C
    if (mfr_model_len == 0) {
        // Empty MFR_MODEL (e.g., some Renesas devices)
        mfr_model = 0x0000000000000000ULL;
    }
    else if (mfr_model_len <= 8) {
        // PMBus MFR_MODEL: ASCII string stored as Little Endian
        mfr_model = 0;
        for (uint8_t i = 0; i < mfr_model_len; i++) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            mfr_model |= (static_cast<uint64_t>(mfr_model_data[i]) << (i * BITS_PER_BYTE));
        }
    }
    else {
        return I2cStatus::Error;  // MFR_MODEL too long (> 8 bytes)
    }
    return I2cStatus::Ok;
}

// ========== Helper Functions: Add Identified Devices ==========

void DeviceManager::add_hsc_device(uint8_t                           address,
                                   DeviceType                        type,
                                   PowerSensorDirectFormatCoeff      power_input_coeff,
                                   uint8_t                           variant_index,
                                   nv::mctp::Type3TemperatureSensors temp_id,
                                   nv::mctp::Type3PowerSensors       power_id,
                                   nv::mctp::T3Voltage               vout_id,
                                   nv::mctp::T3Voltage               vin_id,
                                   nv::mctp::PowerSensorFaults       alert_id,
                                   float                             rsense_milliohm,
                                   HscClPin                          cl_pin)
{
    if (hsc_count_ < MAX_HSC_DEVICES) {
        hsc_devices_.at(hsc_count_) = IdentifiedDevice{address,
                                                       type,
                                                       power_input_coeff,
                                                       variant_index,
                                                       temp_id,
                                                       power_id,
                                                       vout_id,
                                                       vin_id,
                                                       alert_id,
                                                       rsense_milliohm,
                                                       cl_pin};
        hsc_count_++;
    }
    else {
        nv::error("HSC device limit reached (%d), cannot add more\n", MAX_HSC_DEVICES);
    }
}

void DeviceManager::add_hscc_device(uint8_t                           address,
                                    DeviceType                        type,
                                    PowerSensorDirectFormatCoeff      power_input_coeff,
                                    uint8_t                           index,
                                    nv::mctp::Type3TemperatureSensors temp_id,
                                    nv::mctp::Type3PowerSensors       power_id,
                                    nv::mctp::T3Voltage               vout_id,
                                    nv::mctp::T3Voltage               vin_id,
                                    nv::mctp::PowerSensorFaults       alert_id)
{
    if (hscc_count_ < MAX_HSCC_DEVICES) {
        // HSCC chips have no external Rsense; rsense_milliohm stays at the
        // IdentifiedDevice default (0.0f).
        hscc_devices_.at(hscc_count_) = IdentifiedDevice{address,
                                                         type,
                                                         power_input_coeff,
                                                         index,
                                                         temp_id,
                                                         power_id,
                                                         vout_id,
                                                         vin_id,
                                                         alert_id};
        hscc_count_++;
    }
    else {
        nv::error("HSCC device limit reached (%d), cannot add more\n", MAX_HSCC_DEVICES);
    }
}

// ========== Status Query Functions ==========

bool DeviceManager::has_hsc() const
{
    return hsc_count_ > 0;
}

bool DeviceManager::has_hscc() const
{
    return hscc_count_ > 0;
}

const IdentifiedDevice* DeviceManager::get_hsc_device(uint8_t index) const
{
    if (index < hsc_count_) {
        return &hsc_devices_.at(index);
    }
    return nullptr;
}

const IdentifiedDevice* DeviceManager::get_hscc_device(uint8_t index) const
{
    if (index < hscc_count_) {
        return &hscc_devices_.at(index);
    }
    return nullptr;
}

// ========== Sensor ID Lookup Functions ==========
int8_t
DeviceManager::find_hsc_index_by_temp_sensor(nv::mctp::Type3TemperatureSensors sensor_id) const
{
    for (uint8_t i = 0; i < hsc_count_; ++i) {
        if (hsc_devices_.at(i).temp_sensor_id == sensor_id) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;  // Not found
}

int8_t
DeviceManager::find_hsc_index_by_power_sensor(nv::mctp::Type3PowerSensors sensor_id) const
{
    for (uint8_t i = 0; i < hsc_count_; ++i) {
        if (hsc_devices_.at(i).power_sensor_id == sensor_id) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;  // Not found
}

int8_t DeviceManager::find_hsc_index_by_vout_sensor(nv::mctp::T3Voltage sensor_id) const
{
    for (uint8_t i = 0; i < hsc_count_; ++i) {
        if (hsc_devices_.at(i).vout_sensor_id == sensor_id) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;  // Not found
}

int8_t DeviceManager::find_hsc_index_by_vin_sensor(nv::mctp::T3Voltage sensor_id) const
{
    for (uint8_t i = 0; i < hsc_count_; ++i) {
        if (hsc_devices_.at(i).vin_sensor_id == sensor_id) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;  // Not found
}

int8_t
DeviceManager::find_hscc_index_by_temp_sensor(nv::mctp::Type3TemperatureSensors sensor_id) const
{
    for (uint8_t i = 0; i < hscc_count_; ++i) {
        if (hscc_devices_.at(i).temp_sensor_id == sensor_id) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;  // Not found
}

int8_t
DeviceManager::find_hscc_index_by_power_sensor(nv::mctp::Type3PowerSensors sensor_id) const
{
    for (uint8_t i = 0; i < hscc_count_; ++i) {
        if (hscc_devices_.at(i).power_sensor_id == sensor_id) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;  // Not found
}

int8_t DeviceManager::find_hscc_index_by_vout_sensor(nv::mctp::T3Voltage sensor_id) const
{
    for (uint8_t i = 0; i < hscc_count_; ++i) {
        if (hscc_devices_.at(i).vout_sensor_id == sensor_id) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;  // Not found
}

int8_t DeviceManager::find_hscc_index_by_vin_sensor(nv::mctp::T3Voltage sensor_id) const
{
    for (uint8_t i = 0; i < hscc_count_; ++i) {
        if (hscc_devices_.at(i).vin_sensor_id == sensor_id) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;  // Not found
}

int8_t DeviceManager::find_hsc_index_by_alert_sensor(nv::mctp::PowerSensorFaults alert_id) const
{
    for (uint8_t i = 0; i < hsc_count_; ++i) {
        if (hsc_devices_.at(i).alert_sensor_id == alert_id) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;  // Not found
}

int8_t
DeviceManager::find_hscc_index_by_alert_sensor(nv::mctp::PowerSensorFaults alert_id) const
{
    for (uint8_t i = 0; i < hscc_count_; ++i) {
        if (hscc_devices_.at(i).alert_sensor_id == alert_id) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;  // Not found
}

// ========== HSC API (Multi-Device Support) ==========

template<typename Func>
I2cStatus DeviceManager::hsc_dispatch(uint8_t device_index, Func func) const
{
    if (device_index >= hsc_count_) {
        return I2cStatus::Error;  // Invalid device index
    }

    const auto& device = hsc_devices_.at(device_index);

    switch (device.type) {
        case DeviceType::LM5066I: {
            Lm5066i sensor(POWER_SENSOR_PORT, device.address);
            if (has_power_input_coeff(device.power_input_coeff)) {
                sensor.set_power_input_coeff(device.power_input_coeff.m,
                                             device.power_input_coeff.b,
                                             device.power_input_coeff.exp_mult,
                                             device.power_input_coeff.mask);
            }
            return func(sensor);
        }
        case DeviceType::MP5926: {
            Mp5926 sensor(POWER_SENSOR_PORT, device.address);
            sensor.set_rsense_config(device.rsense_milliohm, device.cl_pin);
            if (has_power_input_coeff(device.power_input_coeff)) {
                sensor.set_power_input_coeff(device.power_input_coeff.m,
                                             device.power_input_coeff.b,
                                             device.power_input_coeff.exp_mult,
                                             device.power_input_coeff.mask);
            }
            return func(sensor);
        }
        case DeviceType::XPD712021: {
            Xpd712021 sensor(POWER_SENSOR_PORT, device.address);
            if (has_power_input_coeff(device.power_input_coeff)) {
                sensor.set_power_input_coeff(device.power_input_coeff.m,
                                             device.power_input_coeff.b,
                                             device.power_input_coeff.exp_mult,
                                             device.power_input_coeff.mask);
            }
            return func(sensor);
        }
        default: return I2cStatus::Error;
    }
}

I2cStatus DeviceManager::hsc_read_ot_warn_limit(uint8_t device_index, uint8_t& limit)
{
    return hsc_dispatch(device_index,
                        [&](auto& sensor) { return sensor.read_ot_warn_limit(limit); });
}

I2cStatus DeviceManager::hsc_write_ot_warn_limit(uint8_t device_index, uint8_t limit)
{
    return hsc_dispatch(device_index,
                        [&](auto& sensor) { return sensor.write_ot_warn_limit(limit); });
}

I2cStatus DeviceManager::hsc_read_vin(uint8_t device_index, uint32_t& microvolts)
{
    return hsc_dispatch(device_index,
                        [&](auto& sensor) { return sensor.read_vin(microvolts); });
}

I2cStatus DeviceManager::hsc_read_vout(uint8_t device_index, uint32_t& microvolts)
{
    return hsc_dispatch(device_index,
                        [&](auto& sensor) { return sensor.read_vout(microvolts); });
}

I2cStatus DeviceManager::hsc_read_temperature(uint8_t device_index, uint16_t& temperature)
{
    return hsc_dispatch(device_index,
                        [&](auto& sensor) { return sensor.read_temperature(temperature); });
}

I2cStatus DeviceManager::hsc_read_input_power(uint8_t device_index, uint32_t& milliwatts)
{
    return hsc_dispatch(device_index,
                        [&](auto& sensor) { return sensor.read_input_power(milliwatts); });
}

I2cStatus DeviceManager::hsc_read_faults(uint8_t device_index, uint16_t& faults)
{
    return hsc_dispatch(device_index, [&](auto& sensor) { return sensor.read_faults(faults); });
}

I2cStatus DeviceManager::hsc_clear_faults(uint8_t device_index)
{
    return hsc_dispatch(device_index, [&](auto& sensor) { return sensor.clear_faults(); });
}

// ========== HSCC Query Functions (Multi-Device Support) ==========
template<typename Func>
I2cStatus DeviceManager::hscc_dispatch(uint8_t device_index, Func func) const
{
    if (device_index >= hscc_count_) {
        return I2cStatus::Error;  // Invalid device index
    }

    const auto& device = hscc_devices_.at(device_index);

    switch (device.type) {
        case DeviceType::RAA22800X: {
            Raa22800X sensor(POWER_SENSOR_PORT, device.address);
            return func(sensor);
        }
        case DeviceType::MP29540: {
            Mp29540 sensor(POWER_SENSOR_PORT, device.address);
            return func(sensor);
        }
        default: return I2cStatus::Error;
    }
}

I2cStatus DeviceManager::hscc_read_ot_warn_limit(uint8_t device_index, uint8_t& limit)
{
    return hscc_dispatch(device_index,
                         [&](auto& sensor) { return sensor.read_ot_warn_limit(limit); });
}

I2cStatus DeviceManager::hscc_write_ot_warn_limit(uint8_t device_index, uint8_t limit)
{
    return hscc_dispatch(device_index,
                         [&](auto& sensor) { return sensor.write_ot_warn_limit(limit); });
}

I2cStatus DeviceManager::hscc_read_vin(uint8_t device_index, uint32_t& microvolts)
{
    return hscc_dispatch(device_index,
                         [&](auto& sensor) { return sensor.read_vin(microvolts); });
}

I2cStatus DeviceManager::hscc_read_vout(uint8_t device_index, uint32_t& microvolts)
{
    return hscc_dispatch(device_index,
                         [&](auto& sensor) { return sensor.read_vout(microvolts); });
}

I2cStatus DeviceManager::hscc_read_temperature(uint8_t device_index, uint16_t& temperature)
{
    return hscc_dispatch(device_index,
                         [&](auto& sensor) { return sensor.read_temperature(temperature); });
}

I2cStatus DeviceManager::hscc_read_input_power(uint8_t device_index, uint32_t& milliwatts)
{
    return hscc_dispatch(device_index,
                         [&](auto& sensor) { return sensor.read_input_power(milliwatts); });
}

I2cStatus DeviceManager::hscc_read_faults(uint8_t device_index, uint16_t& faults)
{
    return hscc_dispatch(device_index,
                         [&](auto& sensor) { return sensor.read_faults(faults); });
}

I2cStatus DeviceManager::hscc_clear_faults(uint8_t device_index)
{
    return hscc_dispatch(device_index, [&](auto& sensor) { return sensor.clear_faults(); });
}

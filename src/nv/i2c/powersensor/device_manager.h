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

#include "nv/i2c/port.h"
#include "nv/i2c/powersensor/types.h"
#include "nv/i2c/powersensor/sensor.h"
#include <array>
#include <utility>
#include <cstdint>

namespace nv::i2c::power {

// Value for read_hsc_gpio_strap() / read_hsc_variant(): scan all HSC device columns.
constexpr uint8_t SCAN_ALL_DEVICES = 0xff;

// PMBus standard register addresses used by DeviceManager
namespace PmbusReg {
constexpr uint8_t MFR_ID    = 0x99;  // Manufacturer ID (Block Read, 3-4 bytes)
constexpr uint8_t MFR_MODEL = 0x9A;  // Manufacturer Model (Block Read, 4-8 bytes)
}  // namespace PmbusReg

// Device information structure for multi-device support
struct IdentifiedDevice
{
    uint8_t                      address;
    DeviceType                   type;
    PowerSensorDirectFormatCoeff power_input_coeff;
    uint8_t variant_index;  // For HSC: 0=East, 1=West; For HSCC: device index
    nv::mctp::Type3TemperatureSensors temp_sensor_id;   // NSM Type 3 Temperature sensor ID
    nv::mctp::Type3PowerSensors       power_sensor_id;  // NSM Type 3 Power sensor ID
    nv::mctp::T3Voltage               vout_sensor_id;   // NSM Type 3 Voltage Output sensor ID
    nv::mctp::T3Voltage               vin_sensor_id;    // NSM Type 3 Voltage Input sensor ID
    nv::mctp::PowerSensorFaults       alert_sensor_id;  // NSM Type 3 Alert sensor ID
};

/**
 * HSC/HSCC Unified Device Manager (Multi-Device Support)
 *
 * Manages multiple HSC (West, East) and HSCC devices in the power chain:
 * 54V Input -> HSC West/East -> HSCC -> 12V Output
 *
 * Design:
 * - Configuration in config.h (PowerSensorListConfig lists)
 * - Identification stores all detected devices
 * - Support accessing individual devices by index
 *
 * Usage:
 *   DeviceManager manager;
 *   manager.identify_power_sensors();
 *
 *   // Access multiple HSC devices
 *   for (uint8_t i = 0; i < manager.get_hsc_count(); i++) {
 *       uint32_t vin;
 *       manager.hsc_read_vin(i, vin);
 *   }
 *
 *   // Backward compatible: default to first device
 *   uint32_t vin;
 *   manager.hsc_read_vin(vin);  // Same as hsc_read_vin(0, vin)
 */
class DeviceManager
{
public:
    // ========== Multi-Device Support Constants ==========
    static constexpr uint8_t MAX_HSC_DEVICES  = 6;  // 2 variants × 3 device types
    static constexpr uint8_t MAX_HSCC_DEVICES = 4;  // Adjust based on actual needs

    // ========== Status Query Functions ==========
    bool has_hsc() const;
    bool has_hscc() const;

    uint8_t get_hsc_count() const { return hsc_count_; }
    uint8_t get_hscc_count() const { return hscc_count_; }

    // Get device information by index
    const IdentifiedDevice* get_hsc_device(uint8_t index) const;
    const IdentifiedDevice* get_hscc_device(uint8_t index) const;

    // ========== Sensor ID Lookup Functions ==========
    // Find device index by NSM Type 3 sensor ID, returns -1 if not found
    int8_t find_hsc_index_by_temp_sensor(nv::mctp::Type3TemperatureSensors sensor_id) const;
    int8_t find_hsc_index_by_power_sensor(nv::mctp::Type3PowerSensors sensor_id) const;
    int8_t find_hsc_index_by_vout_sensor(nv::mctp::T3Voltage sensor_id) const;
    int8_t find_hsc_index_by_vin_sensor(nv::mctp::T3Voltage sensor_id) const;

    int8_t find_hscc_index_by_temp_sensor(nv::mctp::Type3TemperatureSensors sensor_id) const;
    int8_t find_hscc_index_by_power_sensor(nv::mctp::Type3PowerSensors sensor_id) const;
    int8_t find_hscc_index_by_vout_sensor(nv::mctp::T3Voltage sensor_id) const;
    int8_t find_hscc_index_by_vin_sensor(nv::mctp::T3Voltage sensor_id) const;
    int8_t find_hsc_index_by_alert_sensor(nv::mctp::PowerSensorFaults sensor_id) const;
    int8_t find_hscc_index_by_alert_sensor(nv::mctp::PowerSensorFaults sensor_id) const;

    // ========== HSC Query Functions (Multi-Device Support) ==========
    template<typename Func>
    I2cStatus hsc_dispatch(uint8_t device_index, Func func) const;

    I2cStatus hsc_read_ot_warn_limit(uint8_t device_index, uint8_t& limit);
    I2cStatus hsc_write_ot_warn_limit(uint8_t device_index, uint8_t limit);
    I2cStatus hsc_read_vin(uint8_t device_index, uint32_t& microvolts);
    I2cStatus hsc_read_vout(uint8_t device_index, uint32_t& microvolts);
    I2cStatus hsc_read_temperature(uint8_t device_index, uint16_t& temperature);
    I2cStatus hsc_read_input_power(uint8_t device_index, uint32_t& milliwatts);
    I2cStatus hsc_read_faults(uint8_t device_index, uint16_t& faults);
    I2cStatus hsc_clear_faults(uint8_t device_index);

    // Backward compatible API: default to first device (index 0)
    I2cStatus hsc_read_vin(uint32_t& microvolts) { return hsc_read_vin(0, microvolts); }
    I2cStatus hsc_read_vout(uint32_t& microvolts) { return hsc_read_vout(0, microvolts); }
    I2cStatus hsc_read_temperature(uint16_t& temperature)
    {
        return hsc_read_temperature(0, temperature);
    }
    I2cStatus hsc_read_input_power(uint32_t& milliwatts)
    {
        return hsc_read_input_power(0, milliwatts);
    }
    I2cStatus hsc_read_faults(uint16_t& faults) { return hsc_read_faults(0, faults); }
    // I2cStatus hsc_clear_faults() { return hsc_clear_faults(0); }

    // ========== HSCC Query Functions (Multi-Device Support) ==========
    template<typename Func>
    I2cStatus hscc_dispatch(uint8_t device_index, Func func) const;

    I2cStatus hscc_read_ot_warn_limit(uint8_t device_index, uint8_t& limit);
    I2cStatus hscc_write_ot_warn_limit(uint8_t device_index, uint8_t limit);
    I2cStatus hscc_read_vin(uint8_t device_index, uint32_t& microvolts);
    I2cStatus hscc_read_vout(uint8_t device_index, uint32_t& microvolts);
    I2cStatus hscc_read_temperature(uint8_t device_index, uint16_t& temperature);
    I2cStatus hscc_read_input_power(uint8_t device_index, uint32_t& milliwatts);
    I2cStatus hscc_read_faults(uint8_t device_index, uint16_t& faults);
    I2cStatus hscc_clear_faults(uint8_t device_index);

    // Backward compatible API: default to first device (index 0)
    I2cStatus hscc_read_vin(uint32_t& microvolts) { return hscc_read_vin(0, microvolts); }
    I2cStatus hscc_read_vout(uint32_t& microvolts) { return hscc_read_vout(0, microvolts); }
    I2cStatus hscc_read_temperature(uint16_t& temperature)
    {
        return hscc_read_temperature(0, temperature);
    }
    I2cStatus hscc_read_input_power(uint32_t& milliwatts)
    {
        return hscc_read_input_power(0, milliwatts);
    }
    I2cStatus hscc_read_faults(uint16_t& faults) { return hscc_read_faults(0, faults); }
    I2cStatus hscc_clear_faults() { return hscc_clear_faults(0); }

    // ========== Identification API ==========
    /**
     * Identify all power sensors by reading PowerSensor lists from config.h
     * Must be called explicitly after system is ready (not in constructor)
     */
    void identify_power_sensors();

private:
    // ========== Multi-Device Storage ==========
    std::array<IdentifiedDevice, MAX_HSC_DEVICES> hsc_devices_;
    uint8_t                                       hsc_count_ = 0;

    std::array<IdentifiedDevice, MAX_HSCC_DEVICES> hscc_devices_;
    uint8_t                                        hscc_count_ = 0;

    /**
     * Read MFR_ID and MFR_MODEL from device
     */
    I2cStatus read_mfr_info(Port port, uint8_t address, uint32_t& mfr_id, uint64_t& mfr_model);

    /**
     * Helper functions to add identified devices
     */
    void add_hsc_device(uint8_t                           address,
                        DeviceType                        type,
                        PowerSensorDirectFormatCoeff      power_input_coeff,
                        uint8_t                           variant_index,
                        nv::mctp::Type3TemperatureSensors temp_id,
                        nv::mctp::Type3PowerSensors       power_id,
                        nv::mctp::T3Voltage               vout_id,
                        nv::mctp::T3Voltage               vin_id,
                        nv::mctp::PowerSensorFaults       alert_id);
    void add_hscc_device(uint8_t                           address,
                         DeviceType                        type,
                         PowerSensorDirectFormatCoeff      power_input_coeff,
                         uint8_t                           index,
                         nv::mctp::Type3TemperatureSensors temp_id,
                         nv::mctp::Type3PowerSensors       power_id,
                         nv::mctp::T3Voltage               vout_id,
                         nv::mctp::T3Voltage               vin_id,
                         nv::mctp::PowerSensorFaults       alert_id);
};

}  // namespace nv::i2c::power

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

/**
 * @file leak_detect.cpp
 * @brief Leak Detection Driver Implementation
 *
 * Implementation of the leak detection consumer for voltage monitor readings.
 */

#include <array>
#include <cstddef>
#include <utility>

#include "nv/volt_mon/leak_detect.h"
#include "nv/logger/log.h"
#include "nv/nv.h"

#include "nv/gpio/driver.h"
#include "nv/iox/task.h"

#include "nv/flash/flash.h"

namespace nv::leak_detect {

using namespace nv::ipc::voltage_monitor_config;

template<size_t... Indices>
inline void leak_detect_init_sensors(nv::volt_mon::LeakDetectSensor* sensors,
                                     std::index_sequence<Indices...> /*indices*/)
{
    ((sensors[Indices] = leak_detect_get_sensor_config<Indices>()), ...);
}

LeakDetect::LeakDetect()
: sensor()
, alertActive(false)
, hwGpioLevel(HwGpioState::High)
, vrGpioState(VrGpioState::Nominal)
{
    leak_detect_init_sensors(sensor.data(), std::make_index_sequence<LeakDetectSensorNum>{});
}

LeakDetect& LeakDetect::inst()
{
    static NV_SHARED_DATA LeakDetect leakDetect;
    return leakDetect;
}

/*
 * The `enabled` parameter is preserved for legacy compatibility: legacy
 * volt_mon::init(bool init_mcu_temp, bool init_leak_detect, ...) propagates the
 * `init_leak_detect` flag here so projects could opt out at runtime. Timer-driven
 * init always calls this with the default `true` because per-module enable/disable
 * is now expressed at compile time via `LeakDetectSensorNum` in the project config.
 */
void LeakDetect::init(bool enabled)
{
    enabled_ = enabled;

    if (!enabled_) {
        return;
    }

    // Load thresholds from PDS (overrides config.h defaults if previously saved).
    load_pds_thresholds();

    // Virtual GPIO is initialized by the iox module; LeakDetect only caches its state.

    initialize_hardware_gpio();
}

namespace {
constexpr uint32_t LowHalfMask = 0xFFFF;

/*
 * PDS leak-detect threshold layout (2 x uint32_t per sensor slot):
 *
 *   Word 0:  [31:16] sensorId   [15:0] minLeak
 *   Word 1:  [31:16] maxLeak    [15:0] maxNormal
 */

nv::flash::Key pds_key(uint8_t slot, uint8_t offset)
{
    return static_cast<nv::flash::Key>(
        static_cast<uint32_t>(nv::flash::Key::PdsLeakDetSlot0Part0)
        + slot * PdsLeakDetEntriesPerSlot + offset);
}

bool are_thresholds_valid_adc(Threshold minLeak, Threshold maxLeak, Threshold maxNormal)
{
    const auto adcMax = to_adc_value(MaxVol);
    const auto H      = Hysteresis;

    if (!(minLeak < maxLeak && maxLeak < maxNormal && maxNormal < adcMax)) {
        return false;
    }

    /**
     * Hysteresis boundary checks (all in ADC domain):
     *
     *  Short  [0 .......... minLeak+H]
     *                                [maxLeak-H .......... maxNormal+H]  Nominal
     *  Leak   [minLeak-H ...... maxLeak+H]
     *                                [maxNormal-H ........... adcMax] Open
     */
    if (minLeak < H) {
        return false;
    }
    if (adcMax - maxNormal < H) {
        return false;
    }

    const uint32_t MinGap = static_cast<uint32_t>(H) * 2U + 1U;
    if (static_cast<uint32_t>(maxLeak) - static_cast<uint32_t>(minLeak) < MinGap) {
        return false;
    }
    if (static_cast<uint32_t>(maxNormal) - static_cast<uint32_t>(maxLeak) < MinGap) {
        return false;
    }

    return true;
}
}  // namespace

void LeakDetect::load_pds_thresholds()
{
    for (uint8_t slot = 0; slot < PdsLeakDetMaxSlots; ++slot) {
        nv::flash::Data pack0 = 0;
        nv::flash::Data pack1 = 0;
        const auto status0    = nv::flash::Flash::get_data_from_kernel(pds_key(slot, 0), pack0);
        const auto status1    = nv::flash::Flash::get_data_from_kernel(pds_key(slot, 1), pack1);

        if (status0 != nv::flash::Status::Ok || status1 != nv::flash::Status::Ok) {
            continue;
        }

        if (pack0 == 0 && pack1 == 0) {
            continue;
        }

        const auto id        = static_cast<uint8_t>(pack0 >> 16);
        const auto minLeak   = static_cast<Threshold>(pack0 & LowHalfMask);
        const auto maxLeak   = static_cast<Threshold>(pack1 >> 16);
        const auto maxNormal = static_cast<Threshold>(pack1 & LowHalfMask);

        if (!are_thresholds_valid_adc(minLeak, maxLeak, maxNormal)) {
            continue;
        }

        for (uint8_t i = 0; i < LeakDetectSensorNum; ++i) {
            if (sensor.at(i).id == id) {
                auto& s     = sensor.at(i);
                s.minLeak   = minLeak;
                s.maxLeak   = maxLeak;
                s.maxNormal = maxNormal;
                break;
            }
        }
    }
}

void LeakDetect::save_pds_thresholds(uint8_t sensorIdx)
{
    if (sensorIdx >= PdsLeakDetMaxSlots) {
        return;
    }

    const auto&           s     = sensor.at(sensorIdx);
    const nv::flash::Data pack0 = (static_cast<uint32_t>(s.id) << 16)
                                | static_cast<uint32_t>(s.minLeak);
    const nv::flash::Data pack1 = (static_cast<uint32_t>(s.maxLeak) << 16)
                                | static_cast<uint32_t>(s.maxNormal);

    nv::flash::Flash::set_data(pds_key(sensorIdx, 0), pack0);
    nv::flash::Flash::set_data(pds_key(sensorIdx, 1), pack1);
}

void LeakDetect::initialize_hardware_gpio()
{
    hwGpioLevel = HwGpioState::High;
    nv::gpio::Driver::init_pin(
        AlertGpioPort, AlertGpioPin, nv::gpio::Direction::Output, nv::gpio::GpioState::High);
}

Status LeakDetect::process_reading(uint8_t sensorId, Reading adcReading)
{
    if (!enabled_) {
        return Status::Ok;
    }

    // coverity[unsigned_compare] - LeakDetectSensorNum is not 0 once compiled
    if (sensorId >= LeakDetectSensorNum) {
        return Status::InvalidSensorId;
    }

    if (errorInjectionOverrides.at(sensorId).enabled) {
        adcReading = errorInjectionOverrides.at(sensorId).reading;
    }

    auto&      _sensor   = sensor.at(sensorId);
    const auto lastState = _sensor.state;
    _sensor.reading      = adcReading;

    if (adcReading < _sensor.minLeak) {
        _sensor.state = State::Short;
    }
    else if (adcReading < _sensor.maxLeak) {
        _sensor.state = State::Leak;
    }
    else if (adcReading < _sensor.maxNormal) {
        _sensor.state = State::Nominal;
    }
    else {
        _sensor.state = State::Open;
    }

    if (_sensor.state != lastState) {
        nv::logger::info(nv::logger::Event::LeakDetectStateChange,
                         nv::volt_mon::make_state_transition_log_data(
                             sensorId, lastState, _sensor.state, adcReading),
                         nv::logger::OutputDirection::Flash);

        // Only push to IOX / hardware GPIO / NSM on a real transition. The downstream
        // consumers reflect persistent state, so re-asserting it every tick just burns
        // I2C bandwidth and risks overflowing the IOX request queue.
        update_gpio_alert(sensorId, _sensor.state);
    }

    return Status::Ok;
}

void LeakDetect::update_gpio_alert(uint8_t sensorId, State state)
{
    // safe to cast State to VrGpioState as we have static_assert in common.h
    update_virtual_gpio(sensorId, static_cast<VrGpioState>(state));
    update_physical_gpio();
}

__attribute__((weak)) void LeakDetect::update_physical_gpio()
{
    // if any sensor is not nominal, trigger alert
    for (const auto& _sensor : sensor) {
        if (_sensor.state != State::Nominal) {
            update_hardware_gpio(HwGpioState::Low);
            alertActive = true;
            return;
        }
    }

    // if all sensors are nominal, clear alert
    update_hardware_gpio(HwGpioState::High);
    alertActive = false;
}

std::array<uint8_t, 2> LeakDetect::get_alert_pin_vals(VrGpioState state)
{
    return {static_cast<uint8_t>((static_cast<uint8_t>(state) >> 0) & 0x01),
            static_cast<uint8_t>((static_cast<uint8_t>(state) >> 1) & 0x01)};
}

void LeakDetect::update_virtual_gpio(uint8_t sensorId, VrGpioState state)
{
    // update internal virtual gpio state
    vrGpioState = state;

    if constexpr (nv::ipc::EnableIoxEmulation) {
        // update iox virtual gpio values
        auto& _sensor = sensor.at(sensorId);

        // LeakDetectSensor uses 2 pins for 2-bit state encoding
        // Pin[0] = bit 0, Pin[1] = bit 1
        const std::array<uint8_t, 2> ioxPinVals = get_alert_pin_vals(state);

        if (!nv::iox::Task::send_vrgpio_request(_sensor.ioxAddr,
                                                nv::iox::Operation::Write,
                                                _sensor.ioxPin,
                                                ioxPinVals,
                                                /*trigger_nsm_event=*/true)) {
            nv::logger::error_no_wait(nv::logger::Event::LeakDetectVrgpioUpdateFail,
                                      {sensorId,
                                       _sensor.ioxAddr,
                                       _sensor.ioxPin.at(0),
                                       _sensor.ioxPin.at(1),
                                       static_cast<uint8_t>(state)},
                                      nv::logger::OutputDirection::Flash);
        }
    }
    else if constexpr (nv::ipc::EnableLstp) {
        // LSTP virtual GPIO: pin indices live in ioxPin (see project leak_detect config).
        auto&                        _sensor = sensor.at(sensorId);
        const std::array<uint8_t, 2> pinVals = get_alert_pin_vals(state);

        for (size_t i = 0; i < pinVals.size(); ++i) {
            const auto status = nv::gpio::Driver::write_virtual_physical_gpio(
                nv::gpio::vrPort, _sensor.ioxPin.at(i), pinVals.at(i));
            if (status != nv::gpio::Status::Ok) {
                nv::logger::error_no_wait(
                    nv::logger::Event::LeakDetectVrgpioUpdateFail,
                    {sensorId, 0xff, _sensor.ioxPin.at(i), 0xff, static_cast<uint8_t>(state)},
                    nv::logger::OutputDirection::Flash);
                break;
            }
        }
    }
}

void LeakDetect::update_hardware_gpio(HwGpioState level)
{
    hwGpioLevel = level;
    nv::gpio::Driver::write(AlertGpioPort, AlertGpioPin, static_cast<uint8_t>(level));
}

// Public interface implementations
Status LeakDetect::get_sensor_info(std::span<LeakDetectSensor> info)
{
    for (size_t i = 0; i < sensor.size(); ++i) {
        info[i] = sensor.at(i);
    }

    return Status::Ok;
}

Status LeakDetect::get_thresholds(uint8_t sensorIdx, ThresholdLeakDet& config)
{
    // coverity[unsigned_compare] - LeakDetectSensorNum is not 0 once compiled
    if (sensorIdx >= LeakDetectSensorNum) {
        return Status::InvalidSensorId;
    }

    config.minLeak   = sensor.at(sensorIdx).minLeak;
    config.maxLeak   = sensor.at(sensorIdx).maxLeak;
    config.maxNormal = sensor.at(sensorIdx).maxNormal;

    return Status::Ok;
}

Status LeakDetect::find_sensor_index(uint8_t sensorId, uint8_t& sensorIdx) const
{
    // coverity[unsigned_compare] - LeakDetectSensorNum is not 0 once compiled
    for (uint8_t i = 0; i < LeakDetectSensorNum; i++) {
        // coverity[dead_error_line] - LeakDetectSensorNum is not 0 once compiled
        if (sensor.at(i).id == sensorId) {
            sensorIdx = i;
            return Status::Ok;
        }
    }
    return Status::NoMatchedSensorId;
}

Status LeakDetect::set_error_injection(SensorId sensorId, Reading adcReading)
{
    uint8_t    sensorIdx = 0;
    const auto status    = find_sensor_index(sensorId, sensorIdx);
    if (status != Status::Ok) {
        return status;
    }

    auto& errInjection   = errorInjectionOverrides.at(sensorIdx);
    errInjection.enabled = true;
    errInjection.reading = adcReading;

    return Status::Ok;
}

Status LeakDetect::clear_error_injection(SensorId sensorId)
{
    uint8_t    sensorIdx = 0;
    const auto status    = find_sensor_index(sensorId, sensorIdx);
    if (status != Status::Ok) {
        return status;
    }

    auto& errInjection   = errorInjectionOverrides.at(sensorIdx);
    errInjection.enabled = false;
    errInjection.reading = 0;

    return Status::Ok;
}

void LeakDetect::clear_error_injection()
{
    for (auto& errInjection : errorInjectionOverrides) {
        errInjection.enabled = false;
        errInjection.reading = 0;
    }
}

Status LeakDetect::set_thresholds(uint8_t sensorIdx, const ThresholdLeakDet& config)
{
    // coverity[unsigned_compare] - LeakDetectSensorNum is not 0 once compiled
    if (sensorIdx >= LeakDetectSensorNum) {
        return Status::InvalidSensorId;
    }

    const auto minL = to_adc_value(config.minLeak);
    const auto maxL = to_adc_value(config.maxLeak);
    const auto maxN = to_adc_value(config.maxNormal);

    if (!are_thresholds_valid_adc(minL, maxL, maxN)) {
        return Status::InvalidThreshold;
    }

    auto& _sensor     = sensor.at(sensorIdx);
    _sensor.minLeak   = minL;
    _sensor.maxLeak   = maxL;
    _sensor.maxNormal = maxN;

    save_pds_thresholds(sensorIdx);

    return Status::Ok;
}

}  // namespace nv::leak_detect

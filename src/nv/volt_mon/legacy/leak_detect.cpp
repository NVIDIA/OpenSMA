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
 * Implementation of the leak detection driver following the System Initialization Flow.
 */

#include <chrono>
#include <cstring>
#include <span>

#include "nv/gpio/common.h"

#include "nv/volt_mon/leak_detect.h"
#include "nv/ctimer/ctimer.h"
#include "nv/nv.h"

#include "nv/gpio/driver.h"
#include "nv/iox/task.h"
#include "nv/ipc/task.h"

#include "nv/volt_mon/adc.h"
#include "nv/flash/flash.h"
#include "nv/logger/log.h"

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

void LeakDetect::init(bool enabled)
{
    enabled_ = enabled;

    // If disabled, skip initialization
    if (!enabled_) {
        return;
    }

    // Step 0: Load thresholds from PDS (overrides config.h defaults if previously saved)
    load_pds_thresholds();

    // Step 1: Initialize ADC Peripheral, Configure Channels, and Setup Interrupts
    initialize_adc();

    // Step 2: Initialize Virtual GPIO
    initialize_virtual_gpio();

    // Step 3: Initialize Hardware GPIO
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

void LeakDetect::initialize_adc()
{
    /**
     * Configure ADC commands with hardware thresholds
     *
     * CRITICAL: This ONLY configures ADC commands. It does NOT start ADC scanning.
     * ADC will be started later by volt_mon::init() after ALL modules have
     * configured their commands.
     *
     * @note Hardware thresholds are explicitly configured in config.h to ensure
     *       what you see is what you get, eliminating hidden Config Tool settings.
     */
    // coverity[unsigned_compare] - LeakDetectSensorNum is not 0 once compiled
    for (uint8_t i = 0; i < LeakDetectSensorNum; ++i) {
        const auto& _sensor = sensor.at(i);
        // Set hardware thresholds from config.h
        update_adccmd_threshold(i, _sensor.maxLeak, _sensor.maxNormal);
    }

    // NOTE: ADC scanning will be started by volt_mon::init()
}

void LeakDetect::initialize_virtual_gpio()
{
    // Initialize virtual GPIO state
    vrGpioState = VrGpioState::Nominal;

    /**
     * virtual gpio is initialized by iox module
     * so NOTHING to do here !!!
     */
}

void LeakDetect::initialize_hardware_gpio()
{
    hwGpioLevel = HwGpioState::High;
    nv::gpio::Driver::init_pin(
        AlertGpioPort, AlertGpioPin, nv::gpio::Direction::Output, nv::gpio::GpioState::High);
}

Status LeakDetect::reset_sensor(uint8_t sensorId)
{
    // coverity[unsigned_compare] - LeakDetectSensorNum is not 0 once compiled
    if (sensorId >= LeakDetectSensorNum) {
        return Status::InvalidSensorId;
    }

    // reset sensor state
    sensor.at(sensorId).state = State::Nominal;

    // update gpio alert
    update_gpio_alert(sensorId, State::Nominal, true);

    /** @note more to-be-implemented as requirement is NOT clear yet */

    // simple log
    nv::info("sensorId=%d reset to nominal\r\n", sensorId);

    return Status::Ok;
}

Status LeakDetect::update_sensor_state(uint8_t sensorId, Reading adcReading)
{
    // coverity[unsigned_compare] - LeakDetectSensorNum is not 0 once compiled
    if (sensorId >= LeakDetectSensorNum) {
        return Status::InvalidSensorId;
    }

    auto&      _sensor   = sensor.at(sensorId);
    const auto lastState = _sensor.state;

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

    const bool stateChanged = (_sensor.state != lastState);
    // Keep transition fact separate from NSM event policy even though they currently match.
    const bool triggerNsmEvent = stateChanged;
    // Always update vrgpio state; only trigger the NSM event conditionally
    // to keep the sync path simple and avoid corner cases.
    update_gpio_alert(sensorId, _sensor.state, triggerNsmEvent);

    return Status::Ok;
}

void LeakDetect::update_gpio_alert(uint8_t sensorId, State state, bool trigger_nsm_event)
{
    // safe to cast State to VrGpioState as we have static_assert in common.h
    update_virtual_gpio(sensorId, static_cast<VrGpioState>(state), trigger_nsm_event);
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

void LeakDetect::generate_alert()
{
    // Alert is already handled in update_sensor_state
    // This function can be extended for additional alert mechanisms
}

std::array<uint8_t, 2> LeakDetect::get_alert_pin_vals(VrGpioState state)
{
    return {static_cast<uint8_t>((static_cast<uint8_t>(state) >> 0) & 0x01),
            static_cast<uint8_t>((static_cast<uint8_t>(state) >> 1) & 0x01)};
}

void LeakDetect::update_virtual_gpio(uint8_t     sensorId,
                                     VrGpioState state,
                                     bool        trigger_nsm_event)
{
    // update internal virtual gpio state
    vrGpioState = state;

    // update iox virtual gpio values
    auto& _sensor = sensor.at(sensorId);

    // LeakDetectSensor uses 2 pins for 2-bit state encoding
    // Pin[0] = bit 0, Pin[1] = bit 1
    const std::array<uint8_t, 2> ioxPinVals = get_alert_pin_vals(state);

    nv::iox::Task::send_vrgpio_request(_sensor.ioxAddr,
                                       nv::iox::Operation::Write,
                                       _sensor.ioxPin,
                                       ioxPinVals,
                                       trigger_nsm_event);
}

void LeakDetect::update_hardware_gpio(HwGpioState level)
{
    hwGpioLevel = level;
    nv::gpio::Driver::write(AlertGpioPort, AlertGpioPin, static_cast<uint8_t>(level));
}

void LeakDetect::send_nsm_event(NsmEventType event_type, uint8_t sensor_id)
{
    // TODO Create NSM event data
}

// Public interface implementations
Status LeakDetect::get_sensor_info(std::span<LeakDetectSensor> info)
{
    auto status = Status::Ok;

    for (size_t i = 0; i < sensor.size(); ++i) {
        const auto& adcId = sensor.at(i).adcId;
        if (volt_mon::Adc::is_adc_error(adcId)) {
            status = volt_mon::Adc::adcerr.at(static_cast<uint32_t>(adcId));
            break;
        }

        // start adc one-shot sampling
        status = volt_mon::Adc::start_oneshot(sensor.at(i));
        if (status != Status::Ok) {
            break;
        }

        // update sensor state based on current reading
        update_sensor_state(i, sensor.at(i).reading);

        // Return LeakDetectSensor directly
        info[i] = sensor.at(i);

        volt_mon::Adc::dbginfo(AdcMode::OneShot,
                               sensor.at(i),
                               sensor.at(i).reading,
                               sensor.at(i).minLeak,
                               sensor.at(i).maxLeak);
    }

    /**
     * if no error, start adc scanning for all sensors
     * although sensors may not be on the same adc
     * for simplicity, we start adc scanning for all adcs
     */
    if (status == Status::Ok) {
        if constexpr (SensorOnAdc0) {
            volt_mon::Adc::start_scanning(AdcInstance::_0);
        }
        if constexpr (SensorOnAdc1) {
            volt_mon::Adc::start_scanning(AdcInstance::_1);
        }
    }

    return status;
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

    auto& _sensor = sensor.at(sensorIdx);

    volt_mon::Adc::stop_sampling(_sensor.adcId);

    _sensor.minLeak   = minL;
    _sensor.maxLeak   = maxL;
    _sensor.maxNormal = maxN;

    /**
     * update threshold to adc command
     *
     * @note adc was just stopped, and we update the normal low and high thresholds
     *       and set adc state to nominal temporarily.
     *
     *       The caller is responsible for reading the sensor value and updating
     *       the actual state and GPIO alerts, then restarting ADC scanning.
     */
    _sensor.state = State::Nominal;
    update_adccmd_threshold(sensorIdx, _sensor.maxLeak, _sensor.maxNormal);

    save_pds_thresholds(sensorIdx);

    /**
     * @note ADC is left in stopped state. The caller must:
     *       1. Read sensor value to determine actual state
     *       2. Update GPIO alerts based on actual state
     *       3. Restart ADC scanning mode
     */

    return Status::Ok;
}

void LeakDetect::get_sensor_state_threshold(uint8_t sensorId, uint16_t& thLow, uint16_t& thHigh)
{
    /** sensor id is guaranteed to be valid */

    const auto&     _sensor = sensor.at(sensorId);
    const Threshold H       = Hysteresis;
    const Threshold adcMin  = to_adc_value(MinVol);
    const Threshold adcMax  = to_adc_value(MaxVol);

    switch (_sensor.state) {
        case State::Short:
            thLow  = adcMin;
            thHigh = (_sensor.minLeak + H <= _sensor.maxLeak)
                       ? static_cast<Threshold>(_sensor.minLeak + H)
                       : _sensor.maxLeak;
            break;
        case State::Leak:
            thLow  = (_sensor.minLeak >= H) ? static_cast<Threshold>(_sensor.minLeak - H)
                                            : adcMin;
            thHigh = (_sensor.maxLeak + H <= _sensor.maxNormal)
                       ? static_cast<Threshold>(_sensor.maxLeak + H)
                       : _sensor.maxNormal;
            break;
        case State::Nominal:
            thLow  = (_sensor.maxLeak >= H && _sensor.maxLeak - H >= _sensor.minLeak)
                       ? static_cast<Threshold>(_sensor.maxLeak - H)
                       : _sensor.minLeak;
            thHigh = (adcMax - _sensor.maxNormal >= H)
                       ? static_cast<Threshold>(_sensor.maxNormal + H)
                       : adcMax;
            break;
        case State::Open:
            thLow  = (_sensor.maxNormal >= H && _sensor.maxNormal - H >= _sensor.maxLeak)
                       ? static_cast<Threshold>(_sensor.maxNormal - H)
                       : _sensor.maxLeak;
            thHigh = adcMax;
            break;
        default:
            thLow  = adcMin;
            thHigh = adcMax;
            break;
    }
}

void LeakDetect::update_adccmd_threshold(uint8_t sensorId, uint16_t thLow, uint16_t thHigh)
{
    /** sensor id is guaranteed to be valid */

    const auto&                           _sensor = sensor.at(sensorId);
    const sys::adc::ADC::AdcCommandConfig newcmd  = {
         .sampleChannelMode        = static_cast<uint8_t>(_sensor.scanMode),
         .channelNumber            = static_cast<uint8_t>(_sensor.channel),
         .chainedNextCommandNumber = static_cast<uint8_t>(_sensor.cmdNext),
         .hwCompareValueHigh       = thHigh,
         .hwCompareValueLow        = thLow,
         .loopCount                = 0,
         .sampleTimeMode           = LeakAdcSampleTime,
         .hardwareAverageMode      = LeakAdcHwAverage,
         .conversionResolutionMode = sys::adc::ADC_RESOLUTION_HIGH,
         .hardwareCompareMode      = sys::adc::ADC_COMPARE_STORE_TRUE};
    sys::adc::ADC::set_adc_command(static_cast<uint32_t>(_sensor.adcId),
                                   static_cast<uint32_t>(_sensor.cmdScanning),
                                   newcmd);
}

void LeakDetect::leak_detect_adc_isr(AdcInstance                         instance,
                                     const sys::adc::ADC::AdcConvResult& result)
{
    nv::logger::Logger::add_from_isr(nv::logger::Event::LeakDetectIsr.unique_id,
                                     nv::logger::Event::LeakDetectIsr.default_level,
                                     volt_mon::make_adc_isr_log_data(instance,
                                                                     result.commandIdSource,
                                                                     result.triggerIdSource,
                                                                     result.loopCountIndex,
                                                                     result.convValue),
                                     nv::logger::OutputDirection::Flash);

    uint8_t   sensorId = 0;
    Threshold thLow = 0, thHigh = 0;

    /**
     * identify sensor id and trigger mode from command id
     *
     * @note we have very limited number of sensors, so just loop through
     */
    // coverity[unsigned_compare] - LeakDetectSensorNum is not 0 once compiled
    for (; sensorId < LeakDetectSensorNum; sensorId++) {
        // coverity[dead_error_line] - LeakDetectSensorNum is not 0 once compiled
        if (sensor.at(sensorId).adcId == instance
            && static_cast<uint32_t>(sensor.at(sensorId).cmdScanning)
                   == result.commandIdSource) {
            break;
        }
    }

    if (sensorId == LeakDetectSensorNum || sensor.at(sensorId).sensor != Sensor::LeakDetect) {
        return;
    }

    /**
     * stop adc and reset fifo0
     *
     * @note if trigger mode is one-shot, then adc is already stopped
     *       if trigger mode is scanning, then that means we need to update new threshold
     *       thus, we need to stop adc and reset fifo0 in both cases
     */
    volt_mon::Adc::stop_sampling(instance);

    /* update sensor state due to voltage out of range */
    // coverity[dead_error_begin] - LeakDetectSensorNum is not 0 once compiled
    update_sensor_state(sensorId, static_cast<Reading>(result.convValue));

    /* get new threshold based on new state */
    get_sensor_state_threshold(sensorId, thLow, thHigh);

    /* update new threshold to sensor's adc command */
    update_adccmd_threshold(sensorId, thLow, thHigh);

    volt_mon::Adc::dbginfo(
        AdcMode::Scanning, sensor.at(sensorId), result.convValue, thLow, thHigh);

    /**
     * restart adc scanning
     *
     * @note adc should always be in scanning mode
     *       it can only be stopped temporarily
     *       if user queries sensor reading
     */
    volt_mon::Adc::start_scanning(instance);
}

}  // namespace nv::leak_detect

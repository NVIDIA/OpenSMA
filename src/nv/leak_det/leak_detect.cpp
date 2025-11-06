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

#include "leak_detect.h"
#include "sys/adc/adc.h"
#include "nv/ctimer/ctimer.h"
#include "nv/nv.h"

#include "nv/gpio/driver.h"
#include "nv/iox/task.h"
#include "nv/ipc/task.h"
#include "nv/logger/log.h"

namespace nv::leak_detect {

using namespace nv::ipc::leak_detect_config;

LeakDetect::LeakDetect()
: sensor()
, adcerr()
, alertActive(false)
, hwGpioLevel(HwGpioState::High)
, vrGpioState(VrGpioState::Nominal)
{
    init_sensors_impl(sensor.data(), std::make_index_sequence<LeakDetectSensorNum>{});
}

LeakDetect& LeakDetect::inst()
{
    static NV_SHARED_DATA LeakDetect leakDetect;
    return leakDetect;
}

void LeakDetect::init()
{
    // Step 1: Initialize ADC Peripheral, Configure Channels, and Setup Interrupts
    initialize_adc();

    // Step 2: Initialize Virtual GPIO
    initialize_virtual_gpio();

    // Step 3: Initialize Hardware GPIO
    initialize_hardware_gpio();

    // Step 4: enable adc interrupt (keep enabled to avoid accessing NVIC from non-privileged
    // task)
    if constexpr (SensorOnAdc0) {
        sys::adc::ADC::enable_adc_nvic_interrupt(static_cast<uint32_t>(AdcInstance::_0));
        sys::adc::ADC::enable_adc_lpadc_interrupt(static_cast<uint32_t>(AdcInstance::_0),
                                                  Fifo0WatermarkInterruptEnable);
    }
    if constexpr (SensorOnAdc1) {
        sys::adc::ADC::enable_adc_nvic_interrupt(static_cast<uint32_t>(AdcInstance::_1));
        sys::adc::ADC::enable_adc_lpadc_interrupt(static_cast<uint32_t>(AdcInstance::_1),
                                                  Fifo0WatermarkInterruptEnable);
    }

    // Step 5: start scanning all sensors by default
    if constexpr (SensorOnAdc0) {
        start_adc_scanning(AdcInstance::_0);
    }
    if constexpr (SensorOnAdc1) {
        start_adc_scanning(AdcInstance::_1);
    }
}

void LeakDetect::initialize_adc()
{
    /**
     * @note adc module is initialized by MCUXpresso Config Tools
     *       by manually config, so do NOT init adc hardware here !!!
     */
    sys::adc::ADC::enable_vref();
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

/**
 * @brief Start ADC sampling
 * @param adcId ADC ID
 * @return true if successful, false otherwise
 *
 * @note make sure to set adc command and trigger properly
 *       before calling this function
 */
void LeakDetect::start_adc_sampling(AdcInstance adcId)
{
    sys::adc::ADC::enable_adc(static_cast<uint32_t>(adcId), true);
    sys::adc::ADC::trigger_read(static_cast<uint32_t>(adcId),
                                1 << static_cast<uint32_t>(AdcCmdTriggerSrc));
}

/**
 * @brief Stop ADC sampling
 * @param adcId ADC ID
 * @return true if successful, false otherwise
 *
 * @note stop adc and reset fifo
 */
void LeakDetect::stop_adc_sampling(AdcInstance adcId)
{
    sys::adc::ADC::enable_adc(static_cast<uint32_t>(adcId), false);
    sys::adc::ADC::reset_fifo(static_cast<uint32_t>(adcId),
                              static_cast<uint32_t>(AdcDataResultFifo::_0));
}

Status LeakDetect::start_adc_oneshot(uint8_t sensorId, Reading& reading)
{
    // coverity[unsigned_compare] - LeakDetectSensorNum is not 0 once compiled
    if (sensorId >= LeakDetectSensorNum) {
        return Status::InvalidSensorId;
    }

    const auto& adcId         = sensor.at(sensorId).adcId;
    const auto& cmdOneShot    = sensor.at(sensorId).cmdOneShot;
    const auto& cmdTriggerSrc = sensor.at(sensorId).cmdTriggerSrc;

    /** @note adc command must be set properly through MCUXpresso Config Tools */

    // set target command to trigger
    sys::adc::ADC::set_adc_trigger(static_cast<uint32_t>(adcId),
                                   static_cast<uint32_t>(cmdTriggerSrc),
                                   static_cast<uint32_t>(cmdOneShot));

    start_adc_sampling(adcId);

    // wait until conversion is done or timeout=500ms
    for (size_t i = 0;
         i < MaxOneShotConvTime && !sys::adc::ADC::adc_ready(static_cast<uint32_t>(adcId));
         ++i) {
        constexpr uint32_t TenTicks = 10;
        vTaskDelay(TenTicks);  // 10 x 5ms = 50ms (yield cpu to avoid blocking)
    }

    // get reading
    sys::adc::ADC::AdcConvResult result{};
    if (!sys::adc::ADC::get_adc_reading(static_cast<uint32_t>(adcId),
                                        result,
                                        static_cast<uint8_t>(AdcDataResultFifo::_0))) {
        adcerr.at(static_cast<uint32_t>(adcId)) = Status::AdcOneShotConvTimeout;
        stop_adc_sampling(adcId);
        return Status::AdcOneShotConvTimeout;
    }
    reading = result.convValue;

    // reach here means conversion is done and adc is stopped

    return Status::Ok;
}

Status LeakDetect::start_adc_scanning(AdcInstance adcId)
{
    if (adcId >= AdcInstance::Invalid) {
        return Status::InvalidAdcInstance;
    }

    stop_adc_sampling(adcId);

    /** @note adc command must be set properly through MCUXpresso Config Tools */

    // set target command to trigger
    sys::adc::ADC::set_adc_trigger(static_cast<uint32_t>(adcId),
                                   static_cast<uint32_t>(AdcCmdTriggerSrc),
                                   static_cast<uint32_t>(AdcCmdScanAllSensors));

    start_adc_sampling(adcId);

    return Status::Ok;
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
    update_gpio_alert(sensorId, State::Nominal);

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

    auto& _sensor = sensor.at(sensorId);
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

    update_gpio_alert(sensorId, _sensor.state);
    return Status::Ok;
}

void LeakDetect::update_gpio_alert(uint8_t sensorId, State state)
{
    // safe to cast State to VrGpioState as we have static_assert in common.h
    update_virtual_gpio(sensorId, static_cast<VrGpioState>(state));

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

void LeakDetect::update_virtual_gpio(uint8_t sensorId, VrGpioState state)
{
    // update internal virtual gpio state
    vrGpioState = state;

    // update iox virtual gpio values
    auto&                                     _sensor    = sensor.at(sensorId);
    const std::array<uint8_t, VirtualGpioNum> ioxPinVals = {
        static_cast<uint8_t>((static_cast<uint8_t>(state) >> 0) & 0x01),
        static_cast<uint8_t>((static_cast<uint8_t>(state) >> 1) & 0x01)};
    nv::iox::Task::send_vrgpio_request(
        _sensor.ioxAddr, nv::iox::Operation::Write, _sensor.ioxPin, ioxPinVals);
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

    if (info.size() != sensor.size()) {
        return Status::InvalidSensorInfoSize;
    }

    for (size_t i = 0; i < sensor.size(); ++i) {
        const auto& adcId = sensor.at(i).adcId;
        if (is_adc_error(adcId)) {
            status = adcerr.at(static_cast<uint32_t>(adcId));
            break;
        }

        // stop adc first as we are in scanning mode
        stop_adc_sampling(adcId);

        // disable interrupt as it is only for scanning mode
        sys::adc::ADC::disable_adc_lpadc_interrupt(static_cast<uint32_t>(adcId),
                                                   Fifo0WatermarkInterruptEnable);

        // start adc one-shot sampling
        status = start_adc_oneshot(i, sensor.at(i).reading);
        if (status != Status::Ok) {
            break;
        }

        info[i] = sensor.at(i);

        dbginfo(AdcMode::OneShot,
                i,
                sensor.at(i).reading,
                sensor.at(i).minLeak,
                sensor.at(i).maxLeak);
    }

    // if no error, re-start adc scanning
    if (status == Status::Ok) {
        if constexpr (SensorOnAdc0) {
            sys::adc::ADC::enable_adc_lpadc_interrupt(static_cast<uint32_t>(AdcInstance::_0),
                                                      Fifo0WatermarkInterruptEnable);
            start_adc_scanning(AdcInstance::_0);
        }
        if constexpr (SensorOnAdc1) {
            sys::adc::ADC::enable_adc_lpadc_interrupt(static_cast<uint32_t>(AdcInstance::_1),
                                                      Fifo0WatermarkInterruptEnable);
            start_adc_scanning(AdcInstance::_1);
        }
    }

    return status;
}

Status LeakDetect::get_thresholds(uint8_t sensorIdx, ThresholdConfig& config)
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

Status LeakDetect::set_thresholds(uint8_t sensorIdx, const ThresholdConfig& config)
{
    // coverity[unsigned_compare] - LeakDetectSensorNum is not 0 once compiled
    if (sensorIdx >= LeakDetectSensorNum) {
        return Status::InvalidSensorId;
    }

    /**
     * simple check if the thresholds are valid
     */
    if (!((config.minLeak < config.maxLeak) && (config.maxLeak < config.maxNormal)
          && (config.maxNormal < to_adc_value(MaxVol)))) {
        return Status::InvalidThreshold;
    }

    if (config.maxNormal >= to_adc_value(MaxVol)) {
        return Status::InvalidThreshold;
    }

    auto& _sensor = sensor.at(sensorIdx);

    /**
     * stop adc first as we need to update threshold to adc command
     */
    stop_adc_sampling(_sensor.adcId);

    /**
     * update threshold to sensor struct
     */
    _sensor.minLeak   = (static_cast<uint32_t>(config.minLeak) * AdcFullScale) / AdcVolVref;
    _sensor.maxLeak   = (static_cast<uint32_t>(config.maxLeak) * AdcFullScale) / AdcVolVref;
    _sensor.maxNormal = (static_cast<uint32_t>(config.maxNormal) * AdcFullScale) / AdcVolVref;

    /**
     * update threshold to adc command
     *
     * @note adc was just stopped, but we don't care about the current sensor state
     *       we always update the normal low and high thresholds and set adc state to normal.
     *
     *       If everything is ok, then sensor state will be stayed as normal.
     *       If not, then sensor state will be updated to the corresponding state
     *       as interrupt will be triggered and sensor state will be updated accordingly.
     */
    _sensor.state = State::Nominal;
    update_adccmd_threshold(sensorIdx, _sensor.maxLeak, _sensor.maxNormal);

    /**
     * scanning mode is the default mode
     * so we need to set the trigger to scanning mode after updating threshold
     */
    sys::adc::ADC::set_adc_trigger(static_cast<uint32_t>(_sensor.adcId),
                                   static_cast<uint32_t>(_sensor.cmdTriggerSrc),
                                   static_cast<uint32_t>(_sensor.cmdScanning));

    /**
     * re-start adc sampling
     */
    start_adc_sampling(_sensor.adcId);

    return Status::Ok;
}

void LeakDetect::get_sensor_state_threshold(uint8_t sensorId, uint16_t& thLow, uint16_t& thHigh)
{
    /** sensor id is guaranteed to be valid */

    const auto& _sensor = sensor.at(sensorId);
    switch (_sensor.state) {
        case State::Short:
            thLow  = to_adc_value(MinVol);
            thHigh = _sensor.minLeak;
            break;
        case State::Leak:
            thLow  = _sensor.minLeak;
            thHigh = _sensor.maxLeak;
            break;
        case State::Nominal:
            thLow  = _sensor.maxLeak;
            thHigh = _sensor.maxNormal;
            break;
        case State::Open:
            thLow  = _sensor.maxNormal;
            thHigh = to_adc_value(MaxVol);
            break;
        default:
            thLow  = to_adc_value(MinVol);
            thHigh = to_adc_value(MaxVol);
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
    };
    sys::adc::ADC::set_adc_command(static_cast<uint32_t>(_sensor.adcId),
                                   static_cast<uint32_t>(_sensor.cmdScanning),
                                   newcmd);
}

const char* LeakDetect::to_float_string(uint16_t value)
{
    constexpr auto OneThousand = 1000;
    constexpr auto OneHundred  = 100;
    constexpr auto Ten         = 10;

    NV_SHARED_DATA static std::array<char, 6> strs = {'0', '.', '0', '0', '0', '\0'};
    // coverity[cert_str34_c_violation] -- safe: debug info only
    strs.at(0) = static_cast<char>(static_cast<char>(value / OneThousand) + '0');
    // coverity[cert_str34_c_violation] -- safe: debug info only
    strs.at(2) = static_cast<char>(static_cast<char>(value % OneThousand / OneHundred) + '0');
    // coverity[cert_str34_c_violation] -- safe: debug info only
    strs.at(3) = static_cast<char>(static_cast<char>(value % OneHundred / Ten) + '0');
    // coverity[cert_str34_c_violation] -- safe: debug info only
    strs.at(4) = static_cast<char>(static_cast<char>(value % Ten) + '0');
    return strs.data();
}

void LeakDetect::dbginfo(
    AdcMode triggerMode, uint8_t sensorId, Reading reading, Threshold thLow, Threshold thHigh)
{
    if constexpr (!EnableDbgInfo) {
        return;
    }

    nv::info("-- Leak Detect Dbg Info --\r\n");

    switch (triggerMode) {
        case AdcMode::OneShot: nv::info("sensorId=%d TriggerMode=OneShot\r\n", sensorId); break;
        case AdcMode::Scanning:
            nv::info("sensorId=%d TriggerMode=Scanning\r\n", sensorId);
            break;
        case AdcMode::Invalid: nv::info("sensorId=%d TriggerMode=Unknown\r\n", sensorId); break;
    }

    switch (sensor.at(sensorId).state) {
        case State::Short  : nv::info("sensorId=%d State=FaultShort\r\n", sensorId); break;
        case State::Leak   : nv::info("sensorId=%d State=LeakDetected\r\n", sensorId); break;
        case State::Nominal: nv::info("sensorId=%d State=Normal\r\n", sensorId); break;
        case State::Open   : nv::info("sensorId=%d State=FaultOpen\r\n", sensorId); break;
        default            : nv::info("sensorId=%d State=Unknown\r\n", sensorId); return;
    }

    reading = static_cast<uint32_t>(reading * AdcVolVref) / AdcFullScale;
    thLow   = static_cast<uint32_t>(thLow * AdcVolVref) / AdcFullScale;
    thHigh  = static_cast<uint32_t>(thHigh * AdcVolVref) / AdcFullScale;

    nv::info("sensorId=%d Reading=%s\r\n", sensorId, to_float_string(reading));

    if (triggerMode == AdcMode::Scanning) {
        nv::info("sensorId=%d NewThresholdLow=%s\r\n", sensorId, to_float_string(thLow));
        nv::info("sensorId=%d NewThresholdHigh=%s\r\n", sensorId, to_float_string(thHigh));
    }
}

void LeakDetect::leak_detect_adc_isr(AdcInstance instance)
{
    uint8_t    sensorId = 0;
    Threshold  thLow = 0, thHigh = 0;
    const auto adcId = static_cast<uint32_t>(instance);

    /**
     * once interrupt is triggered, always reset flags and disable interrupt first
     */
    auto flags = sys::adc::ADC::get_status_flags(static_cast<uint32_t>(adcId));
    sys::adc::ADC::clear_status_flags(static_cast<uint32_t>(adcId), flags);

    /**
     * read the result from fifo0 as we need to disable adc next
     */
    sys::adc::ADC::AdcConvResult result{};
    if (sys::adc::ADC::get_adc_reading(
            static_cast<uint32_t>(adcId), result, static_cast<uint32_t>(AdcDataResultFifo::_0))
        != true) {
        nv::logger::error(nv::logger::Event::LeakDetectIsrNoValidReading,
                          nv::logger::data_from_u32(static_cast<uint32_t>(adcId)));
        adcerr.at(static_cast<uint32_t>(adcId)) = Status::AdcIsrNoValidReading;
        return;
    }

    /**
     * stop adc and reset fifo0
     *
     * @note if trigger mode is one-shot, then adc is already stopped
     *       if trigger mode is scanning, then that means we need to update new threshold
     *       thus, we need to stop adc and reset fifo0 in both cases
     */
    stop_adc_sampling(instance);

    /**
     * identify sensor id and trigger mode from command id
     *
     * @note we have very limited number of sensors, so just loop through
     */
    // coverity[unsigned_compare] - LeakDetectSensorNum is not 0 once compiled
    for (; sensorId < LeakDetectSensorNum; sensorId++) {
        // coverity[dead_error_line] - LeakDetectSensorNum is not 0 once compiled
        if (static_cast<uint32_t>(sensor.at(sensorId).cmdScanning) == result.commandIdSource) {
            break;
        }
    }

    if (sensorId == LeakDetectSensorNum) {
        nv::logger::error(nv::logger::Event::LeakDetectIsrNoValidSensorId,
                          nv::logger::data_from_u32(sensorId));
        adcerr.at(static_cast<uint32_t>(adcId)) = Status::AdcIsrNoValidSensorId;
        return;
    }

    /* update sensor state due to voltage out of range */
    // coverity[dead_error_begin] - LeakDetectSensorNum is not 0 once compiled
    update_sensor_state(sensorId, static_cast<Reading>(result.convValue));

    /* get new threshold based on new state */
    get_sensor_state_threshold(sensorId, thLow, thHigh);

    /* update new threshold to sensor's adc command */
    update_adccmd_threshold(sensorId, thLow, thHigh);

    dbginfo(AdcMode::Scanning, sensorId, result.convValue, thLow, thHigh);

    /**
     * restart adc scanning
     *
     * @note adc should always be in scanning mode
     *       it can only be stopped temporarily
     *       if user queries sensor reading
     */
    start_adc_scanning(instance);
}

}  // namespace nv::leak_detect

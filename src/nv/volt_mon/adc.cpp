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

#include "nv/volt_mon/adc.h"
#include "nv/ipc/task.h"
#include "nv/nv.h"

namespace nv::volt_mon {

using namespace nv::ipc::voltage_monitor_config;

bool           Adc::inited = false;
NV_SHARED_DATA std::atomic<bool> Adc::is_oneshot_converting{false};
NV_SHARED_DATA std::array<Status, static_cast<uint32_t>(AdcInstance::Total)> Adc::adcerr = {
    Status::Ok, Status::Ok};

void Adc::init()
{
    if (inited) {
        return;
    }

    sys::adc::ADC::enable_vref();

    if constexpr (SensorOnAdc0) {
        sys::adc::ADC::enable_adc_nvic_interrupt(static_cast<uint32_t>(AdcInstance::_0));
    }
    if constexpr (SensorOnAdc1) {
        sys::adc::ADC::enable_adc_nvic_interrupt(static_cast<uint32_t>(AdcInstance::_1));
    }

    if constexpr (SensorOnAdc0) {
        start_scanning(AdcInstance::_0);
    }
    if constexpr (SensorOnAdc1) {
        start_scanning(AdcInstance::_1);
    }

    inited = true;
}

/**
 * @brief Start ADC sampling
 * @param adcId ADC ID
 *
 * @note make sure to set adc command and trigger properly
 *       before calling this function
 */
void Adc::start_sampling(AdcInstance adcId)
{
    sys::adc::ADC::enable_adc(static_cast<uint32_t>(adcId), true);
    sys::adc::ADC::trigger_read(static_cast<uint32_t>(adcId),
                                1 << static_cast<uint32_t>(AdcCmdTriggerSrc));
}

/**
 * @brief Stop ADC sampling
 * @param adcId ADC ID
 *
 * @note stop adc and reset fifo
 */
void Adc::stop_sampling(AdcInstance adcId)
{
    sys::adc::ADC::enable_adc(static_cast<uint32_t>(adcId), false);
    sys::adc::ADC::reset_fifo(static_cast<uint32_t>(adcId),
                              static_cast<uint32_t>(AdcDataResultFifo::_0));
}

Status Adc::start_oneshot(VoltMon& sensor)
{
    bool expected = false;
    if (!is_oneshot_converting.compare_exchange_strong(expected, true)) {
        return Status::AdcOneShotConversionOnGoing;
    }

    auto status = Status::Ok;

    const auto& adcId         = sensor.adcId;
    const auto& cmdOneShot    = sensor.cmdOneShot;
    const auto& cmdTriggerSrc = sensor.cmdTriggerSrc;

    // stop adc first as we are in scanning mode
    stop_sampling(adcId);

    // disable interrupt as it is only for scanning mode
    sys::adc::ADC::disable_adc_lpadc_interrupt(static_cast<uint32_t>(adcId),
                                               Fifo0WatermarkInterruptEnable);

    // set target command to trigger
    sys::adc::ADC::set_adc_trigger(static_cast<uint32_t>(adcId),
                                   static_cast<uint32_t>(cmdTriggerSrc),
                                   static_cast<uint32_t>(cmdOneShot));

    start_sampling(adcId);

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
        stop_sampling(adcId);
        status = Status::AdcOneShotConvTimeout;
    }
    sensor.reading = result.convValue;

    // reach here means conversion is done and adc is stopped

    is_oneshot_converting.store(false);
    return status;
}

Status Adc::start_scanning(AdcInstance adcId)
{
    if (adcId >= AdcInstance::Invalid) {
        return Status::InvalidAdcInstance;
    }

    // Skip if this ADC has no sensors requiring scanning mode (e.g., only MCU temp configured)
    if (!((adcId == AdcInstance::_0 && SensorOnAdc0)
          || (adcId == AdcInstance::_1 && SensorOnAdc1))) {
        return Status::Ok;
    }

    stop_sampling(adcId);

    // Enable FIFO 0 watermark interrupt for the specified ADC
    // FIFO 1 used by MCU internal temp (polling mode, no interrupt)
    sys::adc::ADC::enable_adc_lpadc_interrupt(static_cast<uint32_t>(adcId),
                                              Fifo0WatermarkInterruptEnable);

    // set target command to trigger
    sys::adc::ADC::set_adc_trigger(static_cast<uint32_t>(adcId),
                                   static_cast<uint32_t>(AdcCmdTriggerSrc),
                                   static_cast<uint32_t>(AdcCmdScanAllSensors));

    start_sampling(adcId);

    return Status::Ok;
}

const char* Adc::to_float_string(uint16_t value)
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

void Adc::dbginfo_impl(AdcMode   triggerMode,
                       Sensor    sensorType,
                       SensorId  id,
                       State     state,
                       Reading   reading,
                       Threshold thLow,
                       Threshold thHigh)
{
    if constexpr (!EnableDbgInfo) {
        return;
    }

    nv::info("\r\n-- Voltage Monitor Dbg Info --\r\n");
    nv::info("Sensor=%s\r\n",
             (sensorType == Sensor::LeakDetect   ? "LeakDetect"
              : sensorType == Sensor::BusBarTemp ? "BusbarTemp"
              : sensorType == Sensor::PgoodVolt  ? "PgoodVolt"
                                                 : "Unknown"));
    nv::info("SensorId=%d\r\n", id);

    switch (state) {
        case State::Short    : nv::info("State=FaultShort\r\n"); break;
        case State::Leak     : nv::info("State=LeakDetected\r\n"); break;
        case State::Nominal  : nv::info("State=Normal\r\n"); break;
        case State::Open     : nv::info("State=FaultOpen\r\n"); break;
        case State::LowTemp  : nv::info("State=LowTemp\r\n"); break;
        case State::HighTemp : nv::info("State=HighTemp\r\n"); break;
        case State::PgoodLow : nv::info("State=PgoodLow\r\n"); break;
        case State::PgoodHigh: nv::info("State=PgoodHigh\r\n"); break;
        default              : nv::info("State=Unknown\r\n"); return;
    }

    reading = static_cast<uint32_t>(reading * AdcVolVref) / AdcFullScale;
    thLow   = static_cast<uint32_t>(thLow * AdcVolVref) / AdcFullScale;
    thHigh  = static_cast<uint32_t>(thHigh * AdcVolVref) / AdcFullScale;

    switch (triggerMode) {
        case AdcMode::OneShot : nv::info("TriggerMode=OneShot\r\n"); break;
        case AdcMode::Scanning: nv::info("TriggerMode=Scanning\r\n"); break;
        case AdcMode::Invalid : nv::info("TriggerMode=Unknown\r\n"); break;
    }

    nv::info("Reading=%s\r\n", to_float_string(reading));

    if (triggerMode == AdcMode::Scanning) {
        nv::info("NewThresholdLow=%s\r\n", to_float_string(thLow));
        nv::info("NewThresholdHigh=%s\r\n", to_float_string(thHigh));
    }
}

bool Adc::adc_isr_get_conv_result(AdcInstance adcId, sys::adc::ADC::AdcConvResult& result)
{
    /**
     * once interrupt is triggered, always reset flags and disable interrupt first
     */
    auto flags = sys::adc::ADC::get_status_flags(static_cast<uint32_t>(adcId));
    sys::adc::ADC::clear_status_flags(static_cast<uint32_t>(adcId), flags);

    /**
     * read the result from fifo0 as we need to disable adc next
     */
    if (sys::adc::ADC::get_adc_reading(
            static_cast<uint32_t>(adcId), result, static_cast<uint32_t>(AdcDataResultFifo::_0))
        != true) {
        nv::error("ADC%d_IRQHandler: isr found no valid reading\r\n",
                  static_cast<uint32_t>(adcId));
        volt_mon::Adc::adcerr.at(static_cast<uint32_t>(adcId)) = Status::AdcIsrNoValidReading;
        return false;
    }

    return true;
}

}  // namespace nv::volt_mon
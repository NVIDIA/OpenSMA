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
#include <cstring>
#include "nv/telemetry/cache.h"
#include "nv/nv.h"
#include "nv/gpio/driver.h"
#include NV_IPC_CONFIG_H

using namespace nv::telemetry;

// NOLINTNEXTLINE(*-non-const-global-variables)
NV_SHARED_BSS Cache cache;

Cache& Cache::inst()
{
    return cache;
}

void Cache::init()
{
    _table.fill(InvalidData);
    _alert_table.fill(false);
}

Cache::Table Cache::get_table()
{
    refresh();
    return _table;
}

void Cache::set_cache(TelemId item, Value value)
{
    if (item == TelemId::MaxItem) {
        return;
    }

    const int cache_index = get<1>(TelemIndexMapList.at(item));
    if (cache_index == -1) {
        return;
    }
    // Check for potential overflow before multiplication
    if (cache_index >= 0 && static_cast<size_t>(cache_index) <= (SIZE_MAX / sizeof(value))) {
        const size_t index = static_cast<size_t>(cache_index) * sizeof(value);
        std::memcpy(&_table.at(index), &value, sizeof(value));
    }
}

Cache::Value Cache::get_cache(TelemId item)
{
    if (item == TelemId::MaxItem) {
        return InvalidItem;
    }

    const int cache_index = get<1>(TelemIndexMapList.at(item));
    if (cache_index == -1) {
        return InvalidItem;
    }

    Value value = InvalidItem;
    // Check for potential overflow before multiplication
    if (cache_index >= 0 && static_cast<size_t>(cache_index) <= (SIZE_MAX / sizeof(value))) {
        const size_t index = static_cast<size_t>(cache_index) * sizeof(value);
        std::memcpy(&value, &_table.at(index), sizeof(value));
    }
    return value;
}

void Cache::set_error(ErrorItem item)
{
    if (item == ErrorItem::MaxAlertItem) {
        return;
    }
    _alert_table.at(static_cast<uint8_t>(item)) = true;
}

void Cache::clear_error(ErrorItem item)
{
    if (item == ErrorItem::MaxAlertItem) {
        return;
    }
    _alert_table.at(static_cast<uint8_t>(item)) = false;
}

void Cache::refresh()
{
    // ModulePower
    if (get<1>(TelemIndexMapList.at(TelemId::ModulePower)) != -1) {
        Value gpu1_Power = get_cache(TelemId::Gpu1Power);
        Value gpu2_Power = get_cache(TelemId::Gpu2Power);
        if (gpu1_Power == InvalidItem && gpu2_Power == InvalidItem) {
            set_cache(TelemId::ModulePower, InvalidItem);
        }
        else {
            gpu1_Power               = gpu1_Power == InvalidItem ? 0 : gpu1_Power;
            gpu2_Power               = gpu2_Power == InvalidItem ? 0 : gpu2_Power;
            const Value module_power = gpu1_Power + gpu2_Power;
            set_cache(TelemId::ModulePower, module_power);
        }
    }

    // MaxModuleTemp
    if (get<1>(TelemIndexMapList.at(TelemId::MaxModuleTemp)) != -1) {
        Value module_temp1 = get_cache(TelemId::ModuleTemp1);
        Value module_temp2 = get_cache(TelemId::ModuleTemp2);
        if (module_temp1 == InvalidItem && module_temp2 == InvalidItem) {
            set_cache(TelemId::MaxModuleTemp, InvalidItem);
        }
        else {
            module_temp1                = module_temp1 == InvalidItem ? 0 : module_temp1;
            module_temp2                = module_temp2 == InvalidItem ? 0 : module_temp2;
            const Value max_module_temp = module_temp1 > module_temp2 ? module_temp1
                                                                      : module_temp2;
            set_cache(TelemId::MaxModuleTemp, max_module_temp);
        }
    }

    // GPIO
    if (get<1>(TelemIndexMapList.at(TelemId::Gpio)) != -1) {
        Value gpio = InvalidItem;
        // coverity[DEADCODE] the table is not 0 on other projects
        for (auto& [port, pin, bit] : nv::ipc::GpioTelemetryTable) {
            uint8_t value  = 0;
            auto    status = nv::gpio::Driver::read(port, pin, value);
            if (status == nv::gpio::Status::Ok) {
                gpio &= ~(1U << bit);
                gpio |= static_cast<Value>(value) << bit;
            }
        }
        // InterTempWarn bit
        const Value internal_temp = get_cache(TelemId::InternalTemp);
        if (internal_temp != InvalidItem) {
            gpio &= ~(1U << nv::ipc::InternalTempWarnBit);
            gpio |= (internal_temp > TempThreshold ? 0 : 1) << nv::ipc::InternalTempWarnBit;
        }
        // I2cSensorAlert bit
        auto alert  = _alert_table.at(static_cast<uint8_t>(ErrorItem::I2cSensorAlert));
        gpio       &= ~(1U << nv::ipc::I2cSensorAlertBit);
        gpio       |= (alert ? 0 : 1) << nv::ipc::I2cSensorAlertBit;
        set_cache(TelemId::Gpio, gpio);
    }
}

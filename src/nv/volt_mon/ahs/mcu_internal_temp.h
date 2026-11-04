/*
 * SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES.
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
 * @file ahs/mcu_internal_temp.h
 * @brief AHS MCU internal temperature — ISR accumulate / task publish
 *
 * CMD15 rides an existing ADC0 PGOOD chain (Trigger 3: CMD7→CMD8→CMD15) into
 * FIFO1. One Trigger3 completion is a fixed four-entry batch; the IRQ handler
 * validates command IDs in order and passes both VBE samples here together.
 *
 * Callers:
 * - ADC Trig3 ISR → accumulate_pair_from_isr() (integer only, Collecting state)
 * - AHS task → service() before the next soft-trigger (avg + Celsius + cache)
 * - NSM → McuInternalTemp::get_temperature_celsius() (tempCelsius; AbsoluteZero = unread)
 */

#pragma once

#include "nv/volt_mon/mcu_internal_temp.h"

#include <cstdint>

namespace nv::mcu_internal_temp::ahs {

/**
 * @brief ISR path: add one ordered VBE1/VBE8 pair toward the software average.
 *
 * No floating-point work. Ignores pairs while the window state is Ready.
 */
void accumulate_pair_from_isr(uint16_t vbe1, uint16_t vbe8);

/**
 * @brief Task context: if Ready, average VBE sums, publish Celsius, reset, Collecting.
 *
 * Call before the next ADC soft-trigger. Cache stays at AbsoluteZero until the
 * first good averaging window is published.
 */
void service();

}  // namespace nv::mcu_internal_temp::ahs

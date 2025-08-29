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
#include <cstdint>
namespace sys::gpio {

constexpr uint32_t NumInstance         = 6;
constexpr uint32_t PinsPerInstance     = 32;
constexpr uint32_t PinsNumber          = (PinsPerInstance * NumInstance);
constexpr uint32_t PinsToInstanceShift = 5;
constexpr uint32_t InterruptPerPort    = 2;
constexpr uint32_t Gpio0ValidMask      = 0xFFFFFFF8;
constexpr uint32_t Gpio1ValidMask      = 0xC0FFFFFF;
constexpr uint32_t Gpio2ValidMask      = 0x00000FFF;
constexpr uint32_t Gpio3ValidMask      = 0x00FFFFFF;
constexpr uint32_t Gpio4ValidMask      = 0x00FFF0FF;
constexpr uint32_t Gpio5ValidMask      = 0x000003FF;

#define SYS_GPIO_GA_GPIO9_IROT_ERROR_N_PIN           (12)
#define SYS_GPIO_GA_GPIO10_IROT_AP_BOOT_COMPLETE_PIN (13)
#define SYS_GPIO_THERM_OVERT_PIN                     (15)
#define SYS_GPIO_PS_BOARD_PGOOD_PIN                  (20)

#define SYS_GPIO_IO_EXP_ADDR_ALT_PIN (28)

}  // namespace sys::gpio

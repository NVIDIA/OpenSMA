/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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

#include <cstddef>
#include <cstdint>
#include <span>

#include NV_IPC_CONFIG_H

/**
 * Runtime accessor for the active product's GPIO table.
 *
 * Single-product images use the weak default (in gpio/driver.cpp), which returns
 * the compile-time `nv::ipc::GpioSetup` table. Multi-product images provide a
 * strong override (product_config.cpp) that returns the strap-selected product's
 * table; they may leave `GpioSetup` as an empty stub so the weak default still
 * compiles.
 *
 * Shared nv/ code should use gpio_count() / gpio_at() so single-product images
 * keep a compile-time GpioNum / GpioSetup walk (if constexpr). Multi-product
 * images (EnableMultiProduct) still go through this span so a smaller product
 * does not read past the installed table.
 */
namespace nv::ipc {

std::span<const Gpios> project_gpio_setup();

[[nodiscard]] inline uint16_t gpio_count()
{
    if constexpr (EnableMultiProduct) {
        return static_cast<uint16_t>(project_gpio_setup().size());
    }
    else {
        return GpioNum;
    }
}

[[nodiscard]] inline const Gpios& gpio_at(std::size_t index)
{
    if constexpr (EnableMultiProduct) {
        return project_gpio_setup()[index];
    }
    else {
        return GpioSetup.at(index);
    }
}

}  // namespace nv::ipc

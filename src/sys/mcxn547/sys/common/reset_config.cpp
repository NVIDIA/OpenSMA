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

#include "reset_config.h"

// Include necessary headers for hardware registers and interrupt handling
#include <cstdint>
#include <FreeRTOS.h>
#include <fsl_clock.h>
#include <board/clock_config.h>
#include <board/peripherals.h>
#include <board/pin_mux.h>

// Add device register headers to define ITRC0, SPC0, GDET0, GDET1, SCB, NVIC, etc.
#include "fsl_device_registers.h"

namespace sys::common {

void PreventReset()
{
    // Prevent Chip reset by ITRC
    ITRC0->OUT_SEL[4][0] = (ITRC0->OUT_SEL[4][0] & ~ITRC_OUT_SEL_IN7_SELn_MASK)
                         | (ITRC_OUT_SEL_IN7_SELn(0x2));
    ITRC0->OUT_SEL[4][1] = (ITRC0->OUT_SEL[4][1] & ~ITRC_OUT_SEL_IN7_SELn_MASK)
                         | (ITRC_OUT_SEL_IN7_SELn(0x2));
    SCB->SHCSR |= (1UL << 19U);  // Enable secure fault
    NVIC_EnableIRQ(SEC_VIO_IRQn);
}

}  // namespace sys::common

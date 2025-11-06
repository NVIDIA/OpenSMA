/*
** ###################################################################
**     Processors:          MCXN547VDF_cm33_core1
**                          MCXN547VKL_cm33_core1
**                          MCXN547VNL_cm33_core1
**                          MCXN547VPB_cm33_core1
**
**     Compilers:           GNU C Compiler
**                          IAR ANSI C/C++ Compiler for ARM
**                          Keil ARM C/C++ Compiler
**                          MCUXpresso Compiler
**
**     Reference manual:    MCXNx4x Reference Manual
**     Version:             rev. 2.0, 2023-02-01
**     Build:               b250206
**
**     Abstract:
**         Provides a minimal system configuration function for Core1.
**         Core1 runs in non-secure mode and should not access secure peripherals.
**         All system-level initialization is handled by Core0 (secure world).
**
**     Copyright 2016 Freescale Semiconductor, Inc.
**     Copyright 2016-2025 NXP
**     SPDX-License-Identifier: BSD-3-Clause
**
**     http:                 www.nxp.com
**     mail:                 support@nxp.com
**
** ###################################################################
*/

/*!
 * @file MCXN547_cm33_core1
 * @version 2.0
 * @date 2023-02-01
 * @brief Minimal device specific configuration file for MCXN547_cm33_core1
 *        (implementation file)
 *
 * Provides a minimal system configuration function for Core1 that only
 * handles Core1-specific initialization. All secure peripheral initialization
 * is handled by Core0.
 */

#include <stdint.h>
#include "fsl_device_registers.h"

/* Do NOT include fsl_clock.h - Core1 should not access clock registers */

/* ----------------------------------------------------------------------------
   -- Core clock
   ---------------------------------------------------------------------------- */

uint32_t SystemCoreClock = 150000000U;  // Core1 runs at 150MHz

/* ----------------------------------------------------------------------------
   -- SystemInit()
   ---------------------------------------------------------------------------- */

__attribute__((weak)) void SystemInit(void)
{
    // Done in core0
}

/* ----------------------------------------------------------------------------
   -- SystemCoreClockUpdate()
   ---------------------------------------------------------------------------- */

void SystemCoreClockUpdate(void)
{
    /* Core1 uses hardcoded clock frequency - Core0 handles all clock configuration */
    /* DO NOT call CLOCK_GetCoreSysClkFreq() - that accesses SYSCON registers! */
    SystemCoreClock = 150000000U;  // 150MHz - matches what Core0 configured
}

/* ----------------------------------------------------------------------------
   -- SystemInitHook()
   ---------------------------------------------------------------------------- */

__attribute__((weak)) void SystemInitHook(void)
{
    /* Void implementation of the weak function. */
}
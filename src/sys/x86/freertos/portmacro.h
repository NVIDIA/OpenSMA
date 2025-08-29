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
// NOLINTBEGIN
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <limits.h>

#define portCHAR              char  // NOLINT
#define portFLOAT             float
#define portDOUBLE            double
#define portLONG              long
#define portSHORT             short
#define portSTACK_TYPE        unsigned long
#define portBASE_TYPE         long
#define portPOINTER_SIZE_TYPE intptr_t
typedef portSTACK_TYPE StackType_t;
typedef long           BaseType_t;
typedef unsigned long  UBaseType_t;
typedef unsigned long  TickType_t;
#define portMAX_DELAY                   ULONG_MAX
#define portTICK_TYPE_IS_ATOMIC         1
#define portSTACK_GROWTH                (-1)
#define portHAS_STACK_OVERFLOW_CHECKING (1)
#define portTICK_PERIOD_MS              ((TickType_t)1000 / configTICK_RATE_HZ)
#define portTICK_RATE_MICROSECONDS      ((portTickType)1000000 / configTICK_RATE_HZ)
#define portBYTE_ALIGNMENT              8

extern void vPortYield(void);
#define portYIELD() vPortYield()
#define portEND_SWITCHING_ISR(xSwitchRequired)                                                 \
    if (xSwitchRequired != pdFALSE) vPortYield()
#define portYIELD_FROM_ISR(x) portEND_SWITCHING_ISR(x)

extern void vPortDisableInterrupts(void);
extern void vPortEnableInterrupts(void);
#define portSET_INTERRUPT_MASK()   (vPortDisableInterrupts())
#define portCLEAR_INTERRUPT_MASK() (vPortEnableInterrupts())

extern portBASE_TYPE xPortSetInterruptMask(void);
extern void          vPortClearInterruptMask(portBASE_TYPE xMask);
extern void          vPortEnterCritical(void);
extern void          vPortExitCritical(void);

#define portSET_INTERRUPT_MASK_FROM_ISR()    xPortSetInterruptMask()
#define portCLEAR_INTERRUPT_MASK_FROM_ISR(x) vPortClearInterruptMask(x)
#define portDISABLE_INTERRUPTS()             portSET_INTERRUPT_MASK()
#define portENABLE_INTERRUPTS()              portCLEAR_INTERRUPT_MASK()
#define portENTER_CRITICAL()                 vPortEnterCritical()
#define portEXIT_CRITICAL()                  vPortExitCritical()

extern void vPortThreadDying(void* pxTaskToDelete, volatile BaseType_t* pxPendYield);
extern void vPortCancelThread(void* pxTaskToDelete);
#define portPRE_TASK_DELETE_HOOK(pvTaskToDelete, pxPendYield)                                  \
    vPortThreadDying((pvTaskToDelete), (pxPendYield))
#define portCLEAN_UP_TCB(pxTCB) vPortCancelThread(pxTCB)

#define portTASK_FUNCTION_PROTO(vFunction, pvParameters) void vFunction(void* pvParameters)
#define portTASK_FUNCTION(vFunction, pvParameters)       void vFunction(void* pvParameters)
#define portMEMORY_BARRIER()                             __asm volatile("" ::: "memory")

extern unsigned long ulPortGetRunTime();
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() /* no-op */
#define portGET_RUN_TIME_COUNTER_VALUE()         ulPortGetRunTime()

#ifdef __cplusplus
}
#endif
// NOLINTEND

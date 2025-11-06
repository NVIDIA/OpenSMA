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

#define __PTHREAD_STACK_MIN                     16384
#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     1
#define configUSE_TICK_HOOK                     1
#define configUSE_DAEMON_TASK_STARTUP_HOOK      1
#define configTICK_RATE_HZ                      1000
#define configMINIMAL_STACK_SIZE                ((unsigned short)__PTHREAD_STACK_MIN)
#define configTOTAL_HEAP_SIZE                   ((size_t)(65 * 1024))
#define configMAX_TASK_NAME_LEN                 (12)
#define configUSE_TRACE_FACILITY                1
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configQUEUE_REGISTRY_SIZE               20
#define configUSE_APPLICATION_TASK_TAG          1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_QUEUE_SETS                    1
#define configUSE_TASK_NOTIFICATIONS            1
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configUSE_ALTERNATIVE_API               0
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_16_BIT_TICKS                  0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0

#define configUSE_TIMERS                     1
#define configMAX_PRIORITIES                 16
#define configTIMER_TASK_PRIORITY            (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH             20
#define configTIMER_TASK_STACK_DEPTH         (configMINIMAL_STACK_SIZE * 2)
#define configGENERATE_RUN_TIME_STATS        0
#define configUSE_CO_ROUTINES                0
#define configMAX_CO_ROUTINE_PRIORITIES      (2)
#define configUSE_STATS_FORMATTING_FUNCTIONS 0
#define configSTACK_DEPTH_TYPE               uint32_t

#define INCLUDE_vTaskPrioritySet               1
#define INCLUDE_uxTaskPriorityGet              1
#define INCLUDE_vTaskDelete                    0
#define INCLUDE_vTaskCleanUpResources          0
#define INCLUDE_vTaskSuspend                   1
#define INCLUDE_vTaskDelayUntil                1
#define INCLUDE_vTaskDelay                     1
#define INCLUDE_uxTaskGetStackHighWaterMark    1
#define INCLUDE_uxTaskGetStackHighWaterMark2   1
#define INCLUDE_xTaskGetSchedulerState         1
#define INCLUDE_xTimerGetTimerDaemonTaskHandle 1
#define INCLUDE_xTaskGetIdleTaskHandle         1
#define INCLUDE_xTaskGetHandle                 1
#define INCLUDE_eTaskGetState                  1
#define INCLUDE_xSemaphoreGetMutexHolder       1
#define INCLUDE_xTimerPendFunctionCall         1
#define INCLUDE_xTaskAbortDelay                1

// --------- mpuhacks -----------
// TODO: finish this
#define configENABLE_MPU                                      0
#define portUSING_MPU_WRAPPERS                                0
#define configTOTAL_MPU_REGIONS                               8
#define portMPU_REGION_READ_ONLY                              0b00001010ul
#define portMPU_REGION_READ_WRITE                             0b00010000ul
#define portMPU_REGION_PRIVILEGED_READ_ONLY                   0b00000011ul
#define portMPU_REGION_PRIVILEGED_READ_WRITE                  0b00000111ul
#define portMPU_REGION_PRIVILEGED_READ_WRITE_UNPRIV_READ_ONLY 0b00001011ul
#define portMPU_REGION_CACHEABLE_BUFFERABLE                   0b00100000ul
#define portMPU_REGION_EXECUTE_NEVER                          0b01000000ul
#define portIS_PRIVILEGED()                                   pdTRUE
#define portRAISE_PRIVILEGE()                                 pdFALSE
#define portRESET_PRIVILEGE()                                 pdFALSE
typedef struct
{
    int dummy;
} xMPU_SETTINGS;

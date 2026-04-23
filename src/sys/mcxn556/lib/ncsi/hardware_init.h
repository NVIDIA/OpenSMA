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

#ifndef NV_ECM_HARDWARE_INIT_H
#define NV_ECM_HARDWARE_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Interrupt Priority Definitions
 ******************************************************************************/

// USB and ENET use same priority to ensure fair scheduling during
#ifndef USB_DEVICE_INTERRUPT_PRIORITY
#define USB_DEVICE_INTERRUPT_PRIORITY (2U)
#endif

#if USB_DEVICE_CONFIG_CDC_ECM
#ifndef ENET_INTERRUPT_PRIORITY
#define ENET_INTERRUPT_PRIORITY (2U)
#endif
#endif  // USB_DEVICE_CONFIG_CDC_ECM

/*******************************************************************************
 * API
 ******************************************************************************/

#if USB_DEVICE_CONFIG_CDC_ECM
/**
 * @brief Initialize ENET hardware for RMII mode
 *
 * This function initializes:
 * - RMII interface pins
 * - ENET clock (50MHz from PLL0)
 *
 * Note: PHY runs in default auto-negotiation mode (no MDIO control).
 *
 * Should be called before ETH_ADAPTER_Init()
 */
void ECM_InitEnetHardware(void);
#endif  // USB_DEVICE_CONFIG_CDC_ECM

/**
 * @brief Initialize USB device clock
 *
 * Configures USB HS/FS clock based on the USB device configuration.
 * Supports EHCI, KHCI, and LPCIP3511HS/FS controllers.
 */
void USB_DeviceClockInit(void);

/**
 * @brief Enable USB device ISR
 *
 * Sets interrupt priority and enables the USB device interrupt
 * based on the configured controller type.
 */
void USB_DeviceIsrEnable(void);

#if USB_DEVICE_CONFIG_USE_TASK
/**
 * @brief USB device task function
 *
 * Should be called from a FreeRTOS task to process USB events.
 *
 * @param deviceHandle USB device handle
 */
void USB_DeviceTaskFn(void* deviceHandle);
#endif

#ifdef __cplusplus
}
#endif

#endif  // NV_ECM_HARDWARE_INIT_H

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

/**
 * @file usb_init.cpp
 * @brief USB Hardware Abstraction Layer - Low-level initialization functions
 *
 * This file contains platform-specific USB hardware initialization code.
 * To port to different hardware platforms, only this file needs modification.
 */

#include "usb.h"
#include "sys/usb/usb_device_config.h"
#include "usb_device_ch9.h"
#include "fsl_device_registers.h"
#include "usb_phy.h"
#include "fsl_clock.h"

#if defined(__cplusplus)
extern "C" {
#endif

// Constants for USB initialization
namespace {
constexpr uint32_t USB_ACTIVE_VDELAY_VALUE = 0x0500U;  ///< Power management delay configuration
constexpr uint32_t USB_TRIM_LOCK_KEY   = 0x5a5a0001U;  ///< Trim lock key for LDO configuration
constexpr uint32_t USB_PHY_PLL_FREQ_HZ = 24000000U;    ///< USB PHY PLL frequency in Hz (24MHz)
}  // namespace

/**
 * @brief Initialize USB device clock and power management
 *
 * This function configures:
 * - Power management (DCDC, CORELDO, SYSLDO)
 * - Clock sources (SOSC, USB HS clock)
 * - USB PHY initialization
 */
void USB_DeviceClockInit(void)
{
#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
    usb_phy_config_struct_t phyConfig = {
        BOARD_USB_PHY_D_CAL,
        BOARD_USB_PHY_TXCAL45DP,
        BOARD_USB_PHY_TXCAL45DM,
    };

    // Power configuration
    SPC0->ACTIVE_VDELAY = USB_ACTIVE_VDELAY_VALUE;

    /* Change the power DCDC to 1.8v (By default, DCDC is 1.8V),
     * CORELDO to 1.1v (By default, CORELDO is 1.0V) */
    SPC0->ACTIVE_CFG &= ~SPC_ACTIVE_CFG_CORELDO_VDD_DS_MASK;
    SPC0->ACTIVE_CFG |= SPC_ACTIVE_CFG_DCDC_VDD_LVL(0x3) | SPC_ACTIVE_CFG_CORELDO_VDD_LVL(0x3)
                      | SPC_ACTIVE_CFG_SYSLDO_VDD_DS_MASK | SPC_ACTIVE_CFG_DCDC_VDD_DS(0x2u);

    /* Wait until power configuration is done */
    // coverity[no_escape]
    while (SPC0->SC & SPC_SC_BUSY_MASK) {
        /* Wait loop */
    }

    // LDO configuration
    if (0u == (SCG0->LDOCSR & SCG_LDOCSR_LDOEN_MASK)) {
        SCG0->TRIM_LOCK  = USB_TRIM_LOCK_KEY;
        SCG0->LDOCSR    |= SCG_LDOCSR_LDOEN_MASK;
        /* Wait for LDO ready */
        // coverity[no_escape]
        while (0U == (SCG0->LDOCSR & SCG_LDOCSR_VOUT_OK_MASK)) {
            /* Wait loop */
        }
    }

    // Enable USB clocks
    SYSCON->AHBCLKCTRLSET[2] |= SYSCON_AHBCLKCTRL2_USB_HS_MASK
                              | SYSCON_AHBCLKCTRL2_USB_HS_PHY_MASK;

    // SOSC configuration (24MHz crystal)
    SCG0->SOSCCFG &= ~(SCG_SOSCCFG_RANGE_MASK | SCG_SOSCCFG_EREFS_MASK);
    /* xtal = 20 ~ 30MHz */
    SCG0->SOSCCFG  = (1U << SCG_SOSCCFG_RANGE_SHIFT) | (1U << SCG_SOSCCFG_EREFS_SHIFT);
    SCG0->SOSCCSR |= SCG_SOSCCSR_SOSCEN_MASK;

    /* Wait for SOSC to be ready */
    // coverity[no_escape]
    while (!(SCG0->SOSCCSR & SCG_SOSCCSR_SOSCVLD_MASK)) {
        /* Wait loop */
    }

    // Clock control
    SYSCON->CLOCK_CTRL |= SYSCON_CLOCK_CTRL_CLKIN_ENA_MASK
                        | SYSCON_CLOCK_CTRL_CLKIN_ENA_FM_USBH_LPT_MASK;

    // Enable USB clocks
    CLOCK_EnableClock(kCLOCK_UsbHs);
    CLOCK_EnableClock(kCLOCK_UsbHsPhy);
    CLOCK_EnableUsbhsPhyPllClock(kCLOCK_Usbphy480M, USB_PHY_PLL_FREQ_HZ);
    CLOCK_EnableUsbhsClock();

    // Initialize USB PHY
    USB_EhciPhyInit(CONTROLLER_ID, BOARD_XTAL0_CLK_HZ, &phyConfig);
#endif
}

/**
 * @brief Enable USB device interrupt
 *
 * This function configures and enables the USB device interrupt
 * with appropriate priority setting.
 */
void USB_DeviceIsrEnable(void)
{
    uint8_t irqNumber = 0;

#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
    const uint8_t usbDeviceEhciIrq[] = USBHS_IRQS;
    // Use index 0 for EHCI controller since USBHS_IRQS array has only one element
    irqNumber = usbDeviceEhciIrq[0];
#endif

    /* Set interrupt priority and enable IRQ */
    NVIC_SetPriority((IRQn_Type)irqNumber, USB_DEVICE_INTERRUPT_PRIORITY);
    EnableIRQ((IRQn_Type)irqNumber);
}

#if defined(__cplusplus)
}
#endif
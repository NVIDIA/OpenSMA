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

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "fsl_enet.h"
#include "fsl_clock.h"
#include "fsl_port.h"
#include "fsl_spc.h"

#if defined(NCSI_USE_PHY_MDIO) && (NCSI_USE_PHY_MDIO > 0)
#include "fsl_phy.h"
#include "fsl_phylan8741.h"
#endif

#include NV_IPC_CONFIG_H

#include "eth_adapter.h"
#include "hardware_init.h"
#include "usb_device_config.h"
#include "usb_device.h"
#include "usb_phy.h"

/*******************************************************************************
 * USB Configuration
 ******************************************************************************/
#ifndef CONTROLLER_ID
#define CONTROLLER_ID kUSB_ControllerEhci0
#endif

// USB_DEVICE_INTERRUPT_PRIORITY is defined in hardware_init.h

#ifndef BOARD_XTAL0_CLK_HZ
#define BOARD_XTAL0_CLK_HZ (24000000U)
#endif

// USB PHY calibration values
#ifndef BOARD_USB_PHY_D_CAL
#define BOARD_USB_PHY_D_CAL (0x04U)
#endif
#ifndef BOARD_USB_PHY_TXCAL45DP
#define BOARD_USB_PHY_TXCAL45DP (0x07U)
#endif
#ifndef BOARD_USB_PHY_TXCAL45DM
#define BOARD_USB_PHY_TXCAL45DM (0x07U)
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/
// ENET_INTERRUPT_PRIORITY is defined in hardware_init.h

namespace {

constexpr uint16_t EnetTxDriveStrength = nv::ncsi::EnableEnetTxHighDrive
                                           ? static_cast<uint16_t>(kPORT_HighDriveStrength)
                                           : static_cast<uint16_t>(kPORT_LowDriveStrength);

}  // namespace

/*******************************************************************************
 * Variables
 ******************************************************************************/
// Board ENET instance
extern "C" {
ENET_Type* BOARD_Enet = ENET0;

// System clock for ENET (used by SDK)
uint32_t BOARD_PhySysClock = 0U;

#if USB_DEVICE_CONFIG_CDC_ECM && defined(NCSI_USE_PHY_MDIO) && (NCSI_USE_PHY_MDIO > 0)
// LAN8741 on FRDM-MCXN947 is hardware-strapped to MDIO address 0. Override
// per-build by passing BOARD_LAN8741_PHY_ADDR=0xNN to make (handled in the
// devkit block of *-mcxn556-core1.mk). There is no runtime MDIO scan, so a
// wrong value here silently breaks PHY communication.
#ifndef BOARD_LAN8741_PHY_ADDR
#define BOARD_LAN8741_PHY_ADDR (0x00U)
#endif

// PHY resource (read/write callbacks filled in by ECM_InitEnetHardware)
static phy_lan8741_resource_t g_phy_resource;

// PHY operations
const phy_operations_t* BOARD_PhyOps = &phylan8741_ops;

// PHY address (see BOARD_LAN8741_PHY_ADDR above - hardcoded, no MDIO scan)
uint8_t BOARD_PhyAddress = BOARD_LAN8741_PHY_ADDR;

// PHY resource handle (passed into PHY driver)
void* BOARD_PhySource = &g_phy_resource;
#endif  // USB_DEVICE_CONFIG_CDC_ECM && NCSI_USE_PHY_MDIO
}

/*******************************************************************************
 * Code
 ******************************************************************************/

#if USB_DEVICE_CONFIG_CDC_ECM

/**
 * @brief Initialize ENET pins for RMII mode
 *
 * Configure GPIO pins for RMII interface. When NCSI_USE_PHY_MDIO is defined
 * (devkit / FRDM-MCXN947 with LAN8741), MDIO pins are also configured so the
 * PHY can be brought up via MDIO. Production boards keep PHY strap-pin config
 * and skip MDIO entirely.
 */
static void ECM_InitEnetPins(void)
{
    /* Enable clock for PORT1 */
    CLOCK_EnableClock(kCLOCK_Port1);

    /* TX pin config. Some boards need high drive for RMII clock/data rise/fall margin. */
    const port_pin_config_t enet_tx_pin_config = {
        .pullSelect          = kPORT_PullDisable,
        .pullValueSelect     = kPORT_LowPullResistor,
        .slewRate            = kPORT_FastSlewRate,
        .passiveFilterEnable = kPORT_PassiveFilterDisable,
        .openDrainEnable     = kPORT_OpenDrainDisable,
        .driveStrength       = EnetTxDriveStrength,
        .mux                 = kPORT_MuxAlt9, /* ENET function */
        .inputBuffer         = kPORT_InputBufferEnable,
        .invertInput         = kPORT_InputNormal,
        .lockRegister        = kPORT_UnlockRegister,
    };

    /* RX pin config - drive strength is unused for input-only pins; left at Low. */
    const port_pin_config_t enet_rx_pin_config = {
        .pullSelect          = kPORT_PullDisable,
        .pullValueSelect     = kPORT_LowPullResistor,
        .slewRate            = kPORT_FastSlewRate,
        .passiveFilterEnable = kPORT_PassiveFilterDisable,
        .openDrainEnable     = kPORT_OpenDrainDisable,
        .driveStrength       = kPORT_LowDriveStrength,
        .mux                 = kPORT_MuxAlt9, /* ENET function */
        .inputBuffer         = kPORT_InputBufferEnable,
        .invertInput         = kPORT_InputNormal,
        .lockRegister        = kPORT_UnlockRegister,
    };

#if defined(NCSI_USE_PHY_MDIO) && (NCSI_USE_PHY_MDIO > 0)
    /* MDIO pins - required for PHY communication on devkit */
    PORT_SetPinConfig(PORT1, 20U, &enet_tx_pin_config); /* ENET0_MDC */
    PORT_SetPinConfig(PORT1, 21U, &enet_tx_pin_config); /* ENET0_MDIO */
#endif

    /* RMII TX pins */
    PORT_SetPinConfig(PORT1, 4U, &enet_tx_pin_config); /* ENET0_TX_CLK */
    PORT_SetPinConfig(PORT1, 5U, &enet_tx_pin_config); /* ENET0_TXEN */
    PORT_SetPinConfig(PORT1, 6U, &enet_tx_pin_config); /* ENET0_TXD0 */
    PORT_SetPinConfig(PORT1, 7U, &enet_tx_pin_config); /* ENET0_TXD1 */

    /* RMII RX pins */
    PORT_SetPinConfig(PORT1, 13U, &enet_rx_pin_config); /* ENET0_RXDV */
    PORT_SetPinConfig(PORT1, 14U, &enet_rx_pin_config); /* ENET0_RXD0 */
    PORT_SetPinConfig(PORT1, 15U, &enet_rx_pin_config); /* ENET0_RXD1 */
}

#if defined(NCSI_USE_PHY_MDIO) && (NCSI_USE_PHY_MDIO > 0)
/**
 * @brief Configure the MDIO/SMI clock divider.
 *
 * ENET clock gating is already done by ECM_InitEnetHardware before this is
 * called, so we only need to size the SMI clock here.
 */
static void MDIO_Init(void)
{
    ENET_SetSMI(BOARD_Enet, BOARD_PhySysClock);
}

// MDIO read/write callbacks consumed by the LAN8741 PHY driver via function
// pointers stashed in g_phy_resource. The PHY driver is C, so we need C
// language linkage to guarantee a compatible calling convention; static
// keeps them out of the global symbol namespace (generic names like
// MDIO_Read collide with NXP SDK examples otherwise).
extern "C" {

static status_t NCSI_MDIO_Write(uint8_t phyAddr, uint8_t regAddr, uint16_t data)
{
    return ENET_MDIOWrite(BOARD_Enet, phyAddr, regAddr, data);
}

static status_t NCSI_MDIO_Read(uint8_t phyAddr, uint8_t regAddr, uint16_t* pData)
{
    return ENET_MDIORead(BOARD_Enet, phyAddr, regAddr, pData);
}

}  // extern "C"
#endif  // NCSI_USE_PHY_MDIO

/**
 * @brief Initialize ENET hardware
 *
 * Production boards: PHY runs in default auto-negotiation mode driven by
 * hardware strap pins; MCU supplies the 50MHz RMII reference clock from
 * PLL0/div internally.
 *
 * Devkit (NCSI_USE_PHY_MDIO): PHY is brought up via MDIO (LAN8741 driver);
 * the PHY itself sources the 50MHz RMII reference clock into the MCU, so
 * the internal RMII clock generator is left detached.
 */
extern "C" void ECM_InitEnetHardware(void)
{
    // Initialize ENET pins (RMII interface; MDIO when NCSI_USE_PHY_MDIO is set)
    ECM_InitEnetPins();

#if defined(NCSI_USE_PHY_MDIO) && (NCSI_USE_PHY_MDIO > 0)
    // The devkit PHY provides the 50MHz RMII reference clock to the MCU.
    CLOCK_AttachClk(kNONE_to_ENETRMII);
#else
    // Select clock divider based on chip revision:
    // A2+ revision: PLL0 = 150MHz, divide by 3 to get 50MHz
    // Pre-A2:       PLL0 = 100MHz, divide by 2 to get 50MHz
    const uint32_t A2_REVISION = 0xA2;
    const uint32_t rev         = SYSCON->DIEID
                       & (SYSCON_DIEID_MAJOR_REVISION_MASK | SYSCON_DIEID_MINOR_REVISION_MASK);
    const uint32_t div = (rev >= A2_REVISION) ? 3U : 2U;

    CLOCK_AttachClk(kPLL0_to_ENETRMII);
    CLOCK_SetClkDiv(kCLOCK_DivEnetrmiiClk, div);

    CLOCK_AttachClk(kPLL0_to_ENETPTPREF);
    CLOCK_SetClkDiv(kCLOCK_DivEnetptprefClk, div);
#endif

    // Enable ENET clock
    CLOCK_EnableClock(kCLOCK_Enet);

    // Reset ENET
    SYSCON0->PRESETCTRL2 |= SYSCON_PRESETCTRL2_ENET_RST_MASK;
    SYSCON0->PRESETCTRL2 &= ~SYSCON_PRESETCTRL2_ENET_RST_MASK;

    // Get system clock for ENET
    BOARD_PhySysClock = CLOCK_GetCoreSysClkFreq();

#if defined(NCSI_USE_PHY_MDIO) && (NCSI_USE_PHY_MDIO > 0)
    // Initialize MDIO interface and wire up PHY resource read/write callbacks
    MDIO_Init();
    g_phy_resource.read  = NCSI_MDIO_Read;
    g_phy_resource.write = NCSI_MDIO_Write;
#endif

    // Set ENET interrupt priority
    NVIC_SetPriority(ETHERNET_IRQn, ENET_INTERRUPT_PRIORITY);
}

#endif  // USB_DEVICE_CONFIG_CDC_ECM

/*******************************************************************************
 * USB IRQ Handlers
 ******************************************************************************/
// External USB device handle for ISR
extern "C" {
extern usb_device_handle g_ecm_device_handle;
}

#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
extern "C" void USB1_HS_IRQHandler(void)
{
    USB_DeviceEhciIsrFunction(g_ecm_device_handle);
}
#elif defined(USB_DEVICE_CONFIG_LPCIP3511HS) && (USB_DEVICE_CONFIG_LPCIP3511HS > 0U)
extern "C" void USB1_HS_IRQHandler(void)
{
    USB_DeviceLpcIp3511IsrFunction(g_ecm_device_handle);
}
#endif

#if defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0U)
extern "C" void USB0_FS_IRQHandler(void)
{
    USB_DeviceKhciIsrFunction(g_ecm_device_handle);
}
#elif defined(USB_DEVICE_CONFIG_LPCIP3511FS) && (USB_DEVICE_CONFIG_LPCIP3511FS > 0U)
extern "C" void USB0_FS_IRQHandler(void)
{
    USB_DeviceLpcIp3511IsrFunction(g_ecm_device_handle);
}
#endif

/*******************************************************************************
 * USB Device Task Function
 ******************************************************************************/
#if defined(USB_DEVICE_CONFIG_USE_TASK) && (USB_DEVICE_CONFIG_USE_TASK > 0U)
extern "C" void USB_DeviceTaskFn(void* deviceHandle)
{
    USB_DeviceTaskFunction(deviceHandle);
}
#endif

/*******************************************************************************
 * USB Device Clock Init
 ******************************************************************************/
extern "C" void USB_DeviceClockInit(void)
{
#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
    usb_phy_config_struct_t phyConfig = {
        BOARD_USB_PHY_D_CAL,
        BOARD_USB_PHY_TXCAL45DP,
        BOARD_USB_PHY_TXCAL45DM,
    };

    SPC0->ACTIVE_VDELAY = 0x0500;
    // Change the power DCDC to 1.8v, CORELDO to 1.1v
    SPC0->ACTIVE_CFG &= ~SPC_ACTIVE_CFG_CORELDO_VDD_DS_MASK;
    SPC0->ACTIVE_CFG |= SPC_ACTIVE_CFG_DCDC_VDD_LVL(0x3) | SPC_ACTIVE_CFG_CORELDO_VDD_LVL(0x3)
                      | SPC_ACTIVE_CFG_SYSLDO_VDD_DS_MASK | SPC_ACTIVE_CFG_DCDC_VDD_DS(0x2u);
    // Wait until it is done with timeout
    {
        volatile uint32_t spc_timeout = 100000U;
        while ((SPC0->SC & SPC_SC_BUSY_MASK) && (spc_timeout > 0U)) {
            spc_timeout--;
        }
    }
    if (0u == (SCG0->LDOCSR & SCG_LDOCSR_LDOEN_MASK)) {
        SCG0->TRIM_LOCK  = 0x5a5a0001U;
        SCG0->LDOCSR    |= SCG_LDOCSR_LDOEN_MASK;
        // Wait LDO ready with timeout
        volatile uint32_t ldo_timeout = 100000U;
        while ((0U == (SCG0->LDOCSR & SCG_LDOCSR_VOUT_OK_MASK)) && (ldo_timeout > 0U)) {
            ldo_timeout--;
        }
    }
    SYSCON->AHBCLKCTRLSET[2] |= SYSCON_AHBCLKCTRL2_USB_HS_MASK
                              | SYSCON_AHBCLKCTRL2_USB_HS_PHY_MASK;
    SCG0->SOSCCFG &= ~(SCG_SOSCCFG_RANGE_MASK | SCG_SOSCCFG_EREFS_MASK);
    // xtal = 20 ~ 30MHz
    SCG0->SOSCCFG  = (1U << SCG_SOSCCFG_RANGE_SHIFT) | (1U << SCG_SOSCCFG_EREFS_SHIFT);
    SCG0->SOSCCSR |= SCG_SOSCCSR_SOSCEN_MASK;
    // Wait for SOSC valid with timeout to avoid infinite loop if external crystal is not
    // present
    volatile uint32_t timeout = 1000000U;
    while (timeout > 0U) {
        if (SCG0->SOSCCSR & SCG_SOSCCSR_SOSCVLD_MASK) {
            break;
        }
        timeout--;
    }
    // If timeout, external crystal may not be present - USB HS will not work
    SYSCON->CLOCK_CTRL |= SYSCON_CLOCK_CTRL_CLKIN_ENA_MASK
                        | SYSCON_CLOCK_CTRL_CLKIN_ENA_FM_USBH_LPT_MASK;
    CLOCK_EnableClock(kCLOCK_UsbHs);
    CLOCK_EnableClock(kCLOCK_UsbHsPhy);
    CLOCK_EnableUsbhsPhyPllClock(kCLOCK_Usbphy480M, 24000000U);
    CLOCK_EnableUsbhsClock();
    USB_EhciPhyInit(CONTROLLER_ID, BOARD_XTAL0_CLK_HZ, &phyConfig);
#endif

#if defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0U)
    CLOCK_AttachClk(kCLK_48M_to_USB0);
    CLOCK_EnableClock(kCLOCK_Usb0Ram);
    CLOCK_EnableClock(kCLOCK_Usb0Fs);
    CLOCK_EnableUsbfsClock();
#endif
}

/*******************************************************************************
 * USB Device ISR Enable
 ******************************************************************************/
extern "C" void USB_DeviceIsrEnable(void)
{
    uint8_t irqNumber;

#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
    uint8_t usbDeviceEhciIrq[] = USBHS_IRQS;
    irqNumber                  = usbDeviceEhciIrq[CONTROLLER_ID - kUSB_ControllerEhci0];

    // Install isr, set priority, and enable IRQ
    NVIC_SetPriority((IRQn_Type)irqNumber, USB_DEVICE_INTERRUPT_PRIORITY);
    EnableIRQ((IRQn_Type)irqNumber);
#endif

#if defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0U)
    uint8_t usbDeviceKhciIrq[] = USBFS_IRQS;
    irqNumber                  = usbDeviceKhciIrq[CONTROLLER_ID - kUSB_ControllerKhci0];

    // Install isr, set priority, and enable IRQ
    NVIC_SetPriority((IRQn_Type)irqNumber, USB_DEVICE_INTERRUPT_PRIORITY);
    EnableIRQ((IRQn_Type)irqNumber);
#endif
}

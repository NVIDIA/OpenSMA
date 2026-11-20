/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sys/usb/usb_fs.h"

#if defined(USB_CONFIG_KHCI_MCTP) && (USB_CONFIG_KHCI_MCTP > 0U)

#include <FreeRTOS.h>
#include <task.h>

#include "fsl_clock.h"
#include "fsl_device_registers.h"
#include "nv/common/preproc.h"
#include "nv/usb/fs_task.h"
#include "sys/common/common.h"
#include "sys/usb/usb_device_config.h"
#include "sys/usb/usb_device_descriptor.h"
#include "sys/usb/usb_device_mctp.h"
#include NV_IPC_CONFIG_H

extern "C" {

namespace {

constexpr uint8_t FsConfigurationIndex = 1U;
constexpr uint8_t FsInterfaceIndex     = 0U;
constexpr uint8_t FsClassCount         = 1U;
constexpr uint8_t FsMctpSubclass       = 0x02U;  // Managed endpoint with packet spanning
constexpr uint8_t FsMctpEndpoint       = 1U;

struct FsDeviceState
{
    usb_device_handle device_handle{};
    class_handle_t    mctp_handle{};
    uint8_t*          rx_buffer{};
    uint8_t           configuration{};
    uint8_t           alternate_setting{};
    uint8_t           attached{};
    bool              rx_armed{};
    uint32_t          rx_length{USB_CANCELLED_TRANSFER_LENGTH};
};

NV_SHARED_BSS FsDeviceState g_fs_device;

USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
NV_SHARED_DATA uint8_t g_fs_device_descriptor[] = {
    USB_DESCRIPTOR_LENGTH_DEVICE,
    USB_DESCRIPTOR_TYPE_DEVICE,
    0x10U,
    0x01U,  // USB 1.10; avoids high-speed qualifier requests on KHCI
    USB_MCTP_GENERIC_CLASS,
    FsMctpSubclass,
    USB_MCTP_GENERIC_PROTOCOL,
    0x40U,
    USB_SHORT_GET_LOW(nv::ipc::UsbDeviceVid),
    USB_SHORT_GET_HIGH(nv::ipc::UsbDeviceVid),
    USB_SHORT_GET_LOW(nv::ipc::UsbDevicePid),
    USB_SHORT_GET_HIGH(nv::ipc::UsbDevicePid),
    0x00U,
    0x01U,
    0x00U,
    0x00U,
    0x00U,
    0x01U,
};

USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
NV_SHARED_DATA uint8_t g_fs_configuration_descriptor[] = {
    USB_DESCRIPTOR_LENGTH_CONFIGURE,
    USB_DESCRIPTOR_TYPE_CONFIGURE,
    USB_SHORT_GET_LOW(USB_DESCRIPTOR_LENGTH_CONFIGURE + USB_DESCRIPTOR_LENGTH_INTERFACE
                      + (2U * USB_DESCRIPTOR_LENGTH_ENDPOINT)),
    USB_SHORT_GET_HIGH(USB_DESCRIPTOR_LENGTH_CONFIGURE + USB_DESCRIPTOR_LENGTH_INTERFACE
                       + (2U * USB_DESCRIPTOR_LENGTH_ENDPOINT)),
    0x01U,
    FsConfigurationIndex,
    0x00U,
    USB_DESCRIPTOR_CONFIGURE_ATTRIBUTE_D7_MASK
        | (USB_DEVICE_CONFIG_SELF_POWER
           << USB_DESCRIPTOR_CONFIGURE_ATTRIBUTE_SELF_POWERED_SHIFT),
    USB_DEVICE_MAX_POWER,
    USB_DESCRIPTOR_LENGTH_INTERFACE,
    USB_DESCRIPTOR_TYPE_INTERFACE,
    FsInterfaceIndex,
    0x00U,
    USB_MCTP_ENDPOINT_COUNT,
    USB_MCTP_GENERIC_CLASS,
    FsMctpSubclass,
    USB_MCTP_GENERIC_PROTOCOL,
    0x00U,
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    FsMctpEndpoint | (USB_IN << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),
    USB_ENDPOINT_BULK,
    USB_SHORT_GET_LOW(64U),
    USB_SHORT_GET_HIGH(64U),
    0x01U,
    USB_DESCRIPTOR_LENGTH_ENDPOINT,
    USB_DESCRIPTOR_TYPE_ENDPOINT,
    FsMctpEndpoint | (USB_OUT << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),
    USB_ENDPOINT_BULK,
    USB_SHORT_GET_LOW(64U),
    USB_SHORT_GET_HIGH(64U),
    0x01U,
};

USB_DMA_INIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
NV_SHARED_DATA uint8_t g_fs_string0[] = {0x04U, USB_DESCRIPTOR_TYPE_STRING, 0x09U, 0x04U};

NV_SHARED_DATA usb_device_endpoint_struct_t g_fs_mctp_endpoints[] = {
    { FsMctpEndpoint | (USB_IN << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),
     USB_ENDPOINT_BULK, 64U,
     0x01U},
    {FsMctpEndpoint | (USB_OUT << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT),
     USB_ENDPOINT_BULK, 64U,
     0x01U},
};

NV_SHARED_DATA usb_device_interface_struct_t g_fs_mctp_interface[] = {
    {0U, {USB_MCTP_ENDPOINT_COUNT, g_fs_mctp_endpoints}, nullptr},
};

NV_SHARED_DATA usb_device_interfaces_struct_t g_fs_mctp_interfaces[] = {
    {USB_MCTP_GENERIC_CLASS,
     FsMctpSubclass, USB_MCTP_GENERIC_PROTOCOL,
     FsInterfaceIndex, g_fs_mctp_interface,
     1U},
};

NV_SHARED_DATA usb_device_interface_list_t g_fs_mctp_interface_list[] = {
    {1U, g_fs_mctp_interfaces},
};

NV_SHARED_DATA usb_device_class_struct_t g_fs_mctp_class_info = {
    g_fs_mctp_interface_list,
    static_cast<_usb_usb_device_class_type>(USB_MCTP_GENERIC_CLASS),
    1U,
};

/**
 * @brief Handle MCTP transfer completions and notify the full-speed USB task.
 * @param handle MCTP class handle used to send a terminating zero-length packet.
 * @param event MCTP class event identifier.
 * @param param Endpoint completion message, or nullptr.
 * @return Success for handled events, otherwise kStatus_USB_InvalidRequest.
 */
usb_status_t fs_mctp_callback(class_handle_t handle, uint32_t event, void* param)
{
    auto* message = static_cast<usb_device_endpoint_callback_message_struct_t*>(param);
    switch (event) {
        case kUSB_DeviceMctpEventSendResponse:
            // DSP0283 spanning mode requires a short transaction to terminate a
            // transfer. Send a ZLP when the packet ends on the 64-byte FS MPS.
            if (message != nullptr && message->length != 0U && (message->length % 64U) == 0U) {
                auto status = USB_DeviceMctpSend(handle, FsMctpEndpoint, nullptr, 0U);
                if (status == kStatus_USB_Success) {
                    return status;
                }
            }
            nv::usb::FsTask::set_mctp_tx_done_event();
            return kStatus_USB_Success;
        case kUSB_DeviceMctpEventRecvResponse:
            g_fs_device.rx_armed = false;
            if (g_fs_device.attached != 0U && message != nullptr
                && message->length != USB_CANCELLED_TRANSFER_LENGTH) {
                g_fs_device.rx_length = message->length;
                nv::usb::FsTask::set_mctp_rx_event();
            }
            return kStatus_USB_Success;
        default: return kStatus_USB_InvalidRequest;
    }
}

/**
 * @brief Handle enumeration, descriptors, and full-speed USB connection state.
 *
 * Reset and deconfiguration discard any pending receive byte count.
 * @param event USB device event identifier.
 * @param param Event-specific request or response data.
 * @return USB status indicating whether the event was handled successfully.
 */
usb_status_t fs_device_callback(usb_device_handle, uint32_t event, void* param)
{
    auto* value8  = static_cast<uint8_t*>(param);
    auto* value16 = static_cast<uint16_t*>(param);

    switch (event) {
        case kUSB_DeviceEventBusReset:
            g_fs_device.attached      = 0U;
            g_fs_device.configuration = 0U;
            g_fs_device.rx_armed      = false;
            g_fs_device.rx_length     = USB_CANCELLED_TRANSFER_LENGTH;
            nv::usb::FsTask::reset_events();
            return kStatus_USB_Success;
        case kUSB_DeviceEventSetConfiguration:
            if (value8 == nullptr) {
                return kStatus_USB_InvalidRequest;
            }
            if (*value8 == 0U) {
                g_fs_device.attached      = 0U;
                g_fs_device.configuration = 0U;
                g_fs_device.rx_armed      = false;
                g_fs_device.rx_length     = USB_CANCELLED_TRANSFER_LENGTH;
                nv::usb::FsTask::reset_events();
                return kStatus_USB_Success;
            }
            if (*value8 == FsConfigurationIndex) {
                // The class leaves endpoints unchanged for the current configuration.
                // Preserve any completion still owned by the receive task.
                if (g_fs_device.configuration == *value8) {
                    return kStatus_USB_Success;
                }
                g_fs_device.attached      = 1U;
                g_fs_device.configuration = *value8;
                nv::usb::FsTask::set_device_attach_event();
                return kStatus_USB_Success;
            }
            return kStatus_USB_InvalidRequest;
        case kUSB_DeviceEventSetInterface:
            if (value16 != nullptr && g_fs_device.attached != 0U
                && ((*value16 >> 8U) & 0xFFU) == FsInterfaceIndex && (*value16 & 0xFFU) == 0U) {
                g_fs_device.alternate_setting = 0U;
                // Alternate setting zero is the only setting; the class does not
                // recreate endpoints. The task retains ownership of RX rearming.
                return kStatus_USB_Success;
            }
            return kStatus_USB_InvalidRequest;
        case kUSB_DeviceEventGetConfiguration:
            if (value8 != nullptr) {
                *value8 = g_fs_device.configuration;
                return kStatus_USB_Success;
            }
            return kStatus_USB_InvalidRequest;
        case kUSB_DeviceEventGetInterface:
            if (value16 != nullptr && ((*value16 >> 8U) & 0xFFU) == FsInterfaceIndex) {
                *value16 = (*value16 & 0xFF00U) | g_fs_device.alternate_setting;
                return kStatus_USB_Success;
            }
            return kStatus_USB_InvalidRequest;
        case kUSB_DeviceEventGetDeviceDescriptor:
            if (param != nullptr) {
                auto* descriptor = static_cast<usb_device_get_device_descriptor_struct_t*>(
                    param);
                descriptor->buffer = g_fs_device_descriptor;
                descriptor->length = sizeof(g_fs_device_descriptor);
                return kStatus_USB_Success;
            }
            return kStatus_USB_InvalidRequest;
        case kUSB_DeviceEventGetConfigurationDescriptor:
            if (param != nullptr) {
                auto*
                    descriptor = static_cast<usb_device_get_configuration_descriptor_struct_t*>(
                        param);
                if (descriptor->configuration == 0U) {
                    descriptor->buffer = g_fs_configuration_descriptor;
                    descriptor->length = sizeof(g_fs_configuration_descriptor);
                    return kStatus_USB_Success;
                }
            }
            return kStatus_USB_InvalidRequest;
        case kUSB_DeviceEventGetStringDescriptor:
            if (param != nullptr) {
                auto* descriptor = static_cast<usb_device_get_string_descriptor_struct_t*>(
                    param);
                if (descriptor->stringIndex == 0U) {
                    descriptor->buffer = g_fs_string0;
                    descriptor->length = sizeof(g_fs_string0);
                    return kStatus_USB_Success;
                }
            }
            return kStatus_USB_InvalidRequest;
        default: return kStatus_USB_InvalidRequest;
    }
}

NV_SHARED_DATA usb_device_class_config_struct_t g_fs_class_config[] = {
    {fs_mctp_callback, nullptr, &g_fs_mctp_class_info},
};

NV_SHARED_DATA usb_device_class_config_list_struct_t g_fs_class_config_list = {
    g_fs_class_config,
    fs_device_callback,
    FsClassCount,
};

}  // namespace

/** @brief Dispatch the USB0 full-speed interrupt to the KHCI device driver. */
void USB0_FS_IRQHandler(void)
{
    USB_DeviceKhciIsrFunction(g_fs_device.device_handle);
}

}  // extern "C"

namespace sys::usb {

/**
 * @brief Initialize clocks, the MCTP class, and the full-speed USB interrupt.
 * @param rx_buffer Persistent receive buffer of USB_MCTP_OUT_BUFFER_LENGTH bytes.
 * @return Status from USB class initialization or device startup.
 */
uint8_t FsDriver::init(uint8_t* rx_buffer)
{
    g_fs_device           = {};
    g_fs_device.rx_buffer = rx_buffer;

    CLOCK_AttachClk(kCLK_48M_to_USB0);
    CLOCK_EnableClock(kCLOCK_Usb0Ram);
    CLOCK_EnableClock(kCLOCK_Usb0Fs);
    CLOCK_EnableUsbfsClock();

    auto status = USB_DeviceClassInit(
        kUSB_ControllerKhci0, &g_fs_class_config_list, &g_fs_device.device_handle);
    if (status != kStatus_USB_Success) {
        return static_cast<uint8_t>(status);
    }

    g_fs_device.mctp_handle = g_fs_class_config_list.config[0].classHandle;
    const uint8_t irq[]     = USBFS_IRQS;
    static_assert(nv::ipc::UsbDeviceInterruptPriority
                  >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_SetPriority(static_cast<IRQn_Type>(irq[0]), nv::ipc::UsbDeviceInterruptPriority);
    EnableIRQ(static_cast<IRQn_Type>(irq[0]));

    return static_cast<uint8_t>(USB_DeviceRun(g_fs_device.device_handle));
}

/**
 * @brief Submit a transmit request while excluding USB interrupts.
 * @param data Transmit buffer retained until completion.
 * @param length Number of bytes to send.
 * @return Status from USB_DeviceMctpSend().
 */
uint8_t FsDriver::write_mctp(uint8_t* data, uint32_t length)
{
    taskENTER_CRITICAL();
    auto status = USB_DeviceMctpSend(g_fs_device.mctp_handle, FsMctpEndpoint, data, length);
    taskEXIT_CRITICAL();
    return static_cast<uint8_t>(status);
}

/**
 * @brief Read and clear the pending receive length while excluding USB interrupts.
 * @return Completed byte count, or USB_CANCELLED_TRANSFER_LENGTH if none is pending.
 */
uint32_t FsDriver::take_mctp_rx_length()
{
    taskENTER_CRITICAL();
    const auto length     = g_fs_device.rx_length;
    g_fs_device.rx_length = USB_CANCELLED_TRANSFER_LENGTH;
    taskEXIT_CRITICAL();
    return length;
}

/**
 * @brief Submit a receive request for the full buffer while excluding USB interrupts.
 * @return Status from USB_DeviceMctpRecv().
 */
usb_status_t FsDriver::enable_mctp_rx()
{
    taskENTER_CRITICAL();
    auto status = kStatus_USB_Success;
    // A reset/configuration event can interrupt the task before its attach bit
    // is handled. Do not duplicate an arm or overwrite a pending completion.
    if (g_fs_device.attached != 0U && !g_fs_device.rx_armed
        && g_fs_device.rx_length == USB_CANCELLED_TRANSFER_LENGTH) {
        status               = USB_DeviceMctpRecv(g_fs_device.mctp_handle,
                                    FsMctpEndpoint,
                                    g_fs_device.rx_buffer,
                                    USB_MCTP_OUT_BUFFER_LENGTH);
        g_fs_device.rx_armed = status == kStatus_USB_Success;
    }
    taskEXIT_CRITICAL();
    return status;
}

/**
 * @brief Inspect the full-speed device attachment and configuration state.
 * @return True when the device is attached and configured.
 */
bool FsDriver::is_device_connected()
{
    return g_fs_device.attached != 0U && g_fs_device.configuration != 0U;
}

}  // namespace sys::usb

#else

namespace sys::usb {
/**
 * @brief Reject initialization when full-speed MCTP support is disabled.
 * @return 1 to indicate initialization failure.
 */
uint8_t FsDriver::init(uint8_t*)
{
    return 1U;
}
/**
 * @brief Reject transmission when full-speed MCTP support is disabled.
 * @return 1 to indicate transmission failure.
 */
uint8_t FsDriver::write_mctp(uint8_t*, uint32_t)
{
    return 1U;
}
/**
 * @brief Report that no receive completion exists when full-speed MCTP is disabled.
 * @return USB_CANCELLED_TRANSFER_LENGTH.
 */
uint32_t FsDriver::take_mctp_rx_length()
{
    return USB_CANCELLED_TRANSFER_LENGTH;
}
/**
 * @brief Reject reception when full-speed MCTP support is disabled.
 * @return kStatus_USB_Error.
 */
usb_status_t FsDriver::enable_mctp_rx()
{
    return kStatus_USB_Error;
}
/**
 * @brief Report the disabled full-speed MCTP device as disconnected.
 * @return False.
 */
bool FsDriver::is_device_connected()
{
    return false;
}
}  // namespace sys::usb

#endif

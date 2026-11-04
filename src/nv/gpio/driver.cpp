#include "nv/gpio/driver.h"
#include "nv/common/preproc.h"
#include "nv/ipc/event.h"
#include "nv/ipc/tasknotify.h"

#include NV_IPC_CONFIG_H

#if defined(USB_CONFIG_LSTP) && USB_CONFIG_LSTP
#include "nv/lstp/lstp_task.h"
#endif

#include <array>
#include <atomic>
#include <type_traits>
#include <variant>

namespace nv::gpio {

namespace {

// State for emulated virtual GPIO levels (indexed by GpioSetup).
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
NV_SHARED_BSS std::array<std::atomic<uint8_t>, nv::ipc::GpioNum> g_virtual_gpio_level{};

void send_virtual_gpio_notification(GpioPort port, GpioPin pin)
{
    if constexpr (nv::gpio::VirtualGpioEventSetup.size() != 0) {
        for (const auto& virtual_gpio_event : nv::gpio::VirtualGpioEventSetup) {
            if (virtual_gpio_event.port != port || virtual_gpio_event.pin != pin) {
                continue;
            }

            std::visit(
                [&, bits = virtual_gpio_event.bits](auto&& notification_target_id) {
                    using NotificationTargetId = std::decay_t<decltype(notification_target_id)>;
                    if constexpr (std::is_same_v<NotificationTargetId, nv::ipc::EventId>) {
                        (void)nv::ipc::Event::make(notification_target_id).set(bits);
                    }
                    else if constexpr (std::is_same_v<NotificationTargetId, nv::ipc::TaskId>) {
#if defined(USB_CONFIG_LSTP) && USB_CONFIG_LSTP
                        if constexpr (nv::lstp::EnableGpio) {
                            if (notification_target_id == NotificationTargetId::Lstp) {
                                const auto gpio_index = nv::lstp::LstpGpioIdFromPortPin(
                                    nv::lstp::LstpGpioIdxMap, sys::gpio::PortsNumber, pin);
                                nv::lstp::LstpTask::submit_gpio_irq(gpio_index);
                                return;
                            }
                        }
#endif
                        (void)nv::ipc::TaskNotify::send(notification_target_id, bits);
                    }
                    else {
                        static_assert(!sizeof(NotificationTargetId),
                                      "Unhandled VirtualGpioNotificationTarget alternative");
                    }
                },
                virtual_gpio_event.notification_target);
            return;
        }
    }
}

}  // namespace

Status Driver::read_virtual_physical_gpio(GpioPort port, GpioPin pin, uint8_t& data)
{
    if (port == vrPort) {
        for (uint16_t i = 0; i < nv::ipc::GpioNum; ++i) {
            const auto& g = nv::ipc::GpioSetup.at(i);
            if (std::get<0>(g) == port && std::get<1>(g) == pin) {
                data = g_virtual_gpio_level.at(i).load(std::memory_order_relaxed);
                return Status::Ok;
            }
        }
        return Status::InvalidParam;
    }

    if (port < sys::gpio::PortsNumber) {
        return Driver::read(port, pin, data);
    }

    return Status::InvalidParam;
}

Status Driver::write_virtual_physical_gpio(GpioPort port, GpioPin pin, uint8_t data)
{
    if (port == vrPort) {
        const auto bit = static_cast<uint8_t>(data & 1U);
        for (uint16_t i = 0; i < nv::ipc::GpioNum; ++i) {
            const auto& g = nv::ipc::GpioSetup.at(i);
            if (std::get<0>(g) == port && std::get<1>(g) == pin) {
                g_virtual_gpio_level.at(i).store(bit, std::memory_order_relaxed);
                send_virtual_gpio_notification(port, pin);
                return Status::Ok;
            }
        }
        return Status::InvalidParam;
    }

    if (port < sys::gpio::PortsNumber) {
        return Driver::write(port, pin, data);
    }

    return Status::InvalidParam;
}

}  // namespace nv::gpio

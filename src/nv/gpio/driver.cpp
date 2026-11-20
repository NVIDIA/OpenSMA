#include "nv/gpio/driver.h"
#include "nv/common/preproc.h"
#include "nv/gpio/project_gpio.h"
#include "nv/ipc/event.h"
#include "nv/ipc/tasknotify.h"

#include NV_IPC_CONFIG_H

// Weak default: single-product images return the compile-time GpioSetup table.
// Multi-product images (e.g. pt649) provide a strong override in product_config.cpp.
namespace nv::ipc {
__attribute__((weak)) std::span<const Gpios> project_gpio_setup()
{
    return GpioSetup;
}
}  // namespace nv::ipc

#if defined(USB_CONFIG_LSTP) && USB_CONFIG_LSTP
#include "nv/lstp/lstp_task.h"
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <span>
#include <type_traits>
#include <variant>

namespace nv::gpio {

namespace {

// State for emulated virtual GPIO levels (indexed by GpioSetup).
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
NV_SHARED_BSS std::array<std::atomic<uint8_t>, nv::ipc::GpioNum> g_virtual_gpio_level{};
// Shadow of the NSM Type 5 GPIO spoofing payload. NSM owns the writes; other consumers
// (LSTP GPIO, NSM DCD Get GPIO) read here without reaching into NSM state.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
NV_SHARED_BSS std::atomic<bool> g_gpio_spoofing_active{};
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
NV_SHARED_BSS std::atomic<uint8_t> g_gpio_spoofing_entries_num{};
// Aliased so the declaration below fits on one line and NOLINTNEXTLINE lands on the
// line clang-tidy reports (the variable name, not the type).
using GpioSpoofingEntries = std::array<GpioSpoofingEntryState, MaxGpioSpoofingEntries>;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
NV_SHARED_BSS GpioSpoofingEntries g_gpio_spoofing_entries{};

/// Whether GPIO spoofing covers this pin. The project policy decides when it knows the pin;
/// otherwise the hardware direction is the criterion.
bool is_spoofable_pin(GpioPort port, GpioPin pin)
{
    bool spoofable = false;
    if (GpioSpoofingPinPolicy<>::is_spoofable(port, pin, spoofable)) {
        return spoofable;
    }

    // Virtual GPIO has no hardware direction; treat as input for spoofing eligibility.
    if (port == vrPort) {
        return true;
    }

    Direction dir = Direction::Input;
    return Driver::getDirection(port, pin, dir) == Status::Ok && dir == Direction::Input;
}

/// Level a pin reports inside the spoofed view: an activated entry wins, otherwise the
/// configured default, so that the view stays isolated from the physical pin state.
bool spoofed_level(uint16_t                                gpio_index,
                   std::span<const GpioSpoofingEntryState> entries,
                   uint8_t&                                data)
{
    for (const auto& entry : entries) {
        if (entry.gpioIndex == gpio_index && entry.activated == 1U) {
            data = entry.polarity;
            return true;
        }
    }

    return Driver::get_gpio_default_value(gpio_index, data);
}

#if defined(USB_CONFIG_LSTP) && USB_CONFIG_LSTP
void notify_spoofed_level_change(uint16_t                                gpio_index,
                                 std::span<const GpioSpoofingEntryState> before,
                                 std::span<const GpioSpoofingEntryState> after)
{
    if (gpio_index >= nv::ipc::gpio_count()) {
        return;
    }

    const auto& gpio_entry = nv::ipc::gpio_at(gpio_index);
    const auto  port       = std::get<0>(gpio_entry);
    const auto  pin        = std::get<1>(gpio_entry);
    if (!is_spoofable_pin(port, pin)) {
        return;
    }

    const auto lstp_index = nv::lstp::LstpGpioIdFromPortPin(
        nv::lstp::LstpGpioIdxMap, (port == vrPort) ? sys::gpio::PortsNumber : port, pin);
    if (lstp_index >= nv::lstp::LstpGpioNum) {
        return;
    }

    uint8_t old_level = 0;
    uint8_t new_level = 0;
    if (!spoofed_level(gpio_index, before, old_level)
        || !spoofed_level(gpio_index, after, new_level) || old_level == new_level) {
        return;
    }

    nv::lstp::LstpTask::submit_gpio_irq(lstp_index, nv::lstp::GpioIrqSource::Spoofing);
}

/// Raise an LSTP GPIO IRQ for every pin whose spoofed level changed. Only pins named by either
/// table can change, since the level of an unlisted pin is its configured default.
void notify_spoofed_level_changes(std::span<const GpioSpoofingEntryState> before,
                                  std::span<const GpioSpoofingEntryState> after)
{
    for (const auto& entry : before) {
        notify_spoofed_level_change(entry.gpioIndex, before, after);
    }

    for (const auto& entry : after) {
        const bool already_handled = std::any_of(
            before.begin(), before.end(), [&entry](const GpioSpoofingEntryState& previous) {
                return previous.gpioIndex == entry.gpioIndex;
            });
        if (!already_handled) {
            notify_spoofed_level_change(entry.gpioIndex, before, after);
        }
    }
}
#endif

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
                                // Product PinConfigs (linear search; n <= envelope
                                // LstpGpioNum).
                                const auto pin_configs = nv::lstp::project_pin_configs();
                                for (size_t i = 0; i < pin_configs.size(); ++i) {
                                    if (pin_configs[i].port == nv::gpio::vrPort
                                        && pin_configs[i].pin == pin) {
                                        nv::lstp::LstpTask::submit_gpio_irq(
                                            static_cast<nv::lstp::GpioIndex>(i));
                                        return;
                                    }
                                }
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
        for (uint16_t i = 0; i < nv::ipc::gpio_count(); ++i) {
            const auto& g = nv::ipc::gpio_at(i);
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
        for (uint16_t i = 0; i < nv::ipc::gpio_count(); ++i) {
            const auto& g = nv::ipc::gpio_at(i);
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

void Driver::update_gpio_spoofing(bool active, std::span<const GpioSpoofingEntryState> entries)
{
    const auto entry_num = std::min(entries.size(), g_gpio_spoofing_entries.size());

#if defined(USB_CONFIG_LSTP) && USB_CONFIG_LSTP
    // Snapshot the outgoing state so the spoofed levels can be diffed once the new table is
    // live.
    const bool was_active = g_gpio_spoofing_active.load(std::memory_order_acquire);
    std::array<GpioSpoofingEntryState, MaxGpioSpoofingEntries> previous_entries{};
    const auto previous_num = g_gpio_spoofing_entries_num.load(std::memory_order_acquire);
    for (uint8_t i = 0; i < previous_num; ++i) {
        previous_entries.at(i) = g_gpio_spoofing_entries.at(i);
    }
#endif

    // Hide the table while it is rewritten: a reader that observes zero entries falls back to
    // the configured default instead of matching a half-updated entry.
    g_gpio_spoofing_entries_num.store(0, std::memory_order_relaxed);
    for (size_t i = 0; i < entry_num; ++i) {
        g_gpio_spoofing_entries.at(i) = entries[i];
    }
    g_gpio_spoofing_entries_num.store(static_cast<uint8_t>(entry_num),
                                      std::memory_order_release);
    // Publish the flag last so that a reader observing it also observes the entry table.
    g_gpio_spoofing_active.store(active, std::memory_order_release);

#if defined(USB_CONFIG_LSTP) && USB_CONFIG_LSTP
    if constexpr (is_gpio_spoofing_output_enabled(GpioSpoofingOutputInterface::LstpGpio)) {
        // Only a payload change while already isolated can move a spoofed level: entering
        // isolation stays silent, and leaving it needs no event because the host re-reads the
        // restored physical levels. Pin edges queued before isolation began are dropped by the
        // LSTP task as it dequeues them rather than purged from here, because xQueueReset is a
        // privileged-only FreeRTOS call and this runs in the unprivileged NSM task.
        if (was_active && active) {
            notify_spoofed_level_changes(
                std::span<const GpioSpoofingEntryState>(previous_entries.data(), previous_num),
                std::span<const GpioSpoofingEntryState>(entries.data(), entry_num));
        }
    }
#endif
}

bool Driver::is_gpio_spoofing_active()
{
    // A single atomic load, so this stays usable from ISR context.
    return g_gpio_spoofing_active.load(std::memory_order_acquire);
}

bool Driver::get_gpio_default_value(uint16_t gpio_index, uint8_t& data)
{
    if (gpio_index >= nv::ipc::gpio_count()) {
        return false;
    }

    const auto& gpio_entry = nv::ipc::gpio_at(gpio_index);
    const auto  port       = std::get<0>(gpio_entry);
    const auto  pin        = std::get<1>(gpio_entry);

    if constexpr (is_gpio_spoofing_output_enabled(GpioSpoofingOutputInterface::Iox)
                  && nv::ipc::EnableIoxEmulation) {
        for (const auto& iox_config : nv::ipc::IoxConfigs) {
            for (const auto& pin_config : iox_config.pinConfig) {
                if (pin_config.port == port && pin_config.pin == pin) {
                    data = (pin_config.val == GpioState::High) ? 1U : 0U;
                    return true;
                }
            }
        }
    }

    if (GpioSpoofingPinPolicy<>::get_default_value(port, pin, data)) {
        return true;
    }

    data = 0;
    return true;
}

bool Driver::read_gpio_spoofing_value(uint16_t gpio_index, uint8_t& data)
{
    if (!is_gpio_spoofing_active() || gpio_index >= nv::ipc::gpio_count()) {
        return false;
    }

    const auto& gpio_entry = nv::ipc::gpio_at(gpio_index);
    if (!is_spoofable_pin(std::get<0>(gpio_entry), std::get<1>(gpio_entry))) {
        return false;
    }

    const auto entry_num = g_gpio_spoofing_entries_num.load(std::memory_order_acquire);
    return spoofed_level(
        gpio_index,
        std::span<const GpioSpoofingEntryState>(g_gpio_spoofing_entries.data(), entry_num),
        data);
}

bool Driver::read_gpio_spoofing_value(GpioPort port, GpioPin pin, uint8_t& data)
{
    if (!is_gpio_spoofing_active()) {
        return false;
    }

    for (uint16_t gpio_index = 0; gpio_index < nv::ipc::gpio_count(); ++gpio_index) {
        const auto& gpio_entry = nv::ipc::gpio_at(gpio_index);
        if (std::get<0>(gpio_entry) == port && std::get<1>(gpio_entry) == pin) {
            return read_gpio_spoofing_value(gpio_index, data);
        }
    }

    return false;
}

}  // namespace nv::gpio

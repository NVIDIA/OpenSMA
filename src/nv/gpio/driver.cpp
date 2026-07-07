#include "nv/gpio/driver.h"
#include "nv/common/preproc.h"
#include "nv/ipc/event.h"

#include NV_IPC_CONFIG_H

#include <array>
#include <atomic>

namespace nv::gpio {

namespace {

// State for emulated virtual GPIO levels (indexed by GpioSetup).
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
NV_SHARED_BSS std::array<std::atomic<uint8_t>, nv::ipc::GpioNum> g_virtual_gpio_level{};

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
                if constexpr (nv::gpio::VirtualGpioEventSetup.size() != 0) {
                    for (const auto& m : nv::gpio::VirtualGpioEventSetup) {
                        if (m.port == port && m.pin == pin) {
                            (void)nv::ipc::Event::make(m.event).set(m.bits);
                            break;
                        }
                    }
                }
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

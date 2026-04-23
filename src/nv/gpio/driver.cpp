#include "nv/gpio/driver.h"
#include "nv/common/preproc.h"
#include "nv/iox/common.h"

#include NV_IPC_CONFIG_H

#include <array>

namespace nv::gpio {

namespace {

// Shadow of emulated virtual GPIO levels (by GpioSetup index). IOX task updates via
// push_virtual_gpio_level; NSM reads here without touching IOX Task memory.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
NV_SHARED_BSS std::array<uint8_t, nv::ipc::GpioNum> g_virtual_gpio_level{};

}  // namespace

void Driver::push_virtual_gpio_level(uint16_t gpio_index, uint8_t level)
{
    if constexpr (nv::ipc::IoxNum == 0) {
        (void)gpio_index;
        (void)level;
        return;
    }
    if (gpio_index >= nv::ipc::GpioNum) {
        return;
    }
    g_virtual_gpio_level.at(gpio_index) = static_cast<uint8_t>(level & 1U);
}

Status Driver::read_virtual_physical_gpio(GpioPort port, GpioPin pin, uint8_t& data)
{
    if (port == nv::iox::vrPort) {
        if constexpr (nv::ipc::IoxNum == 0) {
            return Status::InvalidParam;
        }
        for (uint16_t i = 0; i < nv::ipc::GpioNum; ++i) {
            const auto& g = nv::ipc::GpioSetup.at(i);
            if (std::get<0>(g) == port && std::get<1>(g) == pin) {
                data = g_virtual_gpio_level.at(i);
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

}  // namespace nv::gpio

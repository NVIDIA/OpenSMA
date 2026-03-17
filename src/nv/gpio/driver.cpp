#include "nv/gpio/driver.h"
#include "nv/iox/common.h"
#include "nv/iox/iox.h"

namespace nv::gpio {

Status Driver::read_virtual_physical_gpio(GpioPort port, GpioPin pin, uint8_t& data)
{
    if (port == nv::iox::vrPort) {
        // TODO: implement virtual GPIO read
        data = 0;
        return Status::Ok;
    }

    else if (port < sys::gpio::PortsNumber) {
        return Driver::read(port, pin, data);
    }

    return Status::InvalidParam;
}

}  // namespace nv::gpio

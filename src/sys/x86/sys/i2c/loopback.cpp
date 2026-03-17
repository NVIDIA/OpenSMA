#include "sys/i2c/loopback.h"

#include "nv/mctp/selftest.h"

#include <cstddef>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cert-dcl37-c,cert-dcl51-cpp)
extern nv::mctp::SelfTest::I2cLoopbackTestResultStruct loopback_result;

namespace sys::i2c {

LoopbackDriver::LoopbackDriver(nv::i2c::Port port) : _port(port) {}

LoopbackDriver::Result LoopbackDriver::start_test()
{
    // Dummy implementation for x86: mark this port as "NotI2C" (skipped).
    const auto idx = static_cast<size_t>(_port);
    if (idx < loopback_result.flexcomm_result.size()) {
        loopback_result.flexcomm_result.at(idx) = static_cast<uint8_t>(Result::NotI2C);
    }
    TestResult = true;
    return Result::NotI2C;
}

}  // namespace sys::i2c

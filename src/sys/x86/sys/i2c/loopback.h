#pragma once

#include <array>
#include <cstdint>
#include <span>

#include NV_IPC_CONFIG_H

namespace sys::i2c {

// x86/testrunner does not have real LPI2C loopback hardware.
// Provide a dummy implementation that matches the MCXN556 API so the
// I2C task and selftest can build and report a deterministic "skipped".
class LoopbackDriver
{
public:
    enum class Result : uint8_t
    {
        Success                = 0x00,
        DataMismatch           = 0x01,
        DataLengthMismatch     = 0x02,
        WriteError             = 0x03,
        SlaveInitFailed        = 0x04,
        CompletionStatusFailed = 0x05,
        NotI2C                 = 0x06,
        I2CNak                 = 0x07,
        MasterInitFailed       = 0x08,
    };

    explicit LoopbackDriver(nv::i2c::Port port);

    Result start_test();

    static constexpr size_t TestTimes = 100;

    nv::i2c::Port _port{};

    static constexpr uint32_t                                 TestDataPatternSize = 76;
    static constexpr std::array<uint8_t, TestDataPatternSize> TestDataPattern     = {
        0x3A, 0xF1, 0x08, 0x9C, 0x27, 0xD4, 0x6E, 0x11, 0xB7, 0x52, 0x0D, 0xA8, 0x7F,
        0xC3, 0x95, 0x2E, 0x64, 0xE9, 0x14, 0x7B, 0x39, 0xAF, 0x01, 0xD2, 0x58, 0xC6,
        0x20, 0x9A, 0xF0, 0x4D, 0x8B, 0x33, 0xBA, 0x05, 0x91, 0x76, 0x2A, 0xDF, 0x48,
        0xC1, 0x6B, 0x10, 0xE3, 0x57, 0x89, 0x2F, 0xA4, 0x7D, 0x12, 0xCD, 0x60, 0x9E,
        0x41, 0xF8, 0x23, 0xB0, 0xFF, 0xFF, 0xFF, 0xFF, 0x96, 0x54, 0xEA, 0x18, 0xB4,
        0x67, 0x02, 0x9D, 0x5F, 0xC8, 0x31, 0x7A, 0x75, 0x0C, 0xD7, 0x3E};

    static constexpr uint8_t TargetAddress = 0x37;

    bool TestResult{true};

private:
    std::array<uint8_t, 2 * TestDataPatternSize> _rx_buffer{};
};

}  // namespace sys::i2c

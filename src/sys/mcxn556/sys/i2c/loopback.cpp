#include "sys/i2c/loopback.h"
#include "sys/i2c/utils.h"
#include NV_IPC_CONFIG_H
#include "nv/logger/log.h"
#include <FreeRTOS.h>
#include <task.h>
#include <cstdint>
#include <utility>
#include "nv/mctp/selftest.h"
#include "fsl_clock.h"
extern nv::mctp::SelfTest::I2cLoopbackTestResultStruct loopback_result;
namespace sys::i2c {

namespace {
uint32_t merge_port_and_result(nv::i2c::Port port, LoopbackDriver::Result result)
{
    return static_cast<uint32_t>(static_cast<uint32_t>(port)
                                 | static_cast<uint32_t>(result) << 8);
};

bool ensure_master_initialized(nv::i2c::Port port, LPI2C_Type* base)
{
    if (base == nullptr) {
        return false;
    }
    if (sys::i2c::is_master_enabled(port)) {
        return true;
    }

    uint32_t lpi2c_clock_hz = CLOCK_GetLPFlexCommClkFreq(LPI2C_GetInstance(base));
    if (lpi2c_clock_hz == 0U) {
        lpi2c_clock_hz = 25000000UL;
    }
    const lpi2c_master_config_t config = {
        .enableMaster            = true,
        .enableDoze              = true,
        .debugEnable             = false,
        .ignoreAck               = false,
        .pinConfig               = kLPI2C_2PinOpenDrain,
        .baudRate_Hz             = 100000UL,
        .busIdleTimeout_ns       = 120000UL,
        .pinLowTimeout_ns        = 0UL,
        .sdaGlitchFilterWidth_ns = 100U,
        .sclGlitchFilterWidth_ns = 100U,
        .hostRequest =
            {
                          .enable   = false,
                          .source   = kLPI2C_HostRequestExternalPin,
                          .polarity = kLPI2C_HostRequestPinActiveHigh,
                          },
    };
    LPI2C_MasterInit(base, &config, lpi2c_clock_hz);
    return true;
}
}  // namespace

LoopbackDriver::LoopbackDriver(nv::i2c::Port port) : _port(port)
{
    _base = sys::i2c::get_base(port);
}

void LoopbackDriver::target_callback(LPI2C_Type*             base,
                                     lpi2c_slave_transfer_t* transfer,
                                     void*                   user_data)
{
    auto driver   = static_cast<LoopbackDriver*>(user_data);
    auto instance = LPI2C_GetInstance(base);
    // nv::info("LoopbackDriver::TargetCallback instance %d, %d\n", instance, transfer->event);

    switch (transfer->event) {
        case kLPI2C_SlaveAddressMatchEvent: {
            transfer->data     = nullptr;
            transfer->dataSize = 0;
            break;
        }
        case kLPI2C_SlaveTransmitEvent: {
            // Not used in loopback (we only receive)
            break;
        }
        case kLPI2C_SlaveReceiveEvent: {
            // Set buffer for slave to receive data
            // Skip first 2 bytes like in i2c.cpp (for protocol overhead)
            transfer->data     = driver->_rx_buffer.data();
            transfer->dataSize = driver->_rx_buffer.size();
            break;
        }
        case kLPI2C_SlaveCompletionEvent: {
            // check completion status
            if (transfer->completionStatus != kStatus_Success) {
                // nv::info("LoopbackDriver::TargetCallback completionStatus %d\n",
                //          transfer->completionStatus);
                const auto result = (transfer->completionStatus == kStatus_LPI2C_Nak)
                                      ? Result::I2CNak
                                      : Result::CompletionStatusFailed;
                loopback_result
                    .flexcomm_result[static_cast<size_t>(driver->_port)] = static_cast<uint8_t>(
                    result);
                nv::logger::Logger::add_from_isr(
                    nv::logger::Event::I2cLoopbackTest.unique_id,
                    nv::logger::Level::Info,
                    nv::logger::data_from_two_u32(
                        merge_port_and_result(driver->_port, result),
                        static_cast<uint32_t>(transfer->completionStatus)));
                driver->TestResult = false;
                break;
            }
            // check transferred count
            if (transfer->transferredCount != TestDataPattern.size()) {
                // nv::info("LoopbackDriver::TargetCallback transferredCount %d != %d\n",
                //          transfer->transferredCount,
                //          TestDataPattern.size());
                loopback_result
                    .flexcomm_result[static_cast<size_t>(driver->_port)] = static_cast<uint8_t>(
                    Result::DataLengthMismatch);
                nv::logger::Logger::add_from_isr(
                    nv::logger::Event::I2cLoopbackTest.unique_id,
                    nv::logger::Level::Info,
                    nv::logger::data_from_two_u32(
                        merge_port_and_result(driver->_port, Result::DataLengthMismatch),
                        static_cast<uint32_t>(transfer->transferredCount)));
                break;
            }
            // check data correctness (data starts at offset 2 in _rx_buffer)
            bool all_match = true;
            for (size_t i = 0; i < transfer->transferredCount; i++) {
                if (driver->_rx_buffer[i] != TestDataPattern[i]) {
                    // nv::info("LoopbackDriver::TargetCallback data[%d] 0x%x != 0x%x\n",
                    //          i,
                    //          driver->_rx_buffer[i],
                    //          TestDataPattern[i]);
                    loopback_result.flexcomm_result[static_cast<size_t>(
                        driver->_port)] = static_cast<uint8_t>(Result::DataMismatch);
                    nv::logger::Logger::add_from_isr(
                        nv::logger::Event::I2cLoopbackTest.unique_id,
                        nv::logger::Level::Info,
                        nv::logger::data_from_two_u32(
                            merge_port_and_result(driver->_port, Result::DataMismatch),
                            static_cast<uint32_t>(driver->_rx_buffer[i])));
                    all_match          = false;
                    driver->TestResult = false;
                    break;
                }
            }
            if (all_match) {
                // nv::info("LoopbackDriver::TargetCallback SUCCESS - all data matched!\n");
                loopback_result
                    .flexcomm_result[static_cast<size_t>(driver->_port)] = static_cast<uint8_t>(
                    Result::Success);
#if 0
                    nv::logger::Logger::add_from_isr(nv::logger::Event::I2cLoopbackTest.unique_id, nv::logger::Level::Info,
                        nv::logger::data_from_two_u32(
                            merge_port_and_result(driver->_port, Result::Success), 0));
#endif
            }
            else {
                driver->TestResult = false;
            }
            break;
        }
        case kLPI2C_SlaveRepeatedStartEvent:
            // TODO the driver call AddressMatchEvent instead of RepeatedStartEvent. Is it a
            // BUG?
            break;
        default: break;
    }
}

LoopbackDriver::Result LoopbackDriver::start_test()
{
    // mark the port as successful
    TestResult                                                  = true;
    loopback_result.flexcomm_result[static_cast<size_t>(_port)] = 0x00;

    // Check if PERSEL is I2C (0b011) only
    // LPI2C_BASE = LP_FLEXCOMM_BASE + 0x800
    // PSELID is at LP_FLEXCOMM_BASE + 0xFF8
    // So PSELID = LPI2C_BASE - 0x800 + 0xFF8 = LPI2C_BASE + 0x7F8
    auto     base_addr   = reinterpret_cast<uintptr_t>(_base);
    auto     pselid_addr = reinterpret_cast<volatile uint32_t*>(base_addr + 0x7F8);
    uint32_t pselid      = *pselid_addr;

    // Extract PERSEL field (bits [2:0])
    uint32_t persel = pselid & 0x7;

    // Skip if not I2C (0b011 only, not UART+I2C 0b111)
    if (persel != 0x3) {
        // nv::info("LoopbackDriver::start_test PERSEL=0x%x, not pure I2C, skipping, port %d\n",
        //          persel,
        //          static_cast<int>(_port));
        nv::logger::info(
            nv::logger::Event::I2cLoopbackTest,
            nv::logger::data_from_two_u32(merge_port_and_result(_port, Result::NotI2C), 0));
        loopback_result.flexcomm_result[static_cast<size_t>(_port)] = static_cast<uint8_t>(
            Result::NotI2C);
        return Result::NotI2C;  // Not an error, just skip
    }

    // Clear receive buffer
    _rx_buffer.fill(0);

    // Ensure master is initialized before we attempt loopback writes.
    if (!ensure_master_initialized(_port, _base)) {
        nv::logger::info(nv::logger::Event::I2cLoopbackTest,
                         nv::logger::data_from_two_u32(
                             merge_port_and_result(_port, Result::MasterInitFailed), 0));
        loopback_result.flexcomm_result[static_cast<size_t>(_port)] = static_cast<uint8_t>(
            Result::MasterInitFailed);
        return Result::MasterInitFailed;
    }

    // Set slave address and create callback handle
    // pg558_gpu mcu flexcomm 2 is connected , cannot use the same address
    // TODO: Use script the prevent loopback test at the same time
    sys::i2c::Driver::set_address(_port, TargetAddress);
    LPI2C_SlaveTransferCreateHandle(_base, &_handle, LoopbackDriver::target_callback, this);

    // Enable SCL clock stretching ("stall") for data phase.
    // - RXSTALL: stretch when receive data flag (RDF) is set (after RX byte).
    // - TXDSTALL: stretch when transmit data flag (TDF) is set (when TX data is needed).
    // This is controlled by lpi2c_slave_config_t::sclStall.{enableRx, enableTx} in SlaveInit,
    // but we force it here to ensure loopback works regardless of board init defaults.
    _base->SCFGR1 |= (LPI2C_SCFGR1_RXSTALL_MASK | LPI2C_SCFGR1_TXDSTALL_MASK);

    LPI2C_MasterEnable(_base, true);
    LPI2C_SlaveEnable(_base, true);

    constexpr uint32_t EventMask = kLPI2C_SlaveAddressMatchEvent | kLPI2C_SlaveCompletionEvent;
    const status_t     Status    = LPI2C_SlaveTransferNonBlocking(_base, &_handle, EventMask);

    if (Status != kStatus_Success) {
        nv::logger::info(nv::logger::Event::I2cLoopbackTest,
                         nv::logger::data_from_two_u32(
                             merge_port_and_result(_port, Result::SlaveInitFailed), Status));
        // nv::info("LoopbackDriver::Init Fail port %d sts %d\n",
        //          static_cast<int>(_port),
        //          static_cast<int>(Status));
        loopback_result.flexcomm_result[static_cast<size_t>(_port)] = static_cast<uint8_t>(
            Result::SlaveInitFailed);

        return Result::SlaveInitFailed;
    }

    // Small delay to ensure slave is ready
    vTaskDelay(pdMS_TO_TICKS(10));

    // Write test data to slave address (loopback)

    auto write_data = TestDataPattern;
    for (int i = 0; i < TestTimes; i++) {
        auto status = sys::i2c::i2c_write(_port, TargetAddress, std::span<uint8_t>(write_data));

        if (loopback_result.flexcomm_result[static_cast<size_t>(_port)] != 0x00) {
            return static_cast<Result>(
                loopback_result.flexcomm_result[static_cast<size_t>(_port)]);
        }

        if (status != nv::i2c::I2cStatus::Ok) {
            loopback_result.flexcomm_result[static_cast<size_t>(_port)] = static_cast<uint8_t>(
                Result::WriteError);
            nv::logger::info(
                nv::logger::Event::I2cLoopbackTest,
                nv::logger::data_from_two_u32(merge_port_and_result(_port, Result::WriteError),
                                              static_cast<uint32_t>(status)));

            return Result::WriteError;
        }
    }

    // Small delay to let transfer complete
    vTaskDelay(pdMS_TO_TICKS(10));

    if (TestResult) {
        nv::logger::info(
            nv::logger::Event::I2cLoopbackTest,
            nv::logger::data_from_two_u32(merge_port_and_result(_port, Result::Success), 0));
    }

    return Result::Success;
}

}  // namespace sys::i2c
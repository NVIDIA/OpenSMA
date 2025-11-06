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
#pragma once
#include <array>
#include <span>

#include "nv/common/utils.h"
#include "nv/flash/common.h"

namespace nv::logger {

constexpr auto EventDataSize = 8;
using EventData              = std::array<uint8_t, EventDataSize>;

constexpr nv::flash::Address LogMetadataStart = sys::flash::config::LoggerStartAddress;
constexpr nv::flash::Address LogEntryStart    = LogMetadataStart + nv::flash::SectorSize;
constexpr nv::flash::Address FaultEntryStart  = LogMetadataStart + 0xE000;  // TBD: refactor

enum class Status : uint16_t  // Changed from uint32_t to save space
{
    Ok,
    Error,
    Busy,
    Timeout,
    InvalidParam,
};

enum class Level : uint8_t
{
    Unknown,
    Debug,
    Info,
    Warning,
    Error,
    Critical,
};
using EventId = uint16_t;
struct EventStructItem
{
    EventId unique_id;
    Level   default_level;
};

struct Event
{
    // Common
    static constexpr EventStructItem CommonPreserved = {0x0000, Level::Unknown};
    static constexpr EventStructItem CommonRaw       = {0x0001, Level::Unknown};
    // Logger
    static constexpr EventStructItem LoggerStart       = {0x0100, Level::Unknown};
    static constexpr EventStructItem LoggerCleanLog    = {0x0101, Level::Unknown};
    static constexpr EventStructItem LoggerLogDrop     = {0x0102, Level::Unknown};
    static constexpr EventStructItem LoggerReqDrop     = {0x0103, Level::Unknown};
    static constexpr EventStructItem SigningClass      = {0x0104, Level::Unknown};
    static constexpr EventStructItem DebugTokenInstall = {0x0105, Level::Unknown};
    static constexpr EventStructItem AddExtTimestamp   = {0x0106, Level::Unknown};
    static constexpr EventStructItem BootWdtDisabled   = {0x0107, Level::Warning};
    // IPC
    static constexpr EventStructItem IpcTaskRegisterTask = {0x0200, Level::Unknown};
    static constexpr EventStructItem IpcTaskInitSuccess  = {0x0201, Level::Info};
    // BootLoader
    static constexpr EventStructItem BootLoaderReadPDS      = {0x0300, Level::Unknown};
    static constexpr EventStructItem BootSlot               = {0x0301, Level::Unknown};
    static constexpr EventStructItem BootReason             = {0x0302, Level::Unknown};
    static constexpr EventStructItem BootWDTPersistent      = {0x0303, Level::Unknown};
    static constexpr EventStructItem BootFmcFaultStatus1    = {0x0304, Level::Unknown};
    static constexpr EventStructItem BootFmcFaultStatus2    = {0x0305, Level::Unknown};
    static constexpr EventStructItem BootSource             = {0x0306, Level::Unknown};
    static constexpr EventStructItem BootSwitchByWdt        = {0x0307, Level::Unknown};
    static constexpr EventStructItem BootFmcStatusCode      = {0x0308, Level::Unknown};
    static constexpr EventStructItem BootFmcOrdinalNumber   = {0x0309, Level::Unknown};
    static constexpr EventStructItem BootFmcAuthResult      = {0x030a, Level::Unknown};
    static constexpr EventStructItem BootCustMkSkCheck      = {0x030b, Level::Unknown};
    static constexpr EventStructItem BootStackGuardInitFail = {0x030c, Level::Warning};
    static constexpr EventStructItem TRNGConfigResult       = {0x030d, Level::Unknown};
    static constexpr EventStructItem BootReasonOriginal     = {0x030e, Level::Unknown};
    // USB
    static constexpr EventStructItem UsbConnectStatus              = {0x0400, Level::Unknown};
    static constexpr EventStructItem UsbCannotSend                 = {0x0401, Level::Unknown};
    static constexpr EventStructItem UsbCannotSendtoMctp           = {0x0402, Level::Unknown};
    static constexpr EventStructItem UsbMctpTxQueueRecvError       = {0x0403, Level::Unknown};
    static constexpr EventStructItem UsbMctpWriteError             = {0x0404, Level::Unknown};
    static constexpr EventStructItem UsbMctpRecvError              = {0x0405, Level::Unknown};
    static constexpr EventStructItem UsbHidWriteError              = {0x0406, Level::Unknown};
    static constexpr EventStructItem UsbHidRecvError               = {0x0407, Level::Unknown};
    static constexpr EventStructItem UsbItemSizeMismatchBufferSize = {0x0408, Level::Unknown};
    static constexpr EventStructItem UsbUpdateRoutingTableFailed   = {0x0409, Level::Unknown};
    static constexpr EventStructItem UsbClearQueueFailed           = {0x040a, Level::Unknown};
    static constexpr EventStructItem UsbRequestQueueError          = {0x040b, Level::Unknown};
    static constexpr EventStructItem UsbBufferOverflow             = {0x040c, Level::Unknown};
    static constexpr EventStructItem UsbDriverInitFailed           = {0x040d, Level::Unknown};
    static constexpr EventStructItem UsbSpiQueueRecvError          = {0x040e, Level::Unknown};
    static constexpr EventStructItem UsbSpiQueueSendError          = {0x040f, Level::Unknown};
    static constexpr EventStructItem UsbSpiWriteError              = {0x0410, Level::Unknown};
    static constexpr EventStructItem UsbSpiRecvError               = {0x0411, Level::Unknown};
    static constexpr EventStructItem UsbHidQueueRecvError          = {0x0412, Level::Unknown};
    // I2C
    static constexpr EventStructItem I2CBind           = {0x0500, Level::Info};
    static constexpr EventStructItem I2CError          = {0x0501, Level::Error};
    static constexpr EventStructItem I2CTempSensor     = {0x0502, Level::Info};
    static constexpr EventStructItem I2CQueueFail      = {0x0503, Level::Error};
    static constexpr EventStructItem I2CPktDrop        = {0x0504, Level::Error};
    static constexpr EventStructItem I2CForwardFail    = {0x0505, Level::Error};
    static constexpr EventStructItem I2CSetEventFail   = {0x0506, Level::Error};
    static constexpr EventStructItem I2CApStatusUpdate = {0x0507, Level::Info};
    static constexpr EventStructItem I2CStateInvalid   = {0x0508, Level::Error};
    // I3C
    static constexpr EventStructItem I3CBind               = {0x0600, Level::Info};
    static constexpr EventStructItem I3CFailedToResetDaa   = {0x0601, Level::Error};
    static constexpr EventStructItem I3CFailedToProcessDaa = {0x0602, Level::Error};
    static constexpr EventStructItem I3CError              = {0x0603, Level::Error};
    static constexpr EventStructItem I3CWriteFail          = {0x0604, Level::Error};
    static constexpr EventStructItem I3CReadFail           = {0x0605, Level::Error};
    static constexpr EventStructItem I3CSetAddr            = {0x0606, Level::Info};
    static constexpr EventStructItem I3COobReset           = {0x0607, Level::Info};
    static constexpr EventStructItem I3CUnhandledError     = {0x0608, Level::Error};
    static constexpr EventStructItem I3CCccError           = {0x0609, Level::Error};
    static constexpr EventStructItem I3CInit               = {0x060a, Level::Info};
    static constexpr EventStructItem I3CUnknownIBI         = {0x060b, Level::Error};
    static constexpr EventStructItem I3CPecInvalid         = {0x060c, Level::Error};
    static constexpr EventStructItem I3CTimeout            = {0x060d, Level::Error};
    static constexpr EventStructItem I3CNack               = {0x060e, Level::Error};
    static constexpr EventStructItem I3CIbiDrop            = {0x060f, Level::Error};
    static constexpr EventStructItem I3CQueueDrop          = {0x0610, Level::Error};
    static constexpr EventStructItem I3CApStatusUpdate     = {0x0611, Level::Info};
    static constexpr EventStructItem I3CI2CWriteFail       = {0x0612, Level::Error};
    static constexpr EventStructItem I3CI2CReadFail        = {0x0613, Level::Error};
    static constexpr EventStructItem I3CGpuI2cAddr         = {0x0614, Level::Info};
    static constexpr EventStructItem I3cDriverInit         = {0x0615, Level::Info};
    // MCTP
    static constexpr EventStructItem MctpApPgoodEventTrigger         = {0x0700, Level::Info};
    static constexpr EventStructItem MctpApPgoodGpioState            = {0x0701, Level::Info};
    static constexpr EventStructItem MctpEndpointState               = {0x0702, Level::Info};
    static constexpr EventStructItem MctpEnumerateResult             = {0x0703, Level::Info};
    static constexpr EventStructItem MctpDiscoveryNotify             = {0x0704, Level::Info};
    static constexpr EventStructItem MctpEnumerated                  = {0x0705, Level::Info};
    static constexpr EventStructItem MctpUuid                        = {0x0706, Level::Info};
    static constexpr EventStructItem MctpBackgroundCopySetup         = {0x0707, Level::Info};
    static constexpr EventStructItem MctpEnumerateSetEid             = {0x0708, Level::Info};
    static constexpr EventStructItem MctpNsmEventNotEnable           = {0x0709, Level::Info};
    static constexpr EventStructItem MctpSetEidReject                = {0x070a, Level::Info};
    static constexpr EventStructItem MctpMcuActAsBridgePacketDrop    = {0x070b, Level::Info};
    static constexpr EventStructItem MctpMcuActAsBridgePacketNotify  = {0x070c, Level::Info};
    static constexpr EventStructItem MctpNsmEventSettingNotMatch     = {0x0710, Level::Info};
    static constexpr EventStructItem MctpNsmRevokeKey                = {0x0711, Level::Info};
    static constexpr EventStructItem MctpNsmRevokeRollbackProtection = {0x0712, Level::Info};
    static constexpr EventStructItem MctpDumpPacket                  = {0x0713, Level::Info};
    static constexpr EventStructItem MctpRouterQueueFail             = {0x0714, Level::Error};
    static constexpr EventStructItem MctpRecvSetEid                  = {0x0715, Level::Info};
    static constexpr EventStructItem MctpInvalidInterface            = {0x0716, Level::Error};
    static constexpr EventStructItem MctpRoutingEntryNotFound        = {0x0717, Level::Error};
    static constexpr EventStructItem MctpInvalidEidDifference        = {0x0718, Level::Error};
    static constexpr EventStructItem MctpProtocolResetStart          = {0x0719, Level::Info};
    static constexpr EventStructItem MctpProtocolResetEnd            = {0x0720, Level::Info};

    // PLDM
    static constexpr EventStructItem PldmError                     = {0x0800, Level::Error};
    static constexpr EventStructItem PldmChangeState               = {0x0801, Level::Info};
    static constexpr EventStructItem PldmTransferComplete          = {0x0802, Level::Info};
    static constexpr EventStructItem PldmAuth                      = {0x0803, Level::Info};
    static constexpr EventStructItem PldmActivate                  = {0x0804, Level::Info};
    static constexpr EventStructItem PldmTimeout                   = {0x0805, Level::Info};
    static constexpr EventStructItem PldmTotalRetry                = {0x0806, Level::Info};
    static constexpr EventStructItem PldmCancel                    = {0x0807, Level::Info};
    static constexpr EventStructItem PldmM0Time                    = {0x0808, Level::Info};
    static constexpr EventStructItem PldmM1Time                    = {0x0809, Level::Info};
    static constexpr EventStructItem PldmUpdateOffset              = {0x080a, Level::Info};
    static constexpr EventStructItem PldmIdleReason                = {0x080b, Level::Info};
    static constexpr EventStructItem PldmStageUpdate               = {0x080c, Level::Info};
    static constexpr EventStructItem BackgroundCopyInitSetup       = {0x080d, Level::Info};
    static constexpr EventStructItem PldmAuthInactiveCryptoStatus  = {0x080e, Level::Info};
    static constexpr EventStructItem PldmMetadataCrossTransferSize = {0x080f, Level::Error};
    static constexpr EventStructItem PldmProtocolResetStart        = {0x0810, Level::Info};
    static constexpr EventStructItem PldmProtocolResetEnd          = {0x0811, Level::Info};
    // SPDM
    static constexpr EventStructItem SpdmError               = {0x0900, Level::Error};
    static constexpr EventStructItem SpdmDevIkGenerateFail   = {0x0901, Level::Error};
    static constexpr EventStructItem SpdmDevAkGenerateFail   = {0x0902, Level::Error};
    static constexpr EventStructItem SpdmSendToMctpFail      = {0x0903, Level::Error};
    static constexpr EventStructItem SpdmReceiveFromMctpFail = {0x0904, Level::Error};
    static constexpr EventStructItem SpdmL3CertGenerateFail  = {0x0905, Level::Error};
    static constexpr EventStructItem SpdmLockFuseBlock       = {0x0906, Level::Unknown};
    static constexpr EventStructItem SpdmCertReady           = {0x0907, Level::Error};
    static constexpr EventStructItem SpdmCertDdaOtpValue     = {0x0908, Level::Info};
    static constexpr EventStructItem SpdmCryptoApAuthResult  = {0x0909, Level::Info};
    static constexpr EventStructItem SpdmApAuthResult        = {0x0910, Level::Info};

    // Flash
    static constexpr EventStructItem FlashReqError = {0x0a00, Level::Unknown};
    // SPI
    static constexpr EventStructItem SpiBind              = {0x0b00, Level::Info};
    static constexpr EventStructItem SpiError             = {0x0b01, Level::Error};
    static constexpr EventStructItem SpiMidError          = {0x0b02, Level::Error};
    static constexpr EventStructItem SpiTimeout           = {0x0b03, Level::Error};
    static constexpr EventStructItem SpiQueueFail         = {0x0b04, Level::Error};
    static constexpr EventStructItem SpiCannotSendToUsb   = {0x0b05, Level::Error};
    static constexpr EventStructItem SpiCannotSendToMctp  = {0x0b06, Level::Error};
    static constexpr EventStructItem SpiFlashromQueueFail = {0x0b07, Level::Error};

    // DebugToken
    static constexpr EventStructItem DtInstallSuccess = {0x0c00, Level::Unknown};
    static constexpr EventStructItem DtEraseSuccess   = {0x0c01, Level::Unknown};
    static constexpr EventStructItem DtVerifyStatus   = {0x0c02, Level::Unknown};
    static constexpr EventStructItem DtAuthTokenFail  = {0x0c03, Level::Unknown};

    // Perf
    static constexpr EventStructItem PerfI3c            = {0x0d00, Level::Info};
    static constexpr EventStructItem PerfTimestamp      = {0x0d01, Level::Info};
    static constexpr EventStructItem PerfCpuMeasurement = {0x0d02, Level::Info};
    static constexpr EventStructItem PerfInterfaceType  = {0x0d03, Level::Info};
    static constexpr EventStructItem PerfInterfaceRecv  = {0x0d04, Level::Info};
    static constexpr EventStructItem PerfInterfaceSend  = {0x0d05, Level::Info};

    // NSM Messages
    static constexpr EventStructItem NsmLogMessages = {0x0e00, Level::Info};
    // NSM T5 EI
    static constexpr EventStructItem T5ActivateMcuException    = {0x0e01, Level::Info};
    static constexpr EventStructItem T5ActivateWatchdogTimeout = {0x0e02, Level::Info};
    // NSM T3 cx8 sensor debug (default: disabled in nsm_type_3.cpp)
    static constexpr EventStructItem T3Cx8TemperatureDebug = {0x0e03, Level::Debug};
    static constexpr EventStructItem T3I2cSensorNotFound   = {0x0e04, Level::Error};
    static constexpr EventStructItem T3I2cSensorFound      = {0x0e05, Level::Info};

    // NBU NVL topology
    static constexpr EventStructItem FruSuccess           = {0x0F00, Level::Info};
    static constexpr EventStructItem FruI2cBusy           = {0x0F01, Level::Error};
    static constexpr EventStructItem FruI2cNack           = {0x0F02, Level::Error};
    static constexpr EventStructItem FruI2cTimeout        = {0x0F03, Level::Error};
    static constexpr EventStructItem FruI2cError          = {0x0F04, Level::Error};
    static constexpr EventStructItem FruParseError        = {0x0F05, Level::Error};
    static constexpr EventStructItem FruChecksumError     = {0x0F06, Level::Error};
    static constexpr EventStructItem FruNoChassisArea     = {0x0F07, Level::Error};
    static constexpr EventStructItem FruInvalidData       = {0x0F08, Level::Error};
    static constexpr EventStructItem FruPartNumberError   = {0x0F09, Level::Error};
    static constexpr EventStructItem FruSerialError       = {0x0F0A, Level::Error};
    static constexpr EventStructItem FruCustomFieldError  = {0x0F0B, Level::Error};
    static constexpr EventStructItem FruTlvDecodeError    = {0x0F0C, Level::Error};
    static constexpr EventStructItem SgpioCallback        = {0x0F0D, Level::Info};
    static constexpr EventStructItem SgpioStartStatus     = {0x0F0E, Level::Info};
    static constexpr EventStructItem SgpioInitFail        = {0x0F0F, Level::Error};
    static constexpr EventStructItem SgpioGetTopologyFail = {0x0F10, Level::Error};

    // Voltage Monitor / Leak Detection
    static constexpr EventStructItem LeakDetectIsrNoValidReading  = {0x1000, Level::Info};
    static constexpr EventStructItem LeakDetectIsrNoValidSensorId = {0x1001, Level::Info};

    Event()                        = delete;
    Event(const Event&)            = delete;
    Event& operator=(const Event&) = delete;
};

enum class LogEvent : uint32_t
{
    Begin  = 0,
    AddLog = 1,
    AddLogBlocking,
    DownloadLog,
    CleanLog,
    DropLog,
    AddLogISR,
    WdtEvent,
};

enum EventBits : uint32_t
{
    None                = 0,
    AddLogEvent         = nv::common::bit(LogEvent::AddLog),
    AddLogBlockingEvent = nv::common::bit(LogEvent::AddLogBlocking),
    DownloadLogEvent    = nv::common::bit(LogEvent::DownloadLog),
    CleanLogEvent       = nv::common::bit(LogEvent::CleanLog),
    DropLogEvent        = nv::common::bit(LogEvent::DropLog),
    AddLogISREvent      = nv::common::bit(LogEvent::AddLogISR),
    WdtEventEvent       = nv::common::bit(LogEvent::WdtEvent)
};

enum class OutputDirection : uint8_t
{
    None    = 0x0,
    Console = 0x1,
    Flash   = 0x2,
    Both    = 0x3,
};

enum class MemorySource : uint8_t
{
    Metadata = 0,
    LogEntry,
    FaultEntry,
};

struct [[gnu::packed]] Entry
{
    uint32_t  timestamp;          // 4
    Level     level         : 3;  // 3 bits level (LSB)
    uint8_t   footprint_msb : 5;  // 5 bits footprint MSB (MSB)
    EventId   event;              // 2
    uint8_t   footprint_lsb : 7;  // 7 bits footprint LSB (LSB)
    uint8_t   coreId        : 1;  // 1 bit for core ID (MSB)
    EventData data;               // 8

    std::span<uint8_t> to_span() const
    {
        return {std::bit_cast<uint8_t*>(this), sizeof(*this)};
    }
};
static_assert(sizeof(Entry) == nv::flash::PhraseSize,
              "Log Entry size not equals to PhraseSize");

// Footprint utilities: compute a 12-bit id from file and function
// and helpers to split/pack across Entry.footprint_msb (5 bits) and Entry.footprint_lsb (7
// bits).
namespace footprint {
using Id                      = uint16_t;
constexpr uint64_t kFnvOffset = 14695981039346656037ull;
constexpr uint64_t kFnvPrime  = 1099511628211ull;

constexpr uint8_t pack_level_bits(Level base_level, uint8_t upper5_bits)
{
    const uint8_t base = static_cast<uint8_t>(base_level) & 0x07u;  // keep 3 LSBs for real
                                                                    // level
    // Ensure upper5_bits is constrained to 5 bits before shifting to prevent overflow
    const uint8_t constrained_upper5 = upper5_bits & 0x1Fu;
    return static_cast<uint8_t>((static_cast<uint8_t>(constrained_upper5 << 3)) | base);
}

// Extract simple function name from std::source_location::function_name()
// The full function name is difficult to generate by a offline script.
constexpr const char* extract_simple_function_name(const char* full_func_name)
{
    if (!full_func_name) {
        return full_func_name;
    }

    // Find the opening parenthesis first
    const char* paren_pos = nullptr;
    for (auto p = full_func_name; *p != '\0'; ++p) {
        if (*p == '(') {
            paren_pos = p;
            break;
        }
    }

    // std::source_location::function_name() should always have '('
    // If not found, something is wrong - just return as-is
    if (!paren_pos) {
        return full_func_name;
    }

    // Find the last occurrence of "::" before the opening parenthesis
    const char* last_scope = nullptr;
    for (auto p = full_func_name; p < paren_pos; ++p) {
        if (*p == ':' && *(p + 1) == ':' && (p + 2) < paren_pos) {
            last_scope = p + 2;  // Point to character after "::"
        }
    }

    // If no "::" found before '(', start from the beginning
    return last_scope ? last_scope : full_func_name;
}

// constexpr function to compute footprint id
constexpr Id filefunc12(const std::source_location& loc)
{
    const auto& file = loc.file_name();
    const auto& func = loc.function_name();

    // Constants for hash function
    constexpr uint8_t  kHashSeparator = 0xFFu;    // Separator between file and function hash
    constexpr uint16_t kHash12BitMask = 0x0FFFu;  // 12-bit mask for final hash result

    // Hash file path directly (normalize backslashes to forward slashes)
    uint64_t h = kFnvOffset;
    for (auto p = file; *p != 0; ++p) {
        const auto c = static_cast<unsigned char>(*p == '\\' ? '/' : *p);

        h ^= c;
        // coverity[cert_int30_c_violation] Allow overflow - this is an expected behavior
        h *= kFnvPrime;
    }
    h ^= kHashSeparator;
    // coverity[cert_int30_c_violation] Allow overflow - this is an expected behavior
    h *= kFnvPrime;
    // Hash simple function name
    const auto simple_func = extract_simple_function_name(func);
    // hash simple function name stops before '('
    for (auto p = simple_func; *p != 0 && *p != '(' && *p != ' '; ++p) {
        h ^= static_cast<unsigned char>(*p);
        // coverity[cert_int30_c_violation] Allow overflow - this is an expected behavior
        h *= kFnvPrime;
    }
    return static_cast<Id>(h & kHash12BitMask);
}

}  // namespace footprint

constexpr auto FlashSize = 0xC000;

constexpr uint32_t PageSize         = nv::flash::BufferSize;
constexpr uint32_t PhraseSize       = nv::flash::PhraseSize;
constexpr uint32_t EntrySize        = sizeof(Entry);
constexpr uint32_t EntryNumInPage   = (PhraseSize / EntrySize);
constexpr uint32_t EntryNumInSector = (nv::flash::SectorSize / EntrySize);
constexpr uint32_t EntryNumInFlash  = (FlashSize / EntrySize);

constexpr uint32_t EntryNumInPharse = (PhraseSize / EntrySize);

constexpr uint32_t DownloadSize = PhraseSize;

constexpr uint32_t LogThreshold = 1;

constexpr uint8_t LogSessionMax = 0xFF;

struct [[gnu::packed]] LogPtr
{
    uint16_t           head;           ///< Point to head
    uint16_t           tail;           ///< Point to the next free log
    uint16_t           head_checksum;  ///< ~ head
    uint16_t           tail_checksum;  ///< ~ tail
    uint8_t            padding[8];
    std::span<uint8_t> to_span() const
    {
        return {std::bit_cast<uint8_t*>(this), sizeof(*this)};
    }
};

static constexpr uint32_t LogPtrSize = uint32_t(sizeof(LogPtr));
static_assert(LogPtrSize == nv::flash::PhraseSize, "Log ptr size not equals to PhraseSize");

constexpr uint32_t PtrNumInPhrase      = (PhraseSize / LogPtrSize);
constexpr uint32_t LogMetaMaxIndex     = (nv::flash::SectorSize / LogPtrSize);
constexpr uint32_t LogMetaInvalidIndex = 0xFFFFFFFF;
constexpr uint32_t PtrNumInPage        = (PageSize / LogPtrSize);

// TBD: remove buffer if phrase program is deem to be used
struct [[gnu::packed]] EntryBuffer
{
    std::array<Entry, EntryNumInPage> entries;

    std::span<uint8_t> to_span() const
    {
        return {std::bit_cast<uint8_t*>(this), sizeof(*this)};
    }
};
static_assert(sizeof(EntryBuffer) == PhraseSize, "EntryBuffer size not equals to PhraseSize");

struct [[gnu::packed]] PtrBuffer
{
    LogPtr ptrs[PtrNumInPhrase];

    std::span<uint8_t> to_span() const
    {
        return {std::bit_cast<uint8_t*>(this), sizeof(*this)};
    }
};
static_assert(sizeof(PtrBuffer) == PhraseSize, "EntryBuffer size not equals to PhraseSize");

struct LogBuffer
{
    uint16_t    index{};  ///< index in metadata
    LogPtr      ptr{};
    PtrBuffer   ptr_buffer{};
    EntryBuffer entry_buffer{};
};

struct [[gnu::packed]] Item
{
    uint32_t        timestamp{};
    OutputDirection direction{};
    EventId         event{};
    Level           level         : 3;  // 3 bits level (LSB)
    uint8_t         footprint_msb : 5;  // 5 bits footprint MSB (MSB)
    EventData       data{};             // 8 bytes
    Status          status{};           // 2 bytes (uint16_t)
    uint8_t         footprint_lsb{};    // footprint 7bitsLSB here in Item.
    uint8_t         core_id{};          // Store originating core ID (1 byte) - moved to end
    bool            wait{};
    bool            flag{};
};

struct AsciiArr
{
    std::array<uint8_t, sizeof(Item) - 2> ascii_arr{};  // -2 for core_id, flag
    uint8_t                               core_id{};    // Store core ID
    bool                                  flag;         // ASCII marker
};

constexpr uint32_t AsciiStrLen = sizeof(AsciiArr{}.ascii_arr);

static_assert(sizeof(Item) == sizeof(AsciiArr), "Item size not match");

struct [[gnu::packed]] Dlreq
{
    uint8_t                           session;
    std::array<uint8_t, DownloadSize> data;
    uint8_t                           size;
    Status                            status;
    uint16_t                          reserved;
};

enum class LogDLState : uint8_t
{
    Start,
    DownloadEvent,
    DownloadFatal,
    DownloadPerf,
    End,
};

struct DownloadSession
{
    uint8_t           session;
    LogDLState        state;
    uint32_t          ptr;
    uint32_t          event_tail;
    uint16_t          event_size;
    uint16_t          fatal_size;
    uint16_t          perf_size;
    uint32_t          buffer_ptr;
    uint32_t          current_page;
    uint32_t          perf_offset;
    nv::flash::Buffer buffer;
};

struct [[gnu::packed]] LogDLHdr
{
    uint16_t event_size;
    uint16_t fatal_size;
    uint16_t perf_size;
    uint16_t major_version;
    uint8_t  reserve[7];
    uint8_t  version;
};
static_assert(sizeof(LogDLHdr) == PhraseSize, "Size of LogDLHdr invalid");

struct [[gnu::packed]] FwVersion
{
    uint16_t           major;
    uint8_t            minor;
    uint16_t           patch;
    uint16_t           build;
    std::span<uint8_t> to_span() const
    {
        return {std::bit_cast<uint8_t*>(this), sizeof(*this)};
    }
};
static_assert(sizeof(FwVersion) == 7, "Size of FwVersion invalid");

constexpr uint8_t  DumpHeadMagic    = 0x5A;
constexpr uint8_t  DumpTailMagic    = 0xA5;
constexpr uint8_t  DumpHeadMagicRaw = 0xAF;
constexpr uint8_t  DumpTailMagicRaw = 0xFA;
constexpr uint8_t  DumpCheckSize    = 2;
constexpr uint32_t DumpBufferSize   = sizeof(nv::logger::Entry) + DumpCheckSize;

}  // namespace nv::logger

/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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

#include <bit>
#include <cstdint>
#include <cstring>
#include <span>

#include "mbedtls/ctr_drbg.h"

#include "nv/gpio/driver.h"
#include "nv/i2c/powersensor/device_manager.h"
#include "nv/ipc/supervisor.h"
#include "nv/mctp/constants.h"
#include "nv/mctp/control.h"
#include "nv/mctp/enums.h"
#include "nv/mctp/interface.h"
#include "nv/mctp/nsm_event.h"
#include "nv/mctp/nsm_msg_bitmask.h"
#include "nv/mctp/nsm_type_2.h"
#include "nv/mctp/nsm_type_3.h"
#include "nv/mctp/nsm_type_4.h"
#include "nv/mctp/nsm_type_5.h"
#include "nv/mctp/nsm_type_ff.h"
#include "sys/common/error-inject.h"

// Forward declarations to reduce compile size
namespace nv {
namespace debugtoken {
enum class TokenErrorCode : uint16_t;
}
}  // namespace nv

namespace nv::mctp {

constexpr uint16_t NvMctpPciVendorId             = 0xDE10;  // MCTP header is big endian
constexpr uint16_t NvMctpFwComponentClass        = 0x000A;
constexpr uint8_t  NvMctpDeviceMcuId             = 0x05;
constexpr uint32_t NvMctpIrreversibleCtrlTimeout = 24000;
constexpr size_t   NvMctpNsmNonceSize            = 8;
constexpr size_t   NvMctpVersionLength           = 17;
constexpr uint8_t  NvPldmMcuComponentIdPrefix    = 0xFF;
constexpr uint8_t  TelemetryCount                = 29;
constexpr uint8_t  SlotSegmentTelemetryCount     = 9;
constexpr uint32_t ByteShift1                    = 8;
constexpr uint32_t ByteShift2                    = 16;
constexpr uint32_t ByteShift3                    = 24;
constexpr uint32_t KeyIndexMax = static_cast<uint32_t>(std::numeric_limits<uint32_t>::digits);

constexpr size_t power_of_two(size_t input_len)
{
    if (input_len > 7) {
        return 0;
    }
    else {
        return (1U << input_len);
    }
}

struct [[gnu::packed]] NvMsgTypeWithEventBitmask
{
    NsmMsgType                                            nv_msg_type;
    std::array<uint8_t, nsm_msg::NvMctpEventSupportedNum> bitmask;
};

struct [[gnu::packed]] EventSubscription
{
    NsmGlobalEventSetting setting;
    uint8_t               endpoint_id;
};

struct [[gnu::packed]] EventLog
{
    NsmMsgType nv_msg_type;
    uint8_t    event_version;  // TODO - event payload version
    uint8_t    event_id;
    uint8_t    event_class;
    uint16_t   event_state;
    uint8_t    data_size;  // event record log is uint16_t but message is uint8_t
    uint8_t    data[1];    // TODO - data size need to be decide
};

struct [[gnu::packed]] Nonce
{
    uint8_t nonce[NvMctpNsmNonceSize];
};

struct [[gnu::packed]] FwCompInfo
{
    uint16_t component_class;
    uint16_t component_id;
    uint8_t  component_index;
};

template<size_t N>
struct [[gnu::packed]] FwCompIds
{
    uint8_t    component_count;
    FwCompInfo components[N];

    constexpr FwCompIds(std::array<FwCompInfo, N> init_array) : component_count(N)
    {
        std::copy(init_array.begin(), init_array.end(), components);
    }
};

// updateAuthKey Request data 16 bytes
// - 1 byte: Request type 0
// - 2 byte: Component Classification 2
// - 2 byte: Component Identifier 4
// - 1 byte: Component Classification Index 5
// - 8 byte: Nonce 6 ~ 13
// - 1 byte: Permission Bitmap Length 14
// - 4 byte: Requested Permission Bitmap 15

struct [[gnu::packed]] UpdateAuthKeyReq
{
    uint8_t    request_type;
    FwCompInfo fw_comp_info;
    Nonce      nonce;
    uint8_t    bitmap_len;
    uint32_t   permission_bitmap;
};

// updateMinSecVerNum Request data 16 bytes
// - 1 byte: Request type 0
// - 2 byte: Component Classification 1 ~ 2
// - 2 byte: Component Identifier 3 ~ 4
// - 1 byte: Component Classification Index 5
// - 8 byte: Nonce 6 ~ 13
// - 2 byte: Requested Minimum Security Version Number 14 ~ 15

struct [[gnu::packed]] UpdateMinSvnReq
{
    uint8_t    request_type;
    FwCompInfo fw_comp_info;
    Nonce      nonce;
    uint16_t   min_svn;
};

// queryAuthKey Response data 9 bytes
// - 2 bytes: Active Component Key Index 0 ~ 1
// - 2 bytes: Pending Component Key Index 2 ~ 3
// - 1 byte: Permission Bitmap Length 4
// - 1 byte: Active Component Key Permission Bitmap 5
// - 1 byte: Pending Component Key Permission Bitmap 6
// - 1 byte: EFUSE Key Permission Bitmap 7
// - 1 byte: Pending EFUSE Key Permission Bitmap 8

struct [[gnu::packed]] QueryAuthKeyResp
{
    uint16_t active_comp_key;
    uint16_t pending_comp_key;
    uint8_t  bitmap_len;
    uint32_t active_permission;
    uint32_t pending_permission;
    uint32_t active_efuse_permission;
    uint32_t pending_efuse_permission;
};

// querySecVerNum Response data
// - 2 bytes: Active Component Security Version Number
// - 2 bytes: Pending Component Security Version Number
// - 2 bytes: Minimum Security Version Number, MIN_SVN stored in EFUSE
// - 2 bytes: Pending Minimum Security Version Number, If no pending, set to 0.
struct [[gnu::packed]] QuerySvnResp
{
    uint16_t active_svn;
    uint16_t pending_svn;
    uint16_t min_svn;
    uint16_t pending_min_svn;
};

// setRotProperty Request data 6 bytes
// - 2 byte: Component Classification
// - 2 byte: Component Identifier
// - 1 byte: Component Classification Index
// - 1 byte: Property
// - (TBD) MCU & CPLD do not support any property

struct [[gnu::packed]] SetRotPropertyReq
{
    FwCompInfo               fw_comp_info;
    NsmSetRotPropertyRequest property;
};

// ImageCopyControl Request data 7 bytes
// - 1 byte: Request type
// - 1 byte: Component Count
// - 2 byte: Component Classification
// - 2 byte: Component Identifier
// - 1 byte: Component Classification Index
// - MCU only support 1 component for now

struct [[gnu::packed]] ImageCopyControlReq
{
    NsmImageCopyControlRequest request;
    uint8_t                    component_count;
    FwCompInfo                 fw_comp_info[];
};

// QueryImageCopyProgressResp Request data 2 bytes
// - 1 byte: status
// - 1 byte: progress

struct [[gnu::packed]] QueryImageCopyProgressResp
{
    NsmImageCopyStatus status;
    uint8_t            progress;
};

constexpr uint16_t GpioBytes = (nv::ipc::GpioNum + 7) / 8;

struct [[gnu::packed]] Type0GetGpioReq
{
    uint16_t offset;
    uint16_t length;
};

struct [[gnu::packed]] Type0SetGpioReq
{
    uint16_t                       offset;
    uint16_t                       length;
    std::array<uint8_t, GpioBytes> gpio;  // 6 instances * (32 pins / 8 bits)
};

struct [[gnu::packed]] Type0GpioResp
{
    uint16_t                       offset;
    uint16_t                       length;
    std::array<uint8_t, GpioBytes> gpio;  // 6 instances * (32 pins / 8 bits)
};

// it is for Get Device Diagnostics
struct [[gnu::packed]] Type4GpioResp
{
    uint8_t                        gpioNum;
    std::array<uint8_t, GpioBytes> gpio;  // 6 instances * (32 pins / 8 bits)
};

// Type4 Debug Token error response
struct [[gnu::packed]] NsmPktRespType4Error : Header
{
    /* nvidia OEM binding */
    MsgType msg_type;

    uint16_t pci_vendor_id;

    uint8_t instance_id : 5;
    uint8_t rsvd0       : 1;
    uint8_t d           : 1;
    uint8_t rq          : 1;

    uint8_t ocp_version : 3;
    uint8_t ocp_type    : 4;
    uint8_t ocp         : 1;

    NsmMsgType nv_msg_type;

    union CmdCode
    {
        NsmType4CmdCode type4_code;
    } cmd_code;

    Ccode    completion_code;
    uint16_t error_code;  // TokenErrorCode directly after completion_code

    static NsmPktRespType4Error& from(Packet& buf)
    {
        return *std::bit_cast<NsmPktRespType4Error*>(&buf.hdr);
    }

    static const NsmPktRespType4Error& from(const Packet& buf)
    {
        return *std::bit_cast<const NsmPktRespType4Error*>(&buf.hdr);
    }

    void set_type4_code(NsmType4CmdCode code) { cmd_code.type4_code = code; }
};

struct [[gnu::packed]] TelemetryMultiBytes
{
    uint8_t  tag;
    uint8_t  v              : 1;  // defaults to 1
    uint8_t  encoded_length : 3;
    uint8_t  rsvd           : 3;
    uint8_t  b              : 1;
    uint16_t multi_bytes_length;

    TelemetryMultiBytes(uint8_t id = 0)
    : tag{id}
    , v{1}
    , encoded_length{0}
    , rsvd{0}
    , b{0}
    , multi_bytes_length{0}
    {
        // Empty
    }

    /**
     * @returns the total size of the structure
     */
    // coverity[routine_not_emitted]
    uint16_t size() const { return sizeof(*this) + this->multi_bytes_length; }
};

template<size_t Sizes>  // coverity[entity_not_emitted]
struct [[gnu::packed]] TelemetryRecord
{
    uint8_t                    tag;
    uint8_t                    v      : 1;  // defaults to 1
    uint8_t                    length : 3;
    uint8_t                    rsvd   : 3;
    uint8_t                    b      : 1;  // length format
    std::array<uint8_t, Sizes> data;

    /**
     * The creator sets length and v, tag assumes 0 when omitted
     * @param id
     */
    // coverity[routine_not_emitted]
    TelemetryRecord(uint8_t id = 0) : tag{id}, v{1}, length{0}, rsvd{0}, b{0}, data{0}
    {
        set_length(sizeof(data));
    }

    /**
     * Sets the length properly according to @a sizes parameter
     *  Also it considers the possibility of sizes being an odd values
     */
    // coverity[routine_not_emitted]
    void set_length(int size)
    {
        this->length = 0;
        if (size > 0) {
            --size;
            while (size > 0) {
                this->length++;
                size >>= 1;
            }
        }
    }

    /**
     * Useful for generic pointers regardless Data size
     * @returns the total size of the structure
     */
    // coverity[routine_not_emitted]
    uint16_t size() const
    {
        return sizeof(*this) - sizeof(this->data)
             + power_of_two(static_cast<size_t>(this->length));
    }

    /**
     * @brief setValue() - Sets the @a value in TelemetryRecord::data
     *
     * @param value - A variable (not constant) with the new value
     *                The variable size MUST match with TelemetryRecord::data
     *                Values 16/32/64 bits are stored in Little-endian
     *
     * @return true if sizes match and it was possible to copy data
     */
    template<typename DataValue>  // coverity[routine_not_emitted]
    bool setValue(const DataValue& value, const bool valid = true)
    {
        if (sizeof(DataValue) != Sizes) {
            this->v = 0;
            return false;
        }
        if constexpr (std::endian::native == std::endian::big) {
            // Big-endian system, convert to Little-endian
            DataValue swapValue = byteswap(value);
            std::memcpy(&this->data[0], &swapValue, sizeof(DataValue));
        }
        else {
            std::memcpy(&this->data[0], &value, sizeof(DataValue));
        }
        this->setValid(valid);
        return true;
    }

    template<typename DataValue>  // coverity[routine_not_emitted]
    bool populate(const uint8_t tag, const DataValue& value, const bool valid = true)
    {
        bool ret = setValue(value, valid);
        if (ret) {
            this->tag = tag;
        }
        return ret;
    }

    /**
     * @brief getValue
     * @param value will have the latest stored value by @sa setValue()
     * @return true if @a value size matches with TelemetryRecord::data
     *         and a value could be set into @a value variable
     */
    template<typename DataValue>  // coverity[routine_not_emitted]
    bool getValue(DataValue& value)
    {
        if (sizeof(DataValue) != Sizes) {
            return false;
        }
        if constexpr (std::endian::native == std::endian::big) {
            // values are stored in Little-endian, convert to Big-endian
            DataValue swapValue = byteswap(value);
            std::memcpy(&value, &swapValue, sizeof(DataValue));
        }
        else {
            std::memcpy(&value, &this->data[0], sizeof(DataValue));
        }
        return true;
    }

    /**
     * @brief setTimeStampValue() - Sets current timestamp in TelemetryRecord::data
     * @return true if the struct was created as size of uint64_t
     */
    // coverity[routine_not_emitted]
    bool setTimeStampValue(const bool valid = true)
    {
        if (sizeof(uint64_t) != Sizes) {
            this->v = 0;
            return false;
        }
        // TODO change to the 64 bits timestamp when Glacier team done that API
        uint64_t timestamp = sys::ipc::get_os_ticks();
        return setValue(timestamp, valid);
    }

    /** @brief Changes the v flag true=1, false=0 */
    void setValid(const bool valid) { this->v = (valid == true) ? 1 : 0; }

    /**
     *
     * @returns The record address, suitable to use in std::memcpy()
     */
    // coverity[routine_not_emitted]
    const void* record() const { return this; }
};

using TelemetryRecordType        = TelemetryRecord<sizeof(uint8_t)>;
using TelemetryRecordPointer     = TelemetryRecordType*;
using TelemetryMultiBytesPointer = TelemetryMultiBytes*;

/**
 * @brief Builds an array of TelemetryRecord structs
 *        - Sizes availabe are:
 *          -# @sa addRecordNvU8()  => TelemetryRecord<1>
 *          -# @sa addRecordNvU16() => TelemetryRecord<2>
 *          -# @sa addRecordNvU32() => TelemetryRecord<4>
 *          -# @sa addRecordNvU64() => TelemetryRecord<8>
 *          -# @sa addTimestampRecord() => TelemetryRecord<8>
 *          -# @sa addRecordVariableArray(N <= 128) => TelemetryRecord<N>
 */
class TelemetryRecordArray
{
public:
    static constexpr uint16_t DefaultMctpDataSize = NsmPktBufDataLen - sizeof(nv::mctp::Header);
    // Maximum NSM Bulk Response message size
    static constexpr uint16_t
        MaxNsmBulkResponseSize = Constants::MctpTxBufSize - sizeof(nv::mctp::PrivateHeader)
                               - sizeof(nv::mctp::Header)
                               - Constants::NsmAggregateHeaderResponseSize;

public:
    /**
     * @brief This constructor uses an internal buffer limited to @a DefaultMctpDataSize
     */
    explicit TelemetryRecordArray()
    : _size{MaxNsmBulkResponseSize}
    , _offset{0}
    , _data{nullptr}
    , _elements{0}
    {
        // Empty
    }

    /**
     * @brief This constructor uses external buffer
     * @param data - Pointer to external buffer
     * @param size - Size of the buffer
     */
    explicit TelemetryRecordArray(uint8_t* data, uint16_t size)
    : _size{size}
    , _offset{0}
    , _data{data}
    , _elements{0}
    {
        // Empty
    }

    void setStorage(void* data, uint16_t size)
    {
        _size = size;
        _data = static_cast<uint8_t*>(data);
        rewind();
    }

    template<typename DataValue>
    bool addRecordNvU8(uint8_t tag, const DataValue& value, const bool valid = true)
    {
        TelemetryRecord<sizeof(uint8_t)> telemetry;
        return telemetry.populate(tag, value, valid)
            && copy_into_data(telemetry.record(), telemetry.size()) && ++_elements;
    }

    template<typename DataValue>
    bool addRecordNvU16(uint8_t tag, const DataValue& value, const bool valid = true)
    {
        TelemetryRecord<sizeof(uint16_t)> telemetry;
        return telemetry.populate(tag, value, valid)
            && copy_into_data(telemetry.record(), telemetry.size()) && ++_elements;
    }

    template<typename DataValue>
    bool addRecordNvU32(uint8_t tag, const DataValue& value, const bool valid = true)
    {
        TelemetryRecord<sizeof(uint32_t)> telemetry;
        return telemetry.populate(tag, value, valid)
            && copy_into_data(telemetry.record(), telemetry.size()) && ++_elements;
    }

    template<typename DataValue>
    bool addRecordNvU64(uint8_t tag, const DataValue& value, const bool valid = true)
    {
        TelemetryRecord<sizeof(uint64_t)> telemetry;
        return telemetry.populate(tag, value, valid)
            && copy_into_data(telemetry.record(), telemetry.size()) && ++_elements;
    }

    bool addTimestampRecord(uint8_t tag)
    {
        TelemetryRecord<sizeof(uint64_t)> telemetry(tag);
        return telemetry.setTimeStampValue()
            && copy_into_data(telemetry.record(), telemetry.size()) && ++_elements;
    }

    template<typename DataValue>
    bool addRecordVariableArray(uint8_t          tag,
                                const uint16_t   array_size,
                                const DataValue& value,
                                const bool       valid = true)
    {
        // b111 = 7 then max TelemetryRecord = 2**7 = 128
        constexpr uint16_t MaxArraySizeUsingThreeBits = 128;
        if (array_size > MaxArraySizeUsingThreeBits) {
            return false;
        }

        TelemetryRecord<1> telemetry(tag);
        telemetry.set_length(array_size);
        uint8_t validated = (valid == true) ? 1 : 0;

        // adjust array_size in case it is not a power of two, length is already correct
        // coverity[cert_int31_c_violation]
        uint16_t encoded_size_power_of_two = static_cast<uint16_t>(
            power_of_two(static_cast<size_t>(telemetry.length)));

        bool ret = false;
        if (encoded_size_power_of_two == array_size) {
            telemetry.v = validated;
            // copy only flags from telemetry, do not use telemetry.size() here
            ret = copy_into_data(telemetry.record(), sizeof(telemetry) - 1)
               && copy_into_data(&value, array_size) && ++_elements;
        }
        else {
            // create TelemetryMultiBytes
            TelemetryMultiBytes telemetry_multi_bytes{};
            telemetry_multi_bytes.tag                = tag;
            telemetry_multi_bytes.rsvd               = 0;
            telemetry_multi_bytes.v                  = validated;
            telemetry_multi_bytes.b                  = 1;  // indicate it is multi byte
            telemetry_multi_bytes.encoded_length     = 0;
            telemetry_multi_bytes.multi_bytes_length = array_size;
            ret = copy_into_data(&telemetry_multi_bytes, sizeof(telemetry_multi_bytes))
               && copy_into_data(&value, array_size) && ++_elements;
        }
        return ret;
    }

    template<typename DataValue>
    bool addRecordVariableArray(uint8_t          tag,
                                const size_t     array_size,
                                const DataValue& value,
                                const bool       valid = true)
    {
        // coverity[cert_int31_c_violation]
        auto short_size = static_cast<uint16_t>(array_size);
        return addRecordVariableArray(tag, short_size, value, valid);
    }

    void rewind()
    {
        _offset   = 0;
        _elements = 0;
    }

    void*    data() const { return _data; }
    uint16_t arraySize() const { return _offset; }
    uint16_t elements() const { return _elements; }

    TelemetryRecordPointer findTag(uint8_t tag)
    {
        void*    telemetry = _data;
        uint16_t offset    = 0;
        while (offset < _offset && telemetry != nullptr) {
            auto ptTelemetryRecord = static_cast<TelemetryRecordPointer>(telemetry);
            if (ptTelemetryRecord->tag == tag) {
                return ptTelemetryRecord;
            }
            if (ptTelemetryRecord->b == 1) {
                auto ptMultiBytesRecord  = static_cast<TelemetryMultiBytesPointer>(telemetry);
                offset                  += ptMultiBytesRecord->size();
            }
            else {
                offset += ptTelemetryRecord->size();
            }
            telemetry = (_data + offset);
        }
        return nullptr;
    }

    bool invalidateTelemetry(uint8_t tag)
    {
        auto telemetry = this->findTag(tag);
        if (telemetry == nullptr) {
            return false;
        }
        telemetry->setValid(false);
        return true;
    }

protected:
    bool is_there_enough_space(uint16_t required_space)
    {
        bool ret = _data != nullptr;
        if (ret) {
            if ((std::numeric_limits<uint16_t>::max() - _offset) > required_space) {
                ret = (required_space + _offset) <= _size;
            }
            else {
                ret = false;
            }
        }
        return ret;
    }

    bool copy_into_data(const void* telemetry, uint16_t telemetry_size)
    {
        if (is_there_enough_space(telemetry_size)) {
            // coverity[cert_ctr50_cpp_violation]
            std::memcpy(&_data[_offset], telemetry, telemetry_size);
            // coverity[cert_ctr50_cpp_violation]
            _offset += telemetry_size;
            return true;
        }
        return false;
    }

    bool clear_data_area(uint16_t size)
    {
        if (is_there_enough_space(size)) {
            // coverity[cert_ctr50_cpp_violation]
            std::memset(&_data[_offset], 0, size);
            return true;
        }
        return false;
    }

private:
    size_t   _size;
    uint16_t _offset;
    uint8_t* _data;
    uint16_t _elements;
};

class TelemetryRecordArrayBuffer : public TelemetryRecordArray
{
    std::array<uint8_t, TelemetryRecordArray::MaxNsmBulkResponseSize> _storage;

public:
    explicit TelemetryRecordArrayBuffer() : TelemetryRecordArray(), _storage{0}
    {
        TelemetryRecordArray::setStorage(_storage.data(), _storage.size());
    }
};

struct [[gnu::packed]] RespAggregate
{
    TelemetryRecord<power_of_two(RotTagLength::TagRedundancyPolicyPersistentLen)>
        tag_redundancy_policy_persistent;
    TelemetryRecord<power_of_two(RotTagLength::TagActiveFirmwareSlotLen)>
                                                                    tag_active_firmware_slot;
    TelemetryRecord<power_of_two(RotTagLength::TagActiveKeySetLen)> tag_active_key_set;
    TelemetryRecord<power_of_two(RotTagLength::TagMinSecurityVerNumLen)>
        tag_min_security_ver_num;
    TelemetryRecord<power_of_two(RotTagLength::TagInbandUpdatePolicyPersistentLen)>
        tag_inband_update_policy_persistent;
    TelemetryRecord<power_of_two(RotTagLength::TagBootStatusCodeLen)> tag_boot_status_code;
    TelemetryRecord<power_of_two(RotTagLength::TagRedundancyPolicyCurrentLen)>
        tag_redundancy_policy_current;
    TelemetryRecord<power_of_two(RotTagLength::TagInbandUpdatePolicyCurrentLen)>
                                                               tag_inband_update_policy_current;
    TelemetryRecord<power_of_two(RotTagLength::TagApSkuIdLen)> tag_ap_sku_id;
    TelemetryRecord<power_of_two(RotTagLength::TagGlobalFailoverPolicyLen)>
        tag_global_failover_policy;
    TelemetryRecord<power_of_two(RotTagLength::TagFirmwareSlotCountLen)>
        tag_firmware_slot_count;

    // slot 0
    TelemetryRecord<power_of_two(RotTagLength::TagFirmwareSlotIdLen)>
        tag_firmware_slot_id_slot0;
    TelemetryRecord<power_of_two(RotTagLength::TagFirmwareVerStringLen)>
        tag_firmware_ver_string_slot0;
    TelemetryRecord<power_of_two(RotTagLength::TagVerComparisonStampLen)>
                                                                 tag_ver_comparison_stamp_slot0;
    TelemetryRecord<power_of_two(RotTagLength::TagBuildTypeLen)> tag_build_type_slot0;
    TelemetryRecord<power_of_two(RotTagLength::TagSigningTypeLen)> tag_signing_type_slot0;
    TelemetryRecord<power_of_two(RotTagLength::TagWriteProtectStateLen)>
        tag_write_protect_state_slot0;
    TelemetryRecord<power_of_two(RotTagLength::TagFirmwareStateLen)> tag_firmware_state_slot0;
    TelemetryRecord<power_of_two(RotTagLength::TagSecurityVerNumLen)>
        tag_security_ver_num_slot0;
    TelemetryRecord<power_of_two(RotTagLength::TagSigningKeyIndexLen)>
        tag_signing_key_index_slot0;

    // slot 1
    TelemetryRecord<power_of_two(RotTagLength::TagFirmwareSlotIdLen)>
        tag_firmware_slot_id_slot1;
    TelemetryRecord<power_of_two(RotTagLength::TagFirmwareVerStringLen)>
        tag_firmware_ver_string_slot1;
    TelemetryRecord<power_of_two(RotTagLength::TagVerComparisonStampLen)>
                                                                 tag_ver_comparison_stamp_slot1;
    TelemetryRecord<power_of_two(RotTagLength::TagBuildTypeLen)> tag_build_type_slot1;
    TelemetryRecord<power_of_two(RotTagLength::TagSigningTypeLen)> tag_signing_type_slot1;
    TelemetryRecord<power_of_two(RotTagLength::TagWriteProtectStateLen)>
        tag_write_protect_state_slot1;
    TelemetryRecord<power_of_two(RotTagLength::TagFirmwareStateLen)> tag_firmware_state_slot1;
    TelemetryRecord<power_of_two(RotTagLength::TagSecurityVerNumLen)>
        tag_security_ver_num_slot1;
    TelemetryRecord<power_of_two(RotTagLength::TagSigningKeyIndexLen)>
        tag_signing_key_index_slot1;
};

struct [[gnu::packed]] SlotSpecificSegment
{
    TelemetryRecord<power_of_two(RotTagLength::TagFirmwareSlotIdLen)> tag_firmware_slot_id;
    TelemetryRecord<power_of_two(RotTagLength::TagFirmwareVerStringLen)>
        tag_firmware_ver_string;
    TelemetryRecord<power_of_two(RotTagLength::TagVerComparisonStampLen)>
                                                                   tag_ver_comparison_stamp;
    TelemetryRecord<power_of_two(RotTagLength::TagBuildTypeLen)>   tag_build_type;
    TelemetryRecord<power_of_two(RotTagLength::TagSigningTypeLen)> tag_signing_type;
    TelemetryRecord<power_of_two(RotTagLength::TagWriteProtectStateLen)>
                                                                       tag_write_protect_state;
    TelemetryRecord<power_of_two(RotTagLength::TagFirmwareStateLen)>   tag_firmware_state;
    TelemetryRecord<power_of_two(RotTagLength::TagSecurityVerNumLen)>  tag_security_ver_num;
    TelemetryRecord<power_of_two(RotTagLength::TagSigningKeyIndexLen)> tag_signing_key_index;
};

// Request Mesage Header V1
struct [[gnu::packed]] NsmPktReq : Header
{
    /* nvidia OEM binding */
    MsgType msg_type;

    uint16_t pci_vendor_id;

    uint8_t instance_id : 5;
    uint8_t rsvd0       : 1;
    uint8_t d           : 1;
    uint8_t rq          : 1;

    uint8_t ocp_version : 3;
    uint8_t ocp_type    : 4;
    uint8_t ocp         : 1;

    NsmMsgType nv_msg_type;

    union CmdCode
    {
        NsmDcdCmdCode      dcd_code;
        NsmFWCmdCode       fw_code;
        NsmPciLinksCmdCode type2_code;
        NsmDevCfgCmdCode   devcfg_code;
        NsmPlatEnvCmdCode  plat_env_code;
        NsmDevDiagCmdCode  devdiag_code;
        NsmType4CmdCode    type4_code;
        NsmTypeFFCmdCode   typeff_code;
    } cmd_code;

    uint8_t data_size;
    uint8_t data[1];

    static NsmPktReq& from(Packet& buf) { return *std::bit_cast<NsmPktReq*>(&buf.hdr); }

    static const NsmPktReq& from(const Packet& buf)
    {
        return *std::bit_cast<const NsmPktReq*>(&buf.hdr);
    }

    static NsmPktReq& from(NsmPacket& buf) { return *std::bit_cast<NsmPktReq*>(&buf.hdr); }

    static const NsmPktReq& from(const NsmPacket& buf)
    {
        return *std::bit_cast<const NsmPktReq*>(&buf.hdr);
    }

    NsmDcdCmdCode get_dcd_code() const { return cmd_code.dcd_code; }
    void          set_dcd_code(NsmDcdCmdCode code) { cmd_code.dcd_code = code; }

    NsmFWCmdCode get_fw_code() const { return cmd_code.fw_code; }
    void         set_fw_code(NsmFWCmdCode code) { cmd_code.fw_code = code; }

    NsmDevCfgCmdCode get_dev_cfg_code() const { return cmd_code.devcfg_code; }
    void             set_dev_cfg_code(NsmDevCfgCmdCode code) { cmd_code.devcfg_code = code; }

    NsmPlatEnvCmdCode get_plat_env_code() const { return cmd_code.plat_env_code; }
    void set_plat_env_code(NsmPlatEnvCmdCode code) { cmd_code.plat_env_code = code; }

    NsmDevDiagCmdCode get_dev_diag_code() const { return cmd_code.devdiag_code; }
    void set_dev_diag_code(NsmDevDiagCmdCode code) { cmd_code.devdiag_code = code; }

    NsmType4CmdCode get_type4_code() const { return cmd_code.type4_code; }
    void            set_type4_code(NsmType4CmdCode code) { cmd_code.type4_code = code; }

    NsmPciLinksCmdCode get_pci_links_code() const { return cmd_code.type2_code; }
    void set_pci_links_code(NsmPciLinksCmdCode code) { cmd_code.type2_code = code; }

    NsmTypeFFCmdCode get_typeff_code() const { return cmd_code.typeff_code; }
    void             set_typeff_code(NsmTypeFFCmdCode code) { cmd_code.typeff_code = code; }
};

// Request Mesage Header V2
struct [[gnu::packed]] NsmPktReqV2 : Header
{
    /* nvidia OEM binding */
    MsgType msg_type;

    uint16_t pci_vendor_id;

    uint8_t instance_id : 5;
    uint8_t rsvd0       : 1;
    uint8_t d           : 1;
    uint8_t rq          : 1;

    uint8_t ocp_version : 3;
    uint8_t ocp_type    : 4;
    uint8_t ocp         : 1;

    NsmMsgType nv_msg_type;

    union CmdCode
    {
        NsmDcdCmdCode dcd_code;
        NsmFWCmdCode  fw_code;
    } cmd_code;

    uint8_t  rsvd8;
    uint16_t data_size;
    uint16_t rsvd16;
    uint8_t  data[1];

    static NsmPktReqV2& from(Packet& buf) { return *std::bit_cast<NsmPktReqV2*>(&buf.hdr); }

    static const NsmPktReqV2& from(const Packet& buf)
    {
        return *std::bit_cast<const NsmPktReqV2*>(&buf.hdr);
    }

    static NsmPktReqV2& from(NsmPacket& buf) { return *std::bit_cast<NsmPktReqV2*>(&buf.hdr); }

    static const NsmPktReqV2& from(const NsmPacket& buf)
    {
        return *std::bit_cast<const NsmPktReqV2*>(&buf.hdr);
    }

    NsmDcdCmdCode get_dcd_code() const { return cmd_code.dcd_code; }
    void          set_dcd_code(NsmDcdCmdCode code) { cmd_code.dcd_code = code; }

    NsmFWCmdCode get_fw_code() const { return cmd_code.fw_code; }
    void         set_fw_code(NsmFWCmdCode code) { cmd_code.fw_code = code; }
};

struct [[gnu::packed]] NsmPktRespAggr : Header
{
    /* nvidia OEM binding */
    MsgType msg_type;

    uint16_t pci_vendor_id;

    uint8_t instance_id : 5;
    uint8_t rsvd0       : 1;
    uint8_t d           : 1;
    uint8_t rq          : 1;

    uint8_t ocp_version : 3;
    uint8_t ocp_type    : 4;
    uint8_t ocp         : 1;

    NsmMsgType nv_msg_type;

    union CmdCode
    {
        NsmDcdCmdCode      dcd_code;
        NsmFWCmdCode       fw_code;
        NsmPciLinksCmdCode type2_code;
        NsmDevCfgCmdCode   devcfg_code;
        NsmPlatEnvCmdCode  plat_env_code;
        NsmDevDiagCmdCode  devdiag_code;
    } cmd_code;

    Ccode    completion_code;
    uint16_t telemetry_count;

    RespAggregate resp_aggregate;

    static NsmPktRespAggr& from(Packet& buf)
    {
        return *std::bit_cast<NsmPktRespAggr*>(&buf.hdr);
    }

    static const NsmPktRespAggr& from(const Packet& buf)
    {
        return *std::bit_cast<const NsmPktRespAggr*>(&buf.hdr);
    }

    static NsmPktRespAggr& from(NsmPacket& buf)
    {
        return *std::bit_cast<NsmPktRespAggr*>(&buf.hdr);
    }

    static const NsmPktRespAggr& from(const NsmPacket& buf)
    {
        return *std::bit_cast<const NsmPktRespAggr*>(&buf.hdr);
    }

    NsmDcdCmdCode get_dcd_code() const { return cmd_code.dcd_code; }
    void          set_dcd_code(NsmDcdCmdCode code) { cmd_code.dcd_code = code; }

    NsmFWCmdCode get_fw_code() const { return cmd_code.fw_code; }
    void         set_fw_code(NsmFWCmdCode code) { cmd_code.fw_code = code; }

    NsmDevCfgCmdCode get_dev_cfg_code() const { return cmd_code.devcfg_code; }
    void             set_dev_cfg_code(NsmDevCfgCmdCode code) { cmd_code.devcfg_code = code; }

    NsmPlatEnvCmdCode get_plat_env_code() const { return cmd_code.plat_env_code; }
    void set_plat_env_code(NsmPlatEnvCmdCode code) { cmd_code.plat_env_code = code; }

    NsmDevDiagCmdCode get_dev_diag_code() const { return cmd_code.devdiag_code; }
    void set_dev_diag_code(NsmDevDiagCmdCode code) { cmd_code.devdiag_code = code; }

    NsmPciLinksCmdCode get_pci_links_code() const { return cmd_code.type2_code; }
    void set_pci_links_code(NsmPciLinksCmdCode code) { cmd_code.type2_code = code; }
};

struct [[gnu::packed]] NsmPktResp : Header
{
    /* nvidia OEM binding */
    MsgType msg_type;

    uint16_t pci_vendor_id;

    uint8_t instance_id : 5;
    uint8_t rsvd0       : 1;
    uint8_t d           : 1;
    uint8_t rq          : 1;

    uint8_t ocp_version : 3;
    uint8_t ocp_type    : 4;
    uint8_t ocp         : 1;

    NsmMsgType nv_msg_type;

    union CmdCode
    {
        NsmDcdCmdCode      dcd_code;
        NsmFWCmdCode       fw_code;
        NsmPciLinksCmdCode type2_code;
        NsmDevCfgCmdCode   devcfg_code;
        NsmPlatEnvCmdCode  plat_env_code;
        NsmDevDiagCmdCode  devdiag_code;
        NsmType4CmdCode    type4_code;
    } cmd_code;

    Ccode    completion_code;
    uint16_t reserved;
    uint16_t data_size;
    uint8_t  data[1];

    static NsmPktResp& from(Packet& buf) { return *std::bit_cast<NsmPktResp*>(&buf.hdr); }

    static const NsmPktResp& from(const Packet& buf)
    {
        return *std::bit_cast<const NsmPktResp*>(&buf.hdr);
    }

    static NsmPktResp& from(NsmPacket& buf) { return *std::bit_cast<NsmPktResp*>(&buf.hdr); }

    static const NsmPktResp& from(const NsmPacket& buf)
    {
        return *std::bit_cast<const NsmPktResp*>(&buf.hdr);
    }

    NsmDcdCmdCode get_dcd_code() const { return cmd_code.dcd_code; }
    void          set_dcd_code(NsmDcdCmdCode code) { cmd_code.dcd_code = code; }

    NsmFWCmdCode get_fw_code() const { return cmd_code.fw_code; }
    void         set_fw_code(NsmFWCmdCode code) { cmd_code.fw_code = code; }

    NsmDevCfgCmdCode get_dev_cfg_code() const { return cmd_code.devcfg_code; }
    void             set_dev_cfg_code(NsmDevCfgCmdCode code) { cmd_code.devcfg_code = code; }

    NsmPlatEnvCmdCode get_plat_env_code() const { return cmd_code.plat_env_code; }
    void set_plat_env_code(NsmPlatEnvCmdCode code) { cmd_code.plat_env_code = code; }

    NsmDevDiagCmdCode get_dev_diag_code() const { return cmd_code.devdiag_code; }
    void set_dev_diag_code(NsmDevDiagCmdCode code) { cmd_code.devdiag_code = code; }

    NsmType4CmdCode get_type4_code() const { return cmd_code.type4_code; }
    void            set_type4_code(NsmType4CmdCode code) { cmd_code.type4_code = code; }

    NsmPciLinksCmdCode get_pci_links_code() const { return cmd_code.type2_code; }
    void set_pci_links_code(NsmPciLinksCmdCode code) { cmd_code.type2_code = code; }
};

struct [[gnu::packed]] NsmPktRespV2 : Header
{
    /* nvidia OEM binding */
    MsgType msg_type;

    uint16_t pci_vendor_id;

    uint8_t instance_id : 5;
    uint8_t rsvd0       : 1;
    uint8_t d           : 1;
    uint8_t rq          : 1;

    uint8_t ocp_version : 3;
    uint8_t ocp_type    : 4;
    uint8_t ocp         : 1;

    NsmMsgType nv_msg_type;

    union CmdCode
    {
        NsmDcdCmdCode      dcd_code;
        NsmFWCmdCode       fw_code;
        NsmPciLinksCmdCode type2_code;
        NsmDevCfgCmdCode   devcfg_code;
        NsmPlatEnvCmdCode  plat_env_code;
        NsmDevDiagCmdCode  devdiag_code;
        NsmType4CmdCode    type4_code;
    } cmd_code;

    Ccode completion_code;
    Rcode reason_code;

    static NsmPktRespV2& from(Packet& buf) { return *std::bit_cast<NsmPktRespV2*>(&buf.hdr); }

    static const NsmPktRespV2& from(const Packet& buf)
    {
        return *std::bit_cast<const NsmPktRespV2*>(&buf.hdr);
    }

    static NsmPktRespV2& from(NsmPacket& buf)
    {
        return *std::bit_cast<NsmPktRespV2*>(&buf.hdr);
    }

    static const NsmPktRespV2& from(const NsmPacket& buf)
    {
        return *std::bit_cast<const NsmPktRespV2*>(&buf.hdr);
    }

    NsmDcdCmdCode get_dcd_code() const { return cmd_code.dcd_code; }
    void          set_dcd_code(NsmDcdCmdCode code) { cmd_code.dcd_code = code; }

    NsmFWCmdCode get_fw_code() const { return cmd_code.fw_code; }
    void         set_fw_code(NsmFWCmdCode code) { cmd_code.fw_code = code; }

    NsmDevCfgCmdCode get_dev_cfg_code() const { return cmd_code.devcfg_code; }
    void             set_dev_cfg_code(NsmDevCfgCmdCode code) { cmd_code.devcfg_code = code; }

    NsmPlatEnvCmdCode get_plat_env_code() const { return cmd_code.plat_env_code; }
    void set_plat_env_code(NsmPlatEnvCmdCode code) { cmd_code.plat_env_code = code; }

    NsmDevDiagCmdCode get_dev_diag_code() const { return cmd_code.devdiag_code; }
    void set_dev_diag_code(NsmDevDiagCmdCode code) { cmd_code.devdiag_code = code; }

    NsmType4CmdCode get_type4_code() const { return cmd_code.type4_code; }
    void            set_type4_code(NsmType4CmdCode code) { cmd_code.type4_code = code; }

    NsmPciLinksCmdCode get_pci_links_code() const { return cmd_code.type2_code; }
    void set_pci_links_code(NsmPciLinksCmdCode code) { cmd_code.type2_code = code; }
};

/** Similar as the NsmPktRespAggr struct, but intented for dynamic payload
 *
 *    The payload can be an array of TelemetryRecord structs and the the
 *       @sa TelemetryRecordArray can be used for that dynamic insertions
 */
struct [[gnu::packed]] NsmPktBulkResp : Header
{
    /* nvidia OEM binding */
    MsgType msg_type;

    uint16_t pci_vendor_id;

    uint8_t instance_id : 5;
    uint8_t rsvd0       : 1;
    uint8_t d           : 1;
    uint8_t rq          : 1;

    uint8_t ocp_version : 3;
    uint8_t ocp_type    : 4;
    uint8_t ocp         : 1;

    NsmMsgType nv_msg_type;

    union CmdCode
    {
        NsmDcdCmdCode     dcd_code;
        NsmFWCmdCode      fw_code;
        NsmPlatEnvCmdCode plat_env_code;
    } cmd_code;

    Ccode    completion_code;
    uint16_t telemetry_count;

    uint8_t data[1];

    static NsmPktBulkResp& from(Packet& buf)
    {
        return *std::bit_cast<NsmPktBulkResp*>(&buf.hdr);
    }

    static const NsmPktBulkResp& from(const Packet& buf)
    {
        return *std::bit_cast<const NsmPktBulkResp*>(&buf.hdr);
    }

    static NsmPktBulkResp& from(NsmPacket& buf)
    {
        return *std::bit_cast<NsmPktBulkResp*>(&buf.hdr);
    }

    static const NsmPktBulkResp& from(const NsmPacket& buf)
    {
        return *std::bit_cast<const NsmPktBulkResp*>(&buf.hdr);
    }

    static NsmPktResp& to_NsmPktReq(NsmPktBulkResp& buf)
    {
        return *std::bit_cast<NsmPktResp*>(&buf);
    }

    NsmPlatEnvCmdCode get_plat_env_code() const { return cmd_code.plat_env_code; }
    void set_plat_env_code(NsmPlatEnvCmdCode code) { cmd_code.plat_env_code = code; }
};

struct [[gnu::packed]] NsmEventMsg : Header
{
    /* nvidia OEM binding */
    MsgType msg_type;

    uint16_t pci_vendor_id;

    uint8_t instance_id : 5;
    uint8_t rsvd0       : 1;
    uint8_t d           : 1;
    uint8_t rq          : 1;

    uint8_t ocp_version : 3;
    uint8_t ocp_type    : 4;
    uint8_t ocp         : 1;

    NsmMsgType nv_msg_type;

    uint8_t rsvd1         : 3;
    uint8_t ackr          : 1;
    uint8_t event_version : 4;

    uint8_t  event_id;
    uint8_t  event_class;
    uint16_t event_state;
    uint8_t  data_size;
    uint8_t  data[1];

    static NsmEventMsg& from(Packet& buf) { return *std::bit_cast<NsmEventMsg*>(&buf.hdr); }

    static const NsmEventMsg& from(const Packet& buf)
    {
        return *std::bit_cast<const NsmEventMsg*>(&buf.hdr);
    }

    static NsmEventMsg& from(NsmPacket& buf) { return *std::bit_cast<NsmEventMsg*>(&buf.hdr); }

    static const NsmEventMsg& from(const NsmPacket& buf)
    {
        return *std::bit_cast<const NsmEventMsg*>(&buf.hdr);
    }
};

// NSM Event related structures moved to nv/mctp/nsm_event.h

constexpr std::array<uint8_t, nsm_msg::NvMctpSupportedNum> gen_nv_meg_bitmask()
{
    std::array<uint8_t, nsm_msg::NvMctpSupportedNum> bitmask = {0};

    // Common Message Types for all SMA variants:
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmMsgType::DeviceCapabilityDiscovery));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmMsgType::Firmware));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmMsgType::PlatformEnviromentals));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmMsgType::Diagnostics));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmMsgType::DeviceConfiguration));

    config_nsm_types(bitmask);
    return bitmask;
}

constexpr std::array<uint8_t, nsm_msg::NvMctpSupportedNum> gen_type0_code_bitmask()
{
    std::array<uint8_t, nsm_msg::NvMctpSupportedNum> bitmask = {0};

    // Common commands for all SMA variants:
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDcdCmdCode::DcdPing));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDcdCmdCode::DcdGetSupNvMsgTypes));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDcdCmdCode::DcdGetSupCmdCodes));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDcdCmdCode::DcdGetSupEventSrcs));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDcdCmdCode::DcdGetCurrentEventSrcs));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDcdCmdCode::DcdSetCurrentEventSrcs));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDcdCmdCode::DcdSetEventSubscription));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDcdCmdCode::DcdGetEventSubscription));
    nsm_msg::set_bit(bitmask,
                     static_cast<uint8_t>(NsmDcdCmdCode::DcdQueryDeviceIdentification));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDcdCmdCode::DcdGetGpio));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDcdCmdCode::DcdSetGpio));

    config_nsm_type0_cmd(bitmask);
    return bitmask;
}

constexpr std::array<uint8_t, nsm_msg::NvMctpSupportedNum> gen_type2_code_bitmask()
{
    std::array<uint8_t, nsm_msg::NvMctpSupportedNum> bitmask = {0};

    config_nsm_type2_cmd(bitmask);
    return bitmask;
}

constexpr std::array<uint8_t, nsm_msg::NvMctpSupportedNum> gen_type3_code_bitmask()
{
    std::array<uint8_t, nsm_msg::NvMctpSupportedNum> bitmask = {0};

    // Common commands for all SMA variants:
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmPlatEnvCmdCode::GetTemperatureReading));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmPlatEnvCmdCode::GetInventoryInformation));

    // Selected Commands upon configuration:
    if constexpr (mcuPowerSensorsSize > 0) {
        nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmPlatEnvCmdCode::GetCurrentPowerDraw));
    }
    if constexpr ((nv::mctp::I2cTempSensorSize + nv::i2c::power::HscDeviceCount
                   + nv::i2c::power::HsccDeviceCount)
                  > 0) {
        nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmPlatEnvCmdCode::SetThermalParameter));
        nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmPlatEnvCmdCode::GetThermalParameter));
    }
    if constexpr (nv::mctp::mcuVoltageSensorsSize > 0) {
        nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmPlatEnvCmdCode::GetVoltage));
    }
    if constexpr (nv::ipc::voltage_monitor_config::LeakDetectSensorNum > 0) {
        nsm_msg::set_bit(bitmask,
                         static_cast<uint8_t>(NsmPlatEnvCmdCode::GetLeakDetectionInfo));
        nsm_msg::set_bit(bitmask,
                         static_cast<uint8_t>(NsmPlatEnvCmdCode::SetLeakDetectionThresholds));
    }

    config_nsm_type3_cmd(bitmask);
    return bitmask;
}

constexpr std::array<uint8_t, nsm_msg::NvMctpSupportedNum> gen_type4_code_bitmask()
{
    std::array<uint8_t, nsm_msg::NvMctpSupportedNum> bitmask = {0};

    // Common commands for all SMA variants:
    nsm_msg::set_bit(bitmask,
                     static_cast<uint8_t>(NsmDevDiagCmdCode::GetDeviceResetStatistics));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDevDiagCmdCode::GetDeviceDiagnostics));

    // Selected Commands upon configuration:
    if constexpr (nv::i2c::power::HscDeviceCount + nv::i2c::power::HsccDeviceCount > 0) {
        nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDevDiagCmdCode::GetDeviceHscAlert));
        nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDevDiagCmdCode::ClearHscFaults));
    }

    if constexpr (nv::ipc::DebugTokenEnabled) {
        nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmType4CmdCode::InstallToken));
        nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmType4CmdCode::EraseToken));
        nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmType4CmdCode::QueryToken));
        nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmType4CmdCode::QueryDeviceIds));
    }

    config_nsm_type4_cmd(bitmask);
    return bitmask;
}

constexpr std::array<uint8_t, nsm_msg::NvMctpSupportedNum> gen_type5_code_bitmask()
{
    std::array<uint8_t, nsm_msg::NvMctpSupportedNum> bitmask = {0};

    // Common commands for all SMA variants:
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDevCfgCmdCode::SetErrorInjectionMode));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDevCfgCmdCode::GetErrorInjectionMode));
    nsm_msg::set_bit(bitmask,
                     static_cast<uint8_t>(NsmDevCfgCmdCode::GetSupportedErrorInjectionTypes));
    nsm_msg::set_bit(bitmask,
                     static_cast<uint8_t>(NsmDevCfgCmdCode::SetCurrentErrorInjectionTypes));
    nsm_msg::set_bit(bitmask,
                     static_cast<uint8_t>(NsmDevCfgCmdCode::GetCurrentErrorInjectionTypes));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDevCfgCmdCode::GetErrorInjectionPayload));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDevCfgCmdCode::SetErrorInjectionPayload));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmDevCfgCmdCode::ActivateErrorInjection));

    config_nsm_type5_cmd(bitmask);
    return bitmask;
}

constexpr std::array<uint8_t, nsm_msg::NvMctpSupportedNum> gen_type6_code_bitmask()
{
    std::array<uint8_t, nsm_msg::NvMctpSupportedNum> bitmask = {0};

    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmFWCmdCode::GetRotStateInfo));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmFWCmdCode::IrreversibleConf));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmFWCmdCode::QueryCodeAuthKey));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmFWCmdCode::UpdateCodeAuthKey));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmFWCmdCode::QuerySecVerNum));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmFWCmdCode::UpdateMinSecVerNum));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmFWCmdCode::QueryFwCompId));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmFWCmdCode::SetRotProperty));
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmFWCmdCode::ImageCopyControl));

    config_nsm_type6_cmd(bitmask);
    return bitmask;
}

constexpr std::array<uint8_t, nsm_msg::NvMctpSupportedNum> gen_typeff_code_bitmask()
{
    std::array<uint8_t, nsm_msg::NvMctpSupportedNum> bitmask = {0};

    config_nsm_typeff_cmd(bitmask);
    return bitmask;
}

constexpr std::array<uint8_t, nsm_msg::NvMctpEventSupportedNum> gen_type6_event_bitmask()
{
    std::array<uint8_t, nsm_msg::NvMctpEventSupportedNum> bitmask = {0};
    nsm_msg::set_bit(bitmask, static_cast<uint8_t>(NsmFwEvent::RotStateInformationChangeEvent));
    return bitmask;
}

static constexpr uint8_t
count_events_in_bitmask(const std::array<uint8_t, nsm_msg::NvMctpEventSupportedNum>& bitmask)
{
    uint8_t count = 0;
    for (size_t i = 0; i < nsm_msg::NvMctpEventSupportedNum; ++i) {
        // Count set bits using Brian Kernighan's algorithm
        uint8_t byte = bitmask[i];
        while (byte) {
            byte &= byte - 1;  // Remove the rightmost set bit
            // Check for overflow before increment
            if (count == UINT8_MAX) {
                return UINT8_MAX;  // Return max value if overflow would occur
            }
            count++;
        }
    }
    return count;
}

class Nsm
{
public:
    Nsm(Control& ctl) : _ctl(ctl) {}
    bool process(const Packet& rx, Packet& tx);
    static constexpr std::array<uint8_t, nsm_msg::NvMctpSupportedNum>
        SupNvMegType = gen_nv_meg_bitmask();
    static constexpr std::array<uint8_t, nsm_msg::NvMctpSupportedNum>
        SupType0Code = gen_type0_code_bitmask();
    static constexpr std::array<uint8_t, nsm_msg::NvMctpSupportedNum>
        SupType2Code = gen_type2_code_bitmask();
    static constexpr std::array<uint8_t, nsm_msg::NvMctpSupportedNum>
        SupType3Code = gen_type3_code_bitmask();
    static constexpr std::array<uint8_t, nsm_msg::NvMctpSupportedNum>
        SupType4Code = gen_type4_code_bitmask();
    static constexpr std::array<uint8_t, nsm_msg::NvMctpSupportedNum>
        SupType5Code = gen_type5_code_bitmask();
    static constexpr std::array<uint8_t, nsm_msg::NvMctpSupportedNum>
        SupType6Code = gen_type6_code_bitmask();
    static constexpr std::array<uint8_t, nsm_msg::NvMctpSupportedNum>
        SupTypeFFCode = gen_typeff_code_bitmask();
    static constexpr std::array<uint8_t, nsm_msg::NvMctpEventSupportedNum>
        SupType0Event = gen_type0_event_bitmask();
    static constexpr std::array<uint8_t, nsm_msg::NvMctpEventSupportedNum>
        SupType6Event = gen_type6_event_bitmask();

    // Constexpr function helpers
    static constexpr bool is_msg_set(mctp::NsmMsgType msg)
    {
        return nsm_msg::is_bit_set(SupNvMegType, static_cast<uint8_t>(msg));
    }

    template<typename MsgCmdCode>
    static constexpr bool is_cmd_set(NsmMsgType msg, MsgCmdCode cmd)
    {
        switch (msg) {
            case NsmMsgType::DeviceCapabilityDiscovery:
                return nsm_msg::is_bit_set(SupType0Code, static_cast<uint8_t>(cmd));
            case NsmMsgType::PciLinks:
                return nsm_msg::is_bit_set(SupType2Code, static_cast<uint8_t>(cmd));
            case NsmMsgType::PlatformEnviromentals:
                return nsm_msg::is_bit_set(SupType3Code, static_cast<uint8_t>(cmd));
            case NsmMsgType::Diagnostics:
                return nsm_msg::is_bit_set(SupType4Code, static_cast<uint8_t>(cmd));
            case NsmMsgType::DeviceConfiguration:
                return nsm_msg::is_bit_set(SupType5Code, static_cast<uint8_t>(cmd));
            case NsmMsgType::Firmware:
                return nsm_msg::is_bit_set(SupType6Code, static_cast<uint8_t>(cmd));
            case NsmMsgType::NvInternal:
                return nsm_msg::is_bit_set(SupTypeFFCode, static_cast<uint8_t>(cmd));
            default: break;
        }
        return false;
    }

    // Constexpr variables to calculate the number of events for each type
    static constexpr uint8_t SupType0EventCount = count_events_in_bitmask(SupType0Event);
    static constexpr uint8_t SupType6EventCount = count_events_in_bitmask(SupType6Event);

    // Total number of events across all types
    static constexpr uint8_t NsmEventNum = SupType0EventCount + SupType6EventCount;

    bool is_event_source_enable(NsmMsgType nv_msg_type, uint8_t event_id);
    bool is_event_ack_enable(NsmMsgType nv_msg_type, uint8_t event_id);

    bool is_global_event_setting_push();
    void fill_event_msg(const EventLog& event_log, Packet& tx) const;
    bool cache_event_msg(Packet& eventMsg);

    static bool    fill_build_type(uint8_t slot, uint8_t& build_type);
    static uint8_t get_active_slot();
    static void    append_number(std::array<char, NvMctpVersionLength>& buf,
                                 size_t&                                index,
                                 uint32_t                               num,
                                 size_t                                 width,
                                 bool                                   add_dot);
    static void    generate_fw_version(std::array<char, NvMctpVersionLength>& buf,
                                       uint16_t                               major,
                                       uint8_t                                minor,
                                       uint16_t                               patch,
                                       uint16_t                               build);

    // GPIO Event Trigger
    static void
    GpioEventTrigger(nv::gpio::GpioPort port, uint32_t flag, GpioEventSource gpioSource);
    static bool VirtualGpioEventTrigger(std::span<const uint8_t> vpins,
                                        std::span<const uint8_t> vals);

    // Type 5 data accessors
    const GPIOSpoofingPayload& getGPIOSpoofingPayload() const
    {
        return type5_data.getGPIOSpoofingPayload();
    }

protected:
    // Type 0
    void on_dcd_ping(const Packet& rx, Packet& tx);
    void on_dcd_get_sup_nv_meg_type(const Packet& rx, Packet& tx);
    void on_dcd_get_sup_cmd_codes(const Packet& rx, Packet& tx);
    void on_dcd_get_sup_event_srcs(const Packet& rx, Packet& tx);
    void on_dcd_get_current_event_srcs(const Packet& rx, Packet& tx);
    void on_dcd_set_current_event_srcs(const Packet& rx, Packet& tx);
    void on_dcd_set_event_subscription(const Packet& rx, Packet& tx);
    void on_dcd_get_event_subscription(const Packet& rx, Packet& tx);
    void on_dcd_configure_event_ack(const Packet& rx, Packet& tx);
    void on_dcd_query_device_id(const Packet& rx, Packet& tx);
    void on_dcd_get_device_capabilities_v2(const Packet& rx, Packet& tx);
    void on_dcd_get_gpio(const Packet& rx, Packet& tx);
    void on_dcd_set_gpio(const Packet& rx, Packet& tx);
    bool process_device_capability_discovery(const Packet& rx, Packet& tx);
    // Type 2
    bool process_pci_links(const Packet& rx, Packet& tx);
    void on_pci_links_assert_pcie_fundamental_reset(const Packet& rx, Packet& tx);
    // Type 3
    bool process_platform_enviromentals(const Packet& rx, Packet& tx);
    void on_plat_env_getTemperatureReading(const Packet& rx, Packet& tx);
    void on_plat_env_getPowerDraw(const Packet& rx, Packet& tx);
    void on_plat_env_getVoltage(const Packet& rx, Packet& tx);
    void on_plat_env_getInventoryInformation(const Packet& rx, Packet& tx);
    void on_plat_env_setThermalParameter(const Packet& rx, Packet& tx);
    void on_plat_env_getThermalParameter(const Packet& rx, Packet& tx);
    void on_plat_env_setLeakDetectionThresholds(const Packet& rx, Packet& tx);
    void on_plat_env_getLeakDetectionInfo(const Packet& rx, Packet& tx);
    // Type 4
    bool process_diagnostics(const Packet& rx, Packet& tx);
    void on_dev_diag_get_reset_statistics(const Packet& rx, Packet& tx);
    void on_dev_diag_get_diagnostics(const Packet& rx, Packet& tx);
    void on_dev_diag_bridge_port_recovery(const Packet& rx, Packet& tx);
    void on_dev_diag_read_gpio(const Packet& rx, Packet& tx);
    void on_dev_diag_set_gpio(const Packet& rx, Packet& tx);
    void on_dev_diag_enable_disable_write_protection(const Packet& rx, Packet& tx);
    void on_dev_diag_get_device_hsc_alert(const Packet& rx, Packet& tx);
    void on_dev_diag_clear_hsc_faults(const Packet& rx, Packet& tx);
    void on_dev_diag_get_cpld_register_table(const Packet& rx, Packet& tx);

    // Type 4 Debug Token
    bool process_debugtoken_diagnostics(const Packet& rx, Packet& tx);
    void on_install_token(const Packet& rx, Packet& tx);
    void on_erase_token(const Packet& rx, Packet& tx);
    void on_query_token(const Packet& rx, Packet& tx);
    void on_query_device_ids(const Packet& rx, Packet& tx);
    // Type 5
    bool process_device_configuration(const Packet& rx, Packet& tx);
    void on_dev_cfg_set_errorInjectionMode(const Packet& rx, Packet& tx);
    void on_dev_cfg_get_errorInjectionMode(const Packet& rx, Packet& tx);
    void on_dev_cfg_get_supportedErrorInjectionTypes(const Packet& rx, Packet& tx);
    void on_dev_cfg_set_currentErrorInjectionTypes(const Packet& rx, Packet& tx);
    void on_dev_cfg_get_currentErrorInjectionTypes(const Packet& rx, Packet& tx);
    void on_dev_cfg_get_ErrorInjectionPayload(const Packet& rx, Packet& tx);
    void on_dev_cfg_submit_ErrorInjectionPayload(const Packet& rx, Packet& tx);
    void on_dev_cfg_activate_ErrorInjectionPayload(const Packet& rx, Packet& tx);
    void on_dev_cfg_set_gpu_degrade_mode(const Packet& rx, Packet& tx);
    void on_dev_cfg_enable_disable_power_supply(const Packet& rx, Packet& tx);
    bool is_gpio_spoofing_activate();
    bool get_gpio_default_value(uint16_t gpio_index, uint8_t& pin_value);
    bool get_gpi_spoofing_value(uint16_t gpio_index, uint8_t& pin_value);
    void notifyIoxSpoofingState(bool spoofingActive);

    void on_dev_cfg_get_sma_baseboard_settings(const Packet& rx, Packet& tx);
    void on_dev_cfg_set_device_mode_settings(const Packet& rx, Packet& tx);
    void on_dev_cfg_get_device_mode_settings(const Packet& rx, Packet& tx);
    void on_dev_cfg_get_supported_device_modes(const Packet& rx, Packet& tx);

    Ccode handle_set_ncsi_mac(const uint8_t* data);
    Ccode handle_get_ncsi_mac(NsmPktResp& ntx);
    // Type 6
    void get_rot_state_info(const Packet& rx, Packet& tx);
    void get_ap_rot_state_info(const Packet& rx, Packet& tx, const uint16_t& component_id);
    void on_get_rot_state_info(const Packet& rx, Packet& tx);
    void query_auth_key(const Packet& rx, Packet& tx);
    void query_ap_auth_key(const Packet& rx, Packet& tx);
    void on_query_auth_key(const Packet& rx, Packet& tx);
    void on_set_rot_property(const Packet& rx, Packet& tx);
    void query_image_copy_progress(const Packet& rx, Packet& tx);
    void initiate_image_copy(const Packet&              rx,
                             Packet&                    tx,
                             const ImageCopyControlReq& image_copy_control_req);
    void on_image_copy_control(const Packet& rx, Packet& tx);
    void update_auth_key(const Packet& rx, Packet& tx, const UpdateAuthKeyReq& update_struct);
    void
    update_ap_auth_key(const Packet& rx, Packet& tx, const UpdateAuthKeyReq& update_struct);
    void on_update_auth_key(const Packet& rx, Packet& tx);
    bool is_irreversible_ctrl_enabled();
    void disable_irreversible_ctrl();
    void on_ctrl_irreversible_conf(const Packet& rx, Packet& tx);
    void query_sec_ver_num(const Packet& rx, Packet& tx);
    void query_ap_sec_ver_num(const Packet& rx, Packet& tx);
    void on_query_sec_ver_num(const Packet& rx, Packet& tx);
    void on_query_fw_comp_id(const Packet& rx, Packet& tx);
    void
    update_min_sec_ver_num(const Packet& rx, Packet& tx, const UpdateMinSvnReq& update_struct);
    void update_ap_min_sec_ver_num(const Packet&          rx,
                                   Packet&                tx,
                                   const UpdateMinSvnReq& update_struct);
    void on_update_min_sec_ver_num(const Packet& rx, Packet& tx);
    bool process_firmware(const Packet& rx, Packet& tx);
    // Type FF - NVIDIA Internal
    void process_nv_internal(const Packet& rx, Packet& tx);
    void on_nv_internal_getRackPowerSmoothingParam(const Packet& rx, Packet& tx);
    void on_nv_internal_setRackPowerSmoothingParam(const Packet& rx, Packet& tx);
    void on_nv_internal_getRackPowerSmoothingTestHook(const Packet& rx, Packet& tx);
    void on_nv_internal_setRackPowerSmoothingTestHook(const Packet& rx, Packet& tx);
    void on_nv_internal_getDebugTelemetry(const Packet& rx, Packet& tx);
    void on_nv_internal_triggerAdcCalibration(const Packet& rx, Packet& tx);
    void on_nv_internal_getAdcCalibrationResults(const Packet& rx, Packet& tx);
    void on_nv_internal_adcCalibSetLoopbackDacCode(const Packet& rx, Packet& tx);
    void on_nv_internal_getPowerSmoothRawReadback(const Packet& rx, Packet& tx);
    void on_nv_internal_setSocCalibCoefficient(const Packet& rx, Packet& tx);
    void on_nv_internal_getSocCalibCoefficient(const Packet& rx, Packet& tx);

    enum class NvInternalReqCheck : uint8_t
    {
        LengthOnly,
        ExactSize,
        ExactSizeLengthOnMismatch,
    };

    template<typename Req>
    bool load_nv_internal_request(const Packet&      rx,
                                  Packet&            tx,
                                  Req&               request,
                                  NvInternalReqCheck check);

    template<typename Handler>
    void dispatch_nv_internal_get(const Packet& rx, Packet& tx, Handler&& handler);

    template<typename Req, typename Handler>
    void dispatch_nv_internal_get_req(const Packet&      rx,
                                      Packet&            tx,
                                      NvInternalReqCheck check,
                                      Handler&&          handler);

    template<typename Req, typename Handler>
    void dispatch_nv_internal_set_req(const Packet&      rx,
                                      Packet&            tx,
                                      NvInternalReqCheck check,
                                      Handler&&          handler);

    template<typename Handler>
    void dispatch_nv_internal_action(const Packet& rx, Packet& tx, Handler&& handler);

    // Misc
    void     fill_nsm_msg_header(const Packet& rx, Packet& tx) const;
    void     fill_error_packet(Ccode code, const Packet& rx, Packet& tx) const;
    void     fill_error_packet_v2(Ccode         completion_code,
                                  Rcode         reason_code,
                                  const Packet& rx,
                                  Packet&       tx) const;
    void     fill_packet_header(const Packet& rx, Packet& tx) const;
    void     fill_nsm_msg_header_aggr(const Packet& rx, Packet& tx) const;
    void     fill_error_packet_aggr(Ccode code, const Packet& rx, Packet& tx) const;
    void     fill_packet_header_aggr(const Packet& rx, Packet& tx) const;
    bool     is_fw_comp_id_valid(const FwCompInfo& input_fw_comp_info, uint16_t component_id);
    bool     is_nonce_match(const Nonce& input_nonce);
    bool     is_input_length_valid(const Packet& rx, uint8_t size);
    bool     can_revoke_otp(Rcode& reason_code);
    bool     can_initiate_image_copy(Ccode& completion_code, Rcode& reason_code);
    void     get_redundancy_policy(NsmRedundancyPolicy& redundancy_policy_persistent,
                                   NsmRedundancyPolicy& redundancy_policy_current);
    Ccode    can_revoke_ap_otp();
    bool     is_inactive_authenticate(uint8_t inactive_slot);
    bool     is_active_slot(uint8_t slot);
    uint8_t  get_inactive_fw_state();
    uint8_t  get_ap_state();
    uint32_t array_to_u32(std::array<uint8_t, 4>& buffer);
    uint8_t  get_inactive_slot();
    uint32_t convert_unary_to_key_index(uint32_t unary_value);
    bool     is_form_zeros_then_ones(uint32_t num);
    Ccode    convert_key_index_to_key_permission(uint16_t key_index, uint32_t& key_permission);
    Ccode    fill_key_index(uint8_t slot, uint16_t& key_index);
    Ccode    fill_ap_key_index(uint16_t& key_index);
    Ccode    fill_fmc_key_index(uint16_t& key_index);
    Ccode    fill_key_permission(uint8_t slot, uint32_t& key_permission);
    Ccode    fill_ap_key_permission(uint32_t& key_permission);
    Ccode    fill_fmc_key_permission(uint32_t& key_permission);
    Ccode    fill_fuse_key_permission(uint32_t& key_permission);
    Ccode    fill_ap_fuse_key_permission(uint32_t& key_permission);
    Ccode    fill_sec_ver_num(uint8_t slot, uint32_t& rollback_protection);
    Ccode    fill_ap_sec_ver_num(uint32_t& rollback_protection);
    Ccode    fill_fmc_sec_ver_num(uint32_t& rollback_protection);
    Ccode    fill_fuse_mini_sec_ver_num(uint32_t& rollback_protection);
    Ccode    fill_ap_fuse_mini_sec_ver_num(uint32_t& rollback_protection);
    Ccode    revoke_rollback_protection(uint32_t rollback_protection);
    Ccode    revoke_ap_rollback_protection(uint32_t rollback_protection);
    uint8_t  fill_signing_type(uint8_t index);
    void     fill_boot_status_code(std::array<uint8_t, 8>& input);
    Ccode    revoke_key_permission(uint32_t key_permission);
    Ccode    revoke_ap_key_permission(uint32_t key_permission);

    Control&                                              _ctl;
    bool                                                  irreversible_input_received  = false;
    uint32_t                                              irreversible_last_time_stamp = 0;
    Nonce                                                 _nonce{};
    std::array<uint8_t, nsm_msg::NvMctpEventSupportedNum> type0_event_enable_bitmask{};
    std::array<uint8_t, nsm_msg::NvMctpEventSupportedNum> type6_event_enable_bitmask{};
    // Event acknowledgement bitmasks
    std::array<uint8_t, nsm_msg::NvMctpEventSupportedNum> type6_event_ack_bitmask{};
    std::array<uint8_t, nsm_msg::NvMctpEventSupportedNum> type0_event_ack_bitmask{};
    std::array<uint8_t, nsm_msg::NvMctpSupportedNum>      log_nvmsg_event_bitmask{};
    bool                                                  log_event_subscription = false;
    Packet            nsm_event_clients{};  // The rx from the set event subscribe command
    EventSubscription event_subscription{NsmGlobalEventSetting::EventDisable, 0};
    std::array<NsmEventCache, NsmEventNum> event_cache{};

    // Nsm constants, refer to nv/mctp/constants.h
    constexpr static auto HeaderResponseSize       = Constants::NsmHeaderResponseSize;
    constexpr static auto HeaderReasonResponseSize = Constants::NsmHeaderReasonResponseSize;
    constexpr static auto
        AggregateHeaderResponseSize           = Constants::NsmAggregateHeaderResponseSize;
    constexpr static auto HeaderRequestSize   = Constants::NsmHeaderRequestSize;
    constexpr static auto HeaderRequestSizeV2 = Constants::NsmHeaderRequestSizeV2;
    constexpr static auto HeaderEventMsgSize  = Constants::NsmHeaderEventMsgSize;
    constexpr static auto Type4ResponseSize   = Constants::NsmType4ResponseSize;

    NsmDevCfgPersistentData type5_data;
};

}  // namespace nv::mctp

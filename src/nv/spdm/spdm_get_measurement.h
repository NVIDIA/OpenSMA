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
#include <bitset>
#include <cstring>
#include <stdint.h>

#include "nv/common/uuid.h"
#include "nv/debugtoken/debugtoken.h"
#include "nv/nv.h"
#include "sys/flash/flash_config.h"

namespace nv {
namespace spdm {
namespace measurement {

constexpr uint32_t EcTagActiveAddr   = 0x0u;
constexpr uint32_t EcTagInactiveAddr = sys::flash::config::Slot1FwAddress;
constexpr uint32_t EcFirmwareMaxSize = sys::flash::config::FirmwareMaxSize;

typedef struct [[gnu::packed]]
{
    nv::common::Uuid measurement_uuid;
} MeasurementCacheT;

// measurement record version - update this when updating the measurement
// records
// minor version: update when add/remove the measurement.
// patch version: update when update the functionality of measurement.
// format:  patch, minor LSB, minor MSB, major
typedef struct [[gnu::packed]]
{
    uint8_t patch;
    uint8_t minor_lsb;
    uint8_t minor_msb;
    uint8_t major;
} MeasurementVersionValueT;
constexpr MeasurementVersionValueT MeasurementVersion{0x0, 0x0, 0x0, 0x02};

typedef struct [[gnu::packed]]
{
    uint16_t build;
    uint16_t patch;
    uint8_t  minor;
    uint16_t major;
} MeasurementFirmwareVersionT;

typedef struct [[gnu::packed]]
{
    uint8_t  meas_type;
    uint16_t meas_size;
} MeasInfoT;

// these enums represent the individual index for a particular measurement
typedef enum
: uint8_t
{
    MeasGetNumMeas = 0,  // illegal index, when requested, response is num meas
    MeasVersion,
    MeasReservedIndex2,
    MeasReservedIndex3,
    MeasReservedIndex4,
    MeasMcuActiveFirmwareHash,
    MeasMcuInactiveFirmwareHash,
    MeasReservedIndex7,
    MeasReservedIndex8,
    MeasReservedIndex9,
    MeasReservedIndex10,
    MeasReservedIndex11,
    MeasReservedIndex12,
    MeasFuses,
    MeasRollbackFuses,
    MeasKeyRevocationFuses,
    MeasReservedIndex16,
    MeasReservedIndex17,
    MeasMcuActiveFirmwareSecurityVersion,
    MeasMcuInctiveFirmwareSecurityVersion,
    MeasReservedIndex20,
    MeasReservedIndex21,
    MeasReservedIndex22,
    MeasReservedIndex23,
    MeasReservedIndex24,
    MeasReservedIndex25,
    MeasMcxn236Uuid,
    MeasMcuActiveFirmwareHeaderHash,
    MeasMcuInactiveFirmwareHeaderHash,
    MeasReservedIndex29,
    MeasReservedIndex30,
    MeasReservedIndex31,
    MeasReservedIndex32,
    MeasReservedIndex33,
    MeasReservedIndex34,
    MeasReservedIndex35,
    MeasReservedIndex36,
    MeasReservedIndex37,
    MeasMcuActiveFirmwareVersion,
    MeasMcuInactiveFirmwareVersion,
    MeasReservedIndex40,
    MeasReservedIndex41,
    MeasReservedIndex42,
    MeasBootStatus,
    MeasReservedIndex44,
    MeasReservedIndex45,
    MeasReservedIndex46,
    MeasReservedIndex47,
    MeasReservedIndex48,
    MeasReservedIndex49,
    MeasDebugTokenTlvConfiguration,
    MeasDebugTokenStatus,
    MeasDeviceIdentifiers,
    MeasMax,  // total number of measurments + 1 (1-based index)
} MeasNumT;

// these enums are the status return codes for the verify debug token
typedef enum
{
    DtStatusSuccess = 0,
    DtStatusMcuFwVerSizeMismatch,
    DtStatusDevSerNumSizeMismatch,
    DtStatusNonceSizeMismatch,
    DtStatusFailBadMcuFwVer,
    DtStatusFailBadDevSerNum,
    DtStatusFailBadNonce,
    DtStatusFailNonceValidityUpdate,
    DtStatusFailBadAgentVer,
    DtStatusFailBadTokenVer,
    DtStatusMax,
} DtStatusT;

// this table is the array of the MeasInfoT element.
// {meas_type, meas_size}
constexpr std::array<MeasInfoT, MeasMax> MeasInfos = {
    {{.meas_type = 0, .meas_size = 0},      // MeasGetNumMeas
     {.meas_type = 0x83, .meas_size = 4},   // MeasVersion
     {.meas_type = 0x83, .meas_size = 1},   // MeasReservedIndex2
     {.meas_type = 0x83, .meas_size = 1},   // MeasReservedIndex3
     {.meas_type = 0x1, .meas_size = 48},   // MeasReservedIndex4
     {.meas_type = 0x1, .meas_size = 48},   // MeasMcuActiveFirmwareHash
     {.meas_type = 0x1, .meas_size = 48},   // MeasMcuInactiveFirmwareHash
     {.meas_type = 0x1, .meas_size = 48},   // MeasReservedIndex7
     {.meas_type = 0x1, .meas_size = 48},   // MeasReservedIndex8
     {.meas_type = 0x1, .meas_size = 48},   // MeasReservedIndex9
     {.meas_type = 0x1, .meas_size = 48},   // MeasReservedIndex10
     {.meas_type = 0x1, .meas_size = 48},   // MeasReservedIndex11
     {.meas_type = 0x1, .meas_size = 48},   // MeasReservedIndex12
     {.meas_type = 0x2, .meas_size = 48},   // MeasFuses
     {.meas_type = 0x82, .meas_size = 4},   // MeasRollbackFuses
     {.meas_type = 0x82, .meas_size = 4},   // MeasKeyRevocationFuses
     {.meas_type = 0x2, .meas_size = 48},   // MeasReservedIndex16
     {.meas_type = 0x2, .meas_size = 48},   // MeasReservedIndex17
     {.meas_type = 0x82, .meas_size = 1},   // MeasMcuActiveFirmwareSecurityVersion
     {.meas_type = 0x82, .meas_size = 1},   // MeasMcuInctiveFirmwareSecurityVersion
     {.meas_type = 0x82, .meas_size = 1},   // MeasReservedIndex20
     {.meas_type = 0x82, .meas_size = 1},   // MeasReservedIndex21
     {.meas_type = 0x82, .meas_size = 1},   // MeasReservedIndex22
     {.meas_type = 0x82, .meas_size = 1},   // MeasReservedIndex23
     {.meas_type = 0x82, .meas_size = 1},   // MeasReservedIndex24
     {.meas_type = 0x82, .meas_size = 1},   // MeasReservedIndex25
     {.meas_type = 0x82, .meas_size = 16},  // MeasMcxn236Uuid
     {.meas_type = 0x3, .meas_size = 48},   // MeasMcuActiveFirmwareHeaderHash
     {.meas_type = 0x3, .meas_size = 48},   // MeasMcuInactiveFirmwareHeaderHash
     {.meas_type = 0x3, .meas_size = 48},   // MeasReservedIndex29
     {.meas_type = 0x3, .meas_size = 48},   // MeasReservedIndex30
     {.meas_type = 0x3, .meas_size = 48},   // MeasReservedIndex31
     {.meas_type = 0x3, .meas_size = 48},   // MeasReservedIndex32
     {.meas_type = 0x3, .meas_size = 48},   // MeasReservedIndex33
     {.meas_type = 0x83, .meas_size = 1},   // MeasReservedIndex34
     {.meas_type = 0x83, .meas_size = 1},   // MeasReservedIndex35
     {.meas_type = 0x83, .meas_size = 1},   // MeasReservedIndex36
     {.meas_type = 0x83, .meas_size = 1},   // MeasReservedIndex37
     {.meas_type = 0x83, .meas_size = 7},   // MeasMcuActiveFirmwareVersion
     {.meas_type = 0x83, .meas_size = 7},   // MeasMcuInactiveFirmwareVersion
     {.meas_type = 0x83, .meas_size = 1},   // MeasReservedIndex40
     {.meas_type = 0x83, .meas_size = 1},   // MeasReservedIndex41
     {.meas_type = 0x83, .meas_size = 1},   // MeasReservedIndex42
     {.meas_type = 0x83, .meas_size = 1},   // MeasBootStatus
     {.meas_type = 0x83, .meas_size = 1},   // MeasReservedIndex44
     {.meas_type = 0x83, .meas_size = 1},   // MeasReservedIndex45
     {.meas_type = 0x3, .meas_size = 48},   // MeasReservedIndex46
     {.meas_type = 0x3, .meas_size = 48},   // MeasReservedIndex47
     {.meas_type = 0x3, .meas_size = 48},   // MeasReservedIndex48
     {.meas_type = 0x3, .meas_size = 48},   // MeasReservedIndex49
     {.meas_type = 0x83,
      .meas_size = sizeof(
          nv::debugtoken::DebugTokenTlvConfig)},  // MeasDebugTokenTlvConfiguration
     {.meas_type = 0x83,
      .meas_size = nv::debugtoken::DebugTokenStatsTSize},  // MeasDebugTokenStatus
     {.meas_type = 0x81, .meas_size = 73}}  // MeasDeviceIdentifiers
};

void spdm_get_measurement(MeasNumT index, void* buffer);

constexpr uint64_t calculate_measurement_needed_space(const std::array<MeasInfoT, MeasMax>& arr)
{
    // Index(1) + MeasurementSpecification(1) + MeasurementSize(2) +
    // DMTFSpecMeasurementValueType(1) + DMTFSpecMeasurementValueSize(2)
    constexpr uint32_t MeasurementRecordDataSize = 7;
    // coverity[overflow_before_widen] - This is should not happen
    uint64_t ret = MeasurementRecordDataSize * (arr.size() - 1);
    for (uint32_t i = 1; i < arr.size(); i++) {
        if (ret > std::numeric_limits<decltype(ret)>::max() - arr[i].meas_size) {
            return 0;
        }
        ret += arr[i].meas_size;
    }
    return ret;
};

constexpr static uint32_t Ecdsa384SignatureSize = 384 / 8 * 2;

constexpr uint64_t MeasurementBlockMaxLength = calculate_measurement_needed_space(MeasInfos);
static_assert(MeasurementBlockMaxLength != 0, "MeasurementBlockMaxLength pre-condition failed");
constexpr uint64_t MeasurementRequestLength     = 37;  // with 32 byte Nonce
constexpr uint64_t MeasurementResponseMaxLength = 42 + MeasurementBlockMaxLength
                                                + Ecdsa384SignatureSize;
constexpr uint64_t MeasurementMaximumNeededBufferSize = MeasurementRequestLength
                                                      + MeasurementResponseMaxLength;

constexpr MeasNumT spdm_get_num_meas()
{
    return static_cast<MeasNumT>(MeasMax - 1);
};
void             spdm_init_all_measurement_cache();
const MeasInfoT& spdm_get_meas_record_info(MeasNumT index);

void spdm_get_fw_version(bool is_active_slot, MeasurementFirmwareVersionT& fw_version);

/*
 *  verify_dbg_token_fields()
 *
 *  This function verifies the fields passed in match the fields in the debug
 *  token structure.
 *
 *  [in] mcu_fw_ver - reference to array holding MCU FW version
 *  [in] dev_ser_num - reference to array holding device serial number
 *  [in] nonce - reference to array holding nonce
 *  [in] meas_num - measurement number to verify
 *  [in] token_ver - MCU debug token structure version
 *  [in] agent_ver - agent version value
 *
 *  Returns: DT_STATUS_SUCCESS if all fields match, otherwise returns an error status
 */
DtStatusT verify_dbg_token_fields(
    const std::array<uint8_t, nv::debugtoken::DT_MCU_FW_VER_SIZE>&  mcu_fw_ver,
    const std::array<uint8_t, nv::debugtoken::DT_DEV_SER_NUM_SIZE>& dev_ser_num,
    const std::array<uint8_t, nv::debugtoken::DT_NONCE_SIZE>&       nonce,
    MeasNumT                                                        meas_num,
    uint32_t                                                        token_ver,
    uint16_t                                                        agent_ver);

bool is_pds_dbg_token_nonce_valid();
bool gen_and_save_dbg_token_nonce();
void update_dbg_token_nonce_meas();
bool set_pds_dbg_token_nonce(const std::array<uint8_t, nv::debugtoken::DT_NONCE_SIZE>& nonce);
bool set_pds_dbg_token_nonce_valid(uint32_t valid);
void spdm_get_and_save_dbg_token_nonce(MeasNumT meas_num, bool& nonce_status);
bool get_pds_dbg_token_nonce(std::array<uint8_t, nv::debugtoken::DT_NONCE_SIZE>& nonce);
void get_debug_token_status(nv::debugtoken::DebugTokenStatsT& status);
void get_debug_token_tlv_config(nv::debugtoken::DebugTokenTlvConfig& config);

}  // namespace measurement
}  // namespace spdm
}  // namespace nv

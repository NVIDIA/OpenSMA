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
    nv::common::Uuid        measurement_uuid;
    std::array<uint8_t, 16> measurement_rom_patch_cmac;
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
constexpr MeasurementVersionValueT MeasurementVersion{0x00, 0x01, 0x00, 0x02};

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
    MeasGetNumMeas = 0,        // illegal index, when requested, response is num meas
    MeasVersion,               // Index 1: Measurement Block Version
    MeasApType,                // Index 2: AP Type (CPD/LPU/NON)
    MeasReservedIndex3,        // Index 3: Reserved
    MeasReservedIndex4,        // Index 4: Reserved
    MeasReservedIndex5,        // Index 5: Reserved
    MeasReservedIndex6,        // Index 6: Reserved
    MeasReservedIndex7,        // Index 7: Reserved
    MeasReservedIndex8,        // Index 8: Reserved
    MeasAp1FirmwareHash,       // Index 9: AP1 Firmware
    MeasAp2FirmwareHash,       // Index 10: AP2 Firmware
    MeasReservedIndex11,       // Index 11: Reserved
    MeasReservedIndex12,       // Index 12: Reserved
    MeasFuses,                 // Index 13: Fuses
    MeasRollbackFuses,         // Index 14: Rollback fuses
    MeasKeyRevocationFuses,    // Index 15: Key Revocation fuses
    MeasApRollbackFuses,       // Index 16: AP Rollback fuses
    MeasApKeyRevocationFuses,  // Index 17: AP Key Revocation fuses
    MeasMcuActiveFirmwareSecurityVersion,    // Index 18: Firmware Security Version (Active)
    MeasMcuInactiveFirmwareSecurityVersion,  // Index 19: Firmware Security Version (Inactive)
    MeasReservedIndex20,                     // Index 20: Reserved
    MeasAp1FirmwareSecurityVersion,          // Index 21: AP1 Firmware Security Version
    MeasAp2FirmwareSecurityVersion,          // Index 22: AP2 Firmware Security Version
    MeasReservedIndex23,                     // Index 23: Reserved
    MeasReservedIndex24,                     // Index 24: Reserved
    MeasReservedIndex25,                     // Index 25: Reserved
    MeasMcxnUuid,                            // Index 26: MCXN556 UUID
    MeasAp1ChipId,                           // Index 27: AP1 Chip ID
    MeasAp2ChipId,                           // Index 28: AP2 Chip ID
    MeasReservedIndex29,                     // Index 29: Reserved
    MeasReservedIndex30,                     // Index 30: Reserved
    MeasAp1MetadataHash,                     // Index 31: AP1 metadata hash
    MeasAp2MetadataHash,                     // Index 32: AP2 metadata hash
    MeasReservedIndex33,                     // Index 33: Reserved
    MeasReservedIndex34,                     // Index 34: Reserved
    MeasAp1FuseMask,                         // Index 35: AP1 fuse mask
    MeasAp2FuseMask,                         // Index 36: AP2 fuse mask
    MeasReservedIndex37,                     // Index 37: Reserved
    MeasReservedIndex38,                     // Index 38: Reserved
    MeasReservedIndex39,                     // Index 39: Reserved
    MeasReservedIndex40,                     // Index 40: Reserved
    MeasAp1FirmwareVersion,                  // Index 41: AP1 Firmware Version
    MeasAp2FirmwareVersion,                  // Index 42: AP2 Firmware Version
    MeasMcuBootStatus,                       // Index 43: MCU Boot Status
    MeasAp1BootStatus,                       // Index 44: AP1 Boot Status
    MeasAp2BootStatus,                       // Index 45: AP2 Boot Status
    MeasReservedIndex46,                     // Index 46: Reserved
    MeasReservedIndex47,                     // Index 47: Reserved
    MeasReservedIndex48,                     // Index 48: Reserved
    MeasReservedIndex49,                     // Index 49: Reserved
    MeasDebugTokenTlvConfiguration,          // Index 50: Debug Token Configuration
    MeasDebugTokenStatus,                    // Index 51: Debug Token Status
    MeasDeviceIdentifiers,                   // Index 52: Device Identifiers
    MeasMax,                                 // total number of measurments + 1 (1-based index)
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
    DtStatusFailBadLifecycleState,
    DtStatusMax,
} DtStatusT;

// this table is the array of the MeasInfoT element.
// {meas_type, meas_size}

constexpr std::array<MeasInfoT, MeasMax> MeasInfos = {
    {{.meas_type = 0, .meas_size = 0},      // MeasGetNumMeas
     {.meas_type = 0x83, .meas_size = 4},   // MeasVersion (Index 1)
     {.meas_type = 0x83, .meas_size = 4},   // MeasApType (Index 2: AP Type)
     {.meas_type = 0x83, .meas_size = 1},   // MeasReservedIndex3
     {.meas_type = 0x1, .meas_size = 48},   // MeasReservedIndex4
     {.meas_type = 0x1, .meas_size = 48},   // MeasReservedIndex5
     {.meas_type = 0x1, .meas_size = 48},   // MeasReservedIndex6
     {.meas_type = 0x1, .meas_size = 48},   // MeasReservedIndex7
     {.meas_type = 0x1, .meas_size = 48},   // MeasReservedIndex8
     {.meas_type = 0x1, .meas_size = 48},   // MeasAp1FirmwareHash
     {.meas_type = 0x1, .meas_size = 48},   // MeasAp2FirmwareHash
     {.meas_type = 0x1, .meas_size = 48},   // MeasReservedIndex11
     {.meas_type = 0x1, .meas_size = 48},   // MeasReservedIndex12
     {.meas_type = 0x2, .meas_size = 48},   // MeasFuses
     {.meas_type = 0x82, .meas_size = 4},   // MeasRollbackFuses
     {.meas_type = 0x82, .meas_size = 4},   // MeasKeyRevocationFuses
     {.meas_type = 0x82, .meas_size = 4},   // MeasApRollbackFuses
     {.meas_type = 0x82, .meas_size = 4},   // MeasApKeyRevocationFuses
     {.meas_type = 0x87, .meas_size = 8},   // MeasMcuActiveFirmwareSecurityVersion
     {.meas_type = 0x87, .meas_size = 8},   // MeasMcuInactiveFirmwareSecurityVersion
     {.meas_type = 0x82, .meas_size = 1},   // MeasReservedIndex20
     {.meas_type = 0x87, .meas_size = 8},   // MeasAp1FirmwareSecurityVersion
     {.meas_type = 0x87, .meas_size = 8},   // MeasAp2FirmwareSecurityVersion
     {.meas_type = 0x82, .meas_size = 1},   // MeasReservedIndex23
     {.meas_type = 0x82, .meas_size = 1},   // MeasReservedIndex24
     {.meas_type = 0x82, .meas_size = 1},   // MeasReservedIndex25
     {.meas_type = 0x82, .meas_size = 16},  // MeasMcxnUuid
     {.meas_type = 0x82, .meas_size = 16},  // MeasAp1ChipId
     {.meas_type = 0x82, .meas_size = 16},  // MeasAp2ChipId
     {.meas_type = 0x3, .meas_size = 48},   // MeasReservedIndex29
     {.meas_type = 0x3, .meas_size = 48},   // MeasReservedIndex30
     {.meas_type = 0x3, .meas_size = 48},   // MeasAp1MetadataHash
     {.meas_type = 0x3, .meas_size = 48},   // MeasAp2MetadataHash
     {.meas_type = 0x3, .meas_size = 48},   // MeasReservedIndex33
     {.meas_type = 0x83, .meas_size = 1},   // MeasReservedIndex34
     {.meas_type = 0x83, .meas_size = 4},   // MeasAp1FuseMask
     {.meas_type = 0x83, .meas_size = 4},   // MeasAp2FuseMask
     {.meas_type = 0x83, .meas_size = 1},   // MeasReservedIndex37
     {.meas_type = 0x86, .meas_size = 7},   // MeasReservedIndex38
     {.meas_type = 0x86, .meas_size = 7},   // MeasReservedIndex39
     {.meas_type = 0x83, .meas_size = 1},   // MeasReservedIndex40
     {.meas_type = 0x86, .meas_size = 4},   // MeasAp1FirmwareVersion
     {.meas_type = 0x86, .meas_size = 4},   // MeasAp2FirmwareVersion
     {.meas_type = 0x83, .meas_size = 1},   // MeasMcuBootStatus (Index 43: MCU Boot Status)
     {.meas_type = 0x83, .meas_size = 1},   // MeasAp1BootStatus
     {.meas_type = 0x83, .meas_size = 1},   // MeasAp2BootStatus
     {.meas_type = 0x3, .meas_size = 48},   // MeasReservedIndex46
     {.meas_type = 0x3, .meas_size = 48},   // MeasReservedIndex47
     {.meas_type = 0x3, .meas_size = 48},   // MeasReservedIndex48
     {.meas_type = 0x3, .meas_size = 48},   // MeasReservedIndex49
     {.meas_type = 0x83, .meas_size = 97},  // MeasDebugTokenTlvConfiguration
     {.meas_type = 0x83, .meas_size = 1},   // MeasDebugTokenStatus
     {.meas_type = 0x81, .meas_size = 88}}  // MeasDeviceIdentifiers
};

static_assert(sizeof(nv::debugtoken::DebugTokenTlvConfig) == 97,
              "DebugTokenTlvConfig structure changed");

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
    uint16_t                                                        agent_ver,
    uint8_t                                                         lifecycle_state);

bool is_pds_dbg_token_nonce_valid();
bool gen_and_save_dbg_token_nonce();
void update_dbg_token_nonce_meas();
bool set_pds_dbg_token_nonce(const std::array<uint8_t, nv::debugtoken::DT_NONCE_SIZE>& nonce);
bool set_pds_dbg_token_nonce_valid(uint32_t valid);
void spdm_get_and_save_dbg_token_nonce(MeasNumT meas_num, bool& nonce_status);
bool get_pds_dbg_token_nonce(std::array<uint8_t, nv::debugtoken::DT_NONCE_SIZE>& nonce);
void get_debug_token_status(nv::debugtoken::DebugTokenStatsT& status);
nv::debugtoken::LifecycleState get_device_lifecycle_state();
void get_debug_token_tlv_config(nv::debugtoken::DebugTokenTlvConfig& config);

}  // namespace measurement
}  // namespace spdm
}  // namespace nv

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
#include "corepdk/modules/pldm-fd/src/types/nvtypes.h"
#include "nv/ipc/queue.h"

extern "C" {
namespace nv {
namespace pldm {

#define NV_PLDM_TYPE_MSG_CTRL_AND_DISCOVERY 0
#define NV_PLDM_TYPE_FIRMWARE_UPDATE        5

//  base
#define NV_PLDM_MAX_PLDM_RESP_SIZE 37
#define NV_PLDM_MAX_PLDM_CMDS      32
#define NV_PLDM_MAX_PLDM_CAP       2

//  fwupdate

#define NV_PLDM_PRIV_MCTP_HDR_SIZE 4
#define NV_PLDM_MCTP_HDR_SIZE      4U
#define NV_PLDM_MSG_TYPE_SIZE      1U
#define NV_PLDM_MCTP_PAYLOAD_SIZE  64

#define NV_PLDM_BASE_RESP_SIZE 4  //  pldm header <3 bytes> + CC <1 byte>
#define NV_PLDM_MAX_TX_SIZE    (0x100)

#define NV_PLDM_RET_SUCCUSS                     (0)
#define NV_PLDM_RET_IMAGE_WRITE_FAIL            (21)
#define NV_PLDM_RET_BOOTFSM_LEDGER_PROGRAM_FAIL (25)
#define NV_PLDM_FW_RET_WP_ON                    (32)
#define NV_PLDM_RET_GET_PDS_STATE_FAIL          (33)

constexpr size_t NV_PLDM_RESERVE_HEADER_SIZE = 32;
constexpr size_t NV_PLDM_MAX_PAYLOAD_SIZE    = 0x1000;
constexpr size_t NV_PLDM_2K_PAYLOAD_SIZE     = 0x800;
constexpr size_t NV_PLDM_1K_PAYLOAD_SIZE     = 0x400;
constexpr size_t NV_PLDM_512_PAYLOAD_SIZE    = 0x200;
constexpr size_t NV_PLDM_256_PAYLOAD_SIZE    = 0x100;
constexpr size_t NV_PLDM_128_PAYLOAD_SIZE    = 0x80;
constexpr size_t NV_PLDM_64_PAYLOAD_SIZE     = 0x40;
constexpr size_t NV_PLDM_32_PAYLOAD_SIZE     = 0x20;
constexpr size_t NV_PLDM_MAX_TRANSFER_SIZE   = NV_PLDM_RESERVE_HEADER_SIZE
                                           + NV_PLDM_MAX_PAYLOAD_SIZE;
constexpr size_t NV_PLDM_2K_TRANSFER_SIZE = NV_PLDM_RESERVE_HEADER_SIZE
                                          + NV_PLDM_2K_PAYLOAD_SIZE;
constexpr size_t NV_PLDM_1K_TRANSFER_SIZE = NV_PLDM_RESERVE_HEADER_SIZE
                                          + NV_PLDM_1K_PAYLOAD_SIZE;
constexpr size_t NV_PLDM_512_TRANSFER_SIZE = NV_PLDM_RESERVE_HEADER_SIZE
                                           + NV_PLDM_512_PAYLOAD_SIZE;
constexpr size_t NV_PLDM_256_TRANSFER_SIZE = NV_PLDM_RESERVE_HEADER_SIZE
                                           + NV_PLDM_256_PAYLOAD_SIZE;
constexpr size_t NV_PLDM_128_TRANSFER_SIZE = NV_PLDM_RESERVE_HEADER_SIZE
                                           + NV_PLDM_128_PAYLOAD_SIZE;
constexpr size_t NV_PLDM_64_TRANSFER_SIZE = NV_PLDM_RESERVE_HEADER_SIZE
                                          + NV_PLDM_64_PAYLOAD_SIZE;
constexpr size_t NV_PLDM_32_TRANSFER_SIZE = NV_PLDM_RESERVE_HEADER_SIZE
                                          + NV_PLDM_32_PAYLOAD_SIZE;

#define NV_PLDM_ERASE_SIZE            (0x2000UL)
#define NV_PLDM_ERASE_ALIGNMENT       (NV_PLDM_ERASE_SIZE - 1)
#define NV_PLDM_FLASH_PAGE_SIZE       (256)
#define NV_PLDM_MAX_4K_RX_QUEUE_SIZE  (NV_PLDM_MAX_TRANSFER_SIZE)
#define NV_PLDM_MCTP_HDR_PAYLOAD_SIZE (NV_PLDM_MCTP_HDR_SIZE + NV_PLDM_MCTP_PAYLOAD_SIZE)
#define NV_PLDM_BASE_RX_QUEUE_SIZE    (NV_PLDM_PRIV_MCTP_HDR_SIZE + NV_PLDM_MCTP_HDR_PAYLOAD_SIZE)
#define NV_PLDM_SHIFT_BYTES                                                                    \
    (NV_PLDM_PRIV_MCTP_HDR_SIZE + NV_PLDM_MCTP_HDR_SIZE + NV_PLDM_MSG_TYPE_SIZE)

#define NV_PLDM_NON_IDEMPOTENT_RESP_SIZE 16
#define NV_PLDM_MAX_COMPONENT_SIZE       3
#define NV_PLDM_IID_PAIR_SIZE            8

// @todo once we have crypto lib we should move below information to it
// https://confluence.nvidia.com/pages/viewpage.action?spaceKey=GFWBC&title=MCU+SMA+Storage+layout+and+image+layout
// interrupt
#define NV_PLDM_CERT_BLOCK_OFFSET                   0x28
// header
#define NV_PLDM_NV_HEADER_OFFSET                    0x2b0
#define NV_PLDM_MAJOR_OFFSET                        0x06
#define NV_PLDM_MINOR_OFFSET                        0x08
#define NV_PLDM_PATCH_OFFSET                        0x09
#define NV_PLDM_BUILD_OFFSET                        0x0b
#define NV_PLDM_APSKU_OFFSET                        0x11
#define NV_PLDM_KEY_REVOKE_LIST_OFFSET              0x1D
#define NV_PLDM_METADATA_VERSION_INFO_RESERVED_SIZE 64
// cert block
#define NV_PLDM_KEY_IDX_OFFSET                      0x134

// crypto to pldm verify CC
#define NV_PLDM_VERIFY_CC_SUCCESS         0
#define NV_PLDM_VERIFY_CC_ERROR           1
#define NV_PLDM_VERIFY_CC_ROLLBACK_FAIL   0x93
#define NV_PLDM_VERIFY_CC_KEY_FAIL        0x94
#define NV_PLDM_VERIFY_CC_IMAGE_AUTH_FAIL 0x95

//  base
struct [[gnu::packed]] PldmCapability
{
    NvU8  p_type;
    NvU32 ver;
    NvU8  cmds[NV_PLDM_MAX_PLDM_CMDS];
};

//  fwupdate
enum class PldmApFwStatus : NvU8
{
    Update_In_Progress,
    Update_Complete,
    Staged,
};

struct [[gnu::packed]] IidU8
{
    NvU8 iid : 5;
    NvU8 rsv : 3;
};

struct [[gnu::packed]] NonIdempotentIidCmdResp
{
    IidU8 iid;
    NvU8  cmd;
    NvU32 len;
    NvU8  resp[NV_PLDM_NON_IDEMPOTENT_RESP_SIZE];
};

struct [[gnu::packed]] CompSizeRecord
{
    NvU8  valid_size;
    NvU16 comp_arr[NV_PLDM_MAX_COMPONENT_SIZE];
};

struct [[gnu::packed]] IidOffsetPair
{
    IidU8 iid;
    NvU32 offset;
};

struct [[gnu::packed]] IidOffsetList
{
    NvU8          idx;
    IidOffsetPair pairs[NV_PLDM_IID_PAIR_SIZE];
};

struct [[gnu::packed]] PldmfwInterfaceInfo
{
    NvU8 client;
    NvU8 src_eid;
    NvU8 header_version;
    NvU8 message_tag;
};

struct [[gnu::packed]] PldmfwSettingTable
{
    NvU8 is_inband_supported : 1;
    NvU8 rsv                 : 7;
};

struct [[gnu::packed]] PldmfwDataBuffer
{
    NvU8  data[NV_PLDM_256_PAYLOAD_SIZE];
    NvU32 size;
};

struct [[gnu::packed]] Metadata
{
    NvU16 major;      //  major
    NvU8  minor;      //  minor
    NvU16 patch;      //  patch
    NvU16 build;      //  build
    NvU32 ap_sku_id;  //  AP SKU ID
};
static_assert(sizeof(Metadata) <= NV_PLDM_METADATA_VERSION_INFO_RESERVED_SIZE);

struct [[gnu::packed]] LimitedRateInfo
{
    NvU8  min;
    NvU8  max;
    NvU32 time;
};

struct PldmContextRecord
{
    NvU8  pldm_state;           //  pldm state machine
    NvU8  pldm_previous_state;  //  pldm previouse state
    NvU8  idle_reason_code;     //  idle reason
    NvU32 max_transfer_size;    //  UA requests max transfer size
    NvU16 comp_id;              //  component id
    NvU32 fw_size;              //  Fw size
    NvU32 fw_current_offset;    //  Current fw offset
    NvU32 fw_update_size;       //  Update size
    NvU32 last_transfer_size;   //  Transfer size of RequestFwData requested by FD
    NvU32 wr_spi_addr;          //  Write spi address
    bool  is_force_update;      //  is force update or not
    IidU8 iid;                  //  iid for req cmd
    NvU8  composer_buffer[NV_PLDM_MAX_TX_SIZE];      //  composer msg payload
    NvU8  slot_state;                                //  Recording pldm slot state
    NonIdempotentIidCmdResp iid_cmd;                 //  non idempotent iid response
    CompSizeRecord          pass_comp_size;          //  Pass component size record
    CompSizeRecord          verify_comp_size;        //  Verify complete component size record
    CompSizeRecord          activate_comp_size;      //  Activate component size record
    IidOffsetList           req_fw_data_list;        //  iid offset pairs for request fw data
    NvU8                    req_retry;               //  requester retry count
    NvU8                    spi_flash_retry;         //  spiflash retry count
    NvU32                   total_retry;             //  total retry
    bool                    is_inforom_ro_activate;  //  If inforomro activate
    NvU8                    rx_msg[NV_PLDM_MAX_TRANSFER_SIZE];  //  Rx memory from UA
    PldmfwInterfaceInfo     rx_interface;           //  Rx fwupdate interface inband/outband
    PldmfwInterfaceInfo     update_interface;       //  keep fwupdate interface inband/outband
    NvU8                    is_inband_enable;       //  Inband update
    bool                    is_backup_in_progress;  //  Is bgcopy in progress
    PldmfwSettingTable      pldm_settings;          //  pldm setting config
    NvU32                   dl_send_time;           //  request fw data time
    NvU32                   dl_m0_time;             //  transfer 4k time
    NvU32                   dl_m1_time;             //  erase/program 4k time
    NvU8                    transfer_fail_cc;       //  record fail cc during download state
    NvU8                    verify_fail_cc;         //  record fail cc for verify
    NvU8                    current_rate;           //  current rate
    NvU8                    limited_rate;  //  limited rate 16 => 8 => 4 => 2 => 2 => 2 ...
    bool                    is_limited_rate_reached;  //  is rate limit reached
    NvU32                   rate_limit_timestamp;     //  rate limit timestamp
    bool                    is_recv_all_metadata;     //  receive all version info
    bool             is_metadata_check_done;  // record if finish check the first dl chunk
    PldmfwDataBuffer fw_data_buffer;          //  buffer to store the received fw data
    NvU8  metadata_version_info[NV_PLDM_METADATA_VERSION_INFO_RESERVED_SIZE];  //  metadata
    NvU32 auth_request_id;   //  authentication request id
    NvU8  write_fail_retry;  //  write fail retry count
};

struct PldmBaseContextRecord
{
    NvU8                tid;
    NvU8                req[NV_PLDM_MAX_PLDM_RESP_SIZE];
    PldmCapability      caps[NV_PLDM_MAX_PLDM_CAP];
    PldmfwInterfaceInfo rx_interface;
};

void send_pldm_msg_to_mctp(NvU8* data,
                           NvU8  client,
                           NvU8  src_eid,
                           NvU8  header_version,
                           NvU8  message_tag,
                           NvU32 size,
                           NvU8  is_request);

void pldm_get_active_version(NvU16& major, NvU8& minor, NvU16& patch, NvU16& build);

void pldm_get_pending_version(NvU16& major, NvU8& minor, NvU16& patch, NvU16& build);

void get_mcu_authentication_result(uint8_t result, uint8_t& verify_cc);
void get_ap_authentication_result(uint16_t comp_id, uint8_t result, uint8_t& verify_cc);

}  // namespace pldm
}  // namespace nv
}

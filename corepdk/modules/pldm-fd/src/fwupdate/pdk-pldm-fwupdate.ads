--
--  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
--  All rights reserved.
--  SPDX-License-Identifier: Apache-2.0
--
--  Licensed under the Apache License, Version 2.0 (the "License");
--  you may not use this file except in compliance with the License.
--  You may obtain a copy of the License at
--
--  http://www.apache.org/licenses/LICENSE-2.0
--
--  Unless required by applicable law or agreed to in writing, software
--  distributed under the License is distributed on an "AS IS" BASIS,
--  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
--  See the License for the specific language governing permissions and
--  limitations under the License.
--

--  @summary
--  PLDM defines and routines
--
--  @description
--  This package contains PLDM - T5 fw update define.
--

with Ada.Unchecked_Conversion;
with Nv_Types;                      use Nv_Types;
with Nv_Types.Array_Types;          use Nv_Types.Array_Types;
with Pdk.Pldm.Interfaces.Plattypes; use Pdk.Pldm.Interfaces.Plattypes;
with Pdk.Pldm.Packet;               use Pdk.Pldm.Packet;

package Pdk.Pldm.Fwupdate is

   PLDMFW_BASE_TRANSFER_SIZE : constant := 16#20#;

   --  PLDM Fwupdate  command defines.
   PLDM_FWUPDATE_CMD_LSB_VER : constant := 16#F1F0_F000#;

   --  pldm fw update CC
   PLDMFW_CC_NOT_IN_UPDATE_MODE                      : constant := 16#80#;
   PLDMFW_CC_ALREADY_IN_UPDATE_MODE                  : constant := 16#81#;
   PLDMFW_CC_DATA_OUT_OF_RANGE                       : constant := 16#82#;
   PLDMFW_CC_INVALID_TRANSFER_LENGTH                 : constant := 16#83#;
   PLDMFW_CC_INVALID_STATE_FOR_CMD                   : constant := 16#84#;
   PLDMFW_CC_INCOMPLETE_UPDATE                       : constant := 16#85#;
   PLDMFW_CC_BUSY_IN_BACKGROUND                      : constant := 16#86#;
   PLDMFW_CC_CANCEL_PENDING                          : constant := 16#87#;
   PLDMFW_CC_CMD_NOT_EXPECTED                        : constant := 16#88#;
   PLDMFW_CC_RETRY_REQUEST_FW_DATA                   : constant := 16#89#;
   PLDMFW_CC_UNABLE_TO_INIT_UPDATE                   : constant := 16#8a#;
   PLDMFW_CC_ACTIVATION_NOT_REQUIRED                 : constant := 16#8b#;
   PLDMFW_CC_SELF_CONTAINED_ACTIVATION_NOT_PERMITTED : constant := 16#8c#;
   PLDMFW_CC_NO_DEVICE_METADATA                      : constant := 16#8d#;
   PLDMFW_CC_RETRY_REQUEST_UPDATE                    : constant := 16#8e#;
   PLDMFW_CC_NO_PKG_DATA                             : constant := 16#8f#;
   PLDMFW_CC_INVALID_TRANSFER_HANDLE                 : constant := 16#90#;
   PLDMFW_CC_INVALID_TRANSFER_OPERATION              : constant := 16#91#;
   PLDMFW_CC_ACTIVATE_PENDING_IMG_NOT_PERMITTED      : constant := 16#92#;
   PLDMFW_CC_PKG_DATA_ERROR                          : constant := 16#93#;

   PLDMFW_COMMAND_QUERY_DEVICE_ID         : constant := 16#01#;
   PLDMFW_COMMAND_GET_FW_PARAMETERS       : constant := 16#02#;
   PLDMFW_COMMAND_REQUEST_UPDATE          : constant := 16#10#;
   PLDMFW_COMMAND_PASS_COMPONENT_TABLE    : constant := 16#13#;
   PLDMFW_COMMAND_UPDATE_COMPONENT        : constant := 16#14#;
   PLDMFW_COMMAND_REQUEST_FW_DATA         : constant := 16#15#;
   PLDMFW_COMMAND_TRANSFER_COMPLETE       : constant := 16#16#;
   PLDMFW_COMMAND_VERIFY_COMPLETE         : constant := 16#17#;
   PLDMFW_COMMAND_APPLY_COMPLETE          : constant := 16#18#;
   PLDMFW_COMMAND_ACTIVATE_FW             : constant := 16#1a#;
   PLDMFW_COMMAND_GET_STATUS              : constant := 16#1b#;
   PLDMFW_COMMAND_CANCEL_UPDATE_COMPONENT : constant := 16#1c#;
   PLDMFW_COMMAND_CANCEL_UPDATE           : constant := 16#1d#;
   PLDMFW_COMMAND_INVALID                 : constant := 16#ff#;

   PLDMFW_STATE_IDLE     : constant := 0;
   PLDMFW_STATE_LC       : constant := 1;
   PLDMFW_STATE_RDY_XFER : constant := 2;
   PLDMFW_STATE_DL       : constant := 3;
   PLDMFW_STATE_VERIFY   : constant := 4;
   PLDMFW_STATE_APPLY    : constant := 5;
   PLDMFW_STATE_ACTIVATE : constant := 6;

   subtype Pldmfw_State is NvU8 range PLDMFW_STATE_IDLE .. PLDMFW_STATE_ACTIVATE;

   PLDMFW_FD_TIMEOUT_T1     : constant := 60_000; --  60 secs
   PLDMFW_FD_TIMEOUT_T2     : constant := 5_000;  --  5 secs
   PLDMFW_FD_TIMEOUT_VERIFY : constant := 30_000; --  30 secs

   PLDMFW_TIMEING_PN1    : constant := 3; --  retry 3 times
   PLDMFW_TIMEING_PT2    : constant := 4_800; --  4.8 secs
   PLDMFW_SPIFLASH_RETRY : constant := (PLDMFW_FD_TIMEOUT_T1 / PLDMFW_TIMEING_PT2);
   MSEC_TO_NSEC          : constant := 16#000F_4240#;

   DESCRIPTOR_ID_TYPE_PCI_VENDOR_ID           : constant NvU16 := 16#0000#;
   DESCRIPTOR_ID_TYPE_IANA_ENTERPRISE_ID      : constant NvU16 := 16#0001#;
   DESCRIPTOR_ID_TYPE_UUID                    : constant NvU16 := 16#0002#;
   DESCRIPTOR_ID_TYPE_PCI_DEVICE_ID           : constant NvU16 := 16#0100#;
   DESCRIPTOR_ID_TYPE_PCI_SUBSYSTEM_VENDOR_ID : constant NvU16 := 16#0101#;
   DESCRIPTOR_ID_TYPE_PCI_SUBSYSTEM_DEVICE_ID : constant NvU16 := 16#0102#;
   DESCRIPTOR_ID_TYPE_VENDOR_DEFINED          : constant NvU16 := 16#ffff#;

   DESCRIPTOR_ID_LENS_VENDOR_DEFINED          : constant :=
     0;  --  vendor defined dont have standard lens
   DESCRIPTOR_ID_LENS_PCI_VENDOR_ID           : constant := 2;
   DESCRIPTOR_ID_LENS_PCI_DEVICE_ID           : constant := 2;
   DESCRIPTOR_ID_LENS_PCI_SUBSYSTEM_VENDOR_ID : constant := 2;
   DESCRIPTOR_ID_LENS_PCI_SUBSYSTEM_DEVICE_ID : constant := 2;
   DESCRIPTOR_ID_LENS_IANA_ENTERPRISE_ID      : constant := 4;
   DESCRIPTOR_ID_LENS_UUID                    : constant := 16;

   FW_STR_TYPE_UNKNOWN : constant NvU8 := 0;
   FW_STR_TYPE_ASCII   : constant NvU8 := 1;

   PLDMFW_COMP_COMPATIBILITY_RESPONSE_CAN_BE_UPDATE    : constant NvU8 := 0;
   PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE : constant NvU8 := 1;

   PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_CAN_BE_UPDATE       : constant NvU8 := 0;
   PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_STAMP_IDENTICAL     : constant NvU8 := 1;
   PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_STAMP_LOWER         : constant NvU8 := 2;
   PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_SUPPORT         : constant NvU8 := 6;
   PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_MATCH_PASS_COMP : constant NvU8 := 9;

   PLDMFW_CMD_PASS_COMP_TRANSFER_FLAG_START     : constant := 1;
   PLDMFW_CMD_PASS_COMP_TRANSFER_FLAG_MID       : constant := 2;
   PLDMFW_CMD_PASS_COMP_TRANSFER_FLAG_END       : constant := 4;
   PLDMFW_CMD_PASS_COMP_TRANSFER_FLAG_START_END : constant := 5;

   PLDMFW_CMD_TRANSFER_COMPLETE_RESULT_SUCCESS        : constant NvU8 := 16#00#;
   PLDMFW_CMD_TRANSFER_COMPLETE_RESULT_ERROR          : constant NvU8 := 16#0a#;
   PLDMFW_CMD_TRANSFER_COMPLETE_RESULT_WP_ERROR       : constant NvU8 := 16#71#;
   PLDMFW_CMD_TRANSFER_COMPLETE_RESULT_INTERNAL_ERROR : constant NvU8 := 16#72#;

   PLDMFW_CMD_VERIFY_COMPLETE_RESULT_SUCCESS              : constant NvU8 := 0;
   PLDMFW_CMD_VERIFY_COMPLETE_RESULT_ERROR                : constant NvU8 := 1;
   PLDMFW_CMD_VERIFY_COMPLETE_RESULT_ROLLBACK_REVOKE_FAIL : constant NvU8 := 16#93#;
   PLDMFW_CMD_VERIFY_COMPLETE_RESULT_KEY_REVOKE_FAIL      : constant NvU8 := 16#94#;
   PLDMFW_CMD_VERIFY_COMPLETE_RESULT_IMAGE_AUTH_FAIL      : constant NvU8 := 16#95#;
   PLDMFW_CMD_VERIFY_COMPLETE_RESULT_VERSION_MISMATCH     : constant NvU8 := 16#97#;
   PLDMFW_CMD_VERIFY_COMPLETE_RESULT_STAGE_LOWER_VERSION  : constant NvU8 := 16#9C#;

   PLDMFW_CMD_GET_STATUS_AUX_IN_PROGRESS    : constant NvU8 := 0;
   PLDMFW_CMD_GET_STATUS_AUX_SUCCESS        : constant NvU8 := 1;
   PLDMFW_CMD_GET_STATUS_AUX_NOT_IN_OPERATE : constant NvU8 := 3; --  idle/lc/rdy_xfer state

   PLDMFW_CMD_GET_STATUS_REASON_INIT_FW_DEVICE   : constant NvU8 := 0;
   PLDMFW_CMD_GET_STATUS_REASON_ACTIVE_FW        : constant NvU8 := 1;
   PLDMFW_CMD_GET_STATUS_REASON_CANCEL_UPDATE    : constant NvU8 := 2;
   PLDMFW_CMD_GET_STATUS_REASON_TIMEOUT_LC       : constant NvU8 := 3;
   PLDMFW_CMD_GET_STATUS_REASON_TIMEOUT_RDY_XFER : constant NvU8 := 4;
   PLDMFW_CMD_GET_STATUS_REASON_TIMEOUT_DL       : constant NvU8 := 5;
   PLDMFW_CMD_GET_STATUS_REASON_TIMEOUT_VERIFY   : constant NvU8 := 6;
   PLDMFW_CMD_GET_STATUS_REASON_TIMEOUT_APPLY    : constant NvU8 := 7;

   --  @todo inband update is not related to pldm
   --  it would be better to move to related module
   type Ib_Fw_Update_Flag_Type is
     (IB_FW_UPDATE_FLAG_INB_DISALLOWED, IB_FW_UPDATE_FLAG_INB_ALLOWED) with
     Size => 8, Object_Size => 8;

   for Ib_Fw_Update_Flag_Type use
     (IB_FW_UPDATE_FLAG_INB_DISALLOWED => 0,
      IB_FW_UPDATE_FLAG_INB_ALLOWED    => 1);

   type Pldmfw_Slot_State is
     (Pldmfw_Slot_State_Init,
      Pldmfw_Slot_State_Trans_Comp, --  dl state
      Pldmfw_Slot_State_Trans_Error,
      Pldmfw_Slot_State_Sec_Ver_Fail, Pldmfw_Slot_State_Spiflash_Error,
      Pldmfw_Slot_State_Auth_Fail,  --  verify state
      Pldmfw_Slot_State_Auth_Success) with
     Size => 8, Object_Size => 8, Alignment => 1;

   type Pldmfw_Ret is
     (Pldmfw_Ret_Success, Pldmfw_Ret_Over_Size, Pldmfw_Ret_Update_Comp_Version_Identical,
      Pldmfw_Ret_Update_Comp_Version_Lower, Pldmfw_Ret_Recv_Unfulfill_Pkt,
      Pldmfw_Ret_Invalid_Slot_State, Pldmfw_Ret_UA_Send_Error_CC_Req_Fwdata,
      Pldmfw_Ret_Fail_Auth_fw, Pldmfw_Ret_Unsupport_Cmd, Pldmfw_Ret_Send_Non_Idempotent_Resp,
      Pldmfw_Ret_Fail_Update_Component_Not_Match, Pldmfw_Ret_Invalid_Lens,
      Pldmfw_Ret_Pass_3More_Component, Pldmfw_Ret_Pending_Img_Not_Permitted,
      Pldmfw_Ret_Unsupport_Comp_Id, Pldmfw_Ret_Recv_Repeat_Pkt, Pldmfw_Ret_Fail_Set_Timer,
      Pldmfw_Ret_Fail_Clear_Timer, Pldmfw_Ret_Invalid_Response_Size,
      Pldmfw_Ret_Pass_Comp_Size_Invalid, Pldmfw_Ret_Query_Vdpa_Fail,
      Pldmfw_Ret_Image_Write_Fail, Pldmfw_Ret_Recv_Update_Cmd_From_Diff_Interface,
      Pldmfw_Ret_Ib_Update_Disabled, Pldmfw_Ret_Background_Copy_In_Progress,
      Pldmfw_Ret_Bootfsm_Ledger_Program_Fail, Pldmfw_Ret_Set_Write_Protect_Fail,
      Pldmfw_Ret_Authenticate_Fail, Pldmfw_Ret_Timeout_Invalid_Pldm_State,
      Pldmfw_Ret_Variable_Overflow, Pldmfw_Ret_Ap_Sku_Id_Mismatch,
      Pldmfw_Ret_Version_Check_Fail, Pldmfw_Ret_Wp_On, Pldmfw_Ret_Get_Pds_State_Fail,
      Pldmfw_Ret_Staged_Fw_Identical, Pldmfw_Ret_Staged_Fw_Lower_Stamp,
      Pldmfw_Ret_Pass_Zero_Component, Pldmfw_Ret_Invalid_Ver_str_Len_and_Type,
      Pldmfw_Ret_Invalid_Class_or_Class_Idx, Pldmfw_Ret_Invalid_authenticate_event,
      Pldmfw_Ret_Rate_Limit_Reached) with
     Size => 16, Object_Size => 16, Alignment => 1;

   for Pldmfw_Ret use
     (Pldmfw_Ret_Success                             => 0,
      Pldmfw_Ret_Over_Size                           => 1,
      Pldmfw_Ret_Update_Comp_Version_Identical       => 2,
      Pldmfw_Ret_Update_Comp_Version_Lower           => 3,
      Pldmfw_Ret_Recv_Unfulfill_Pkt                  => 4,
      Pldmfw_Ret_Invalid_Slot_State                  => 5,
      Pldmfw_Ret_UA_Send_Error_CC_Req_Fwdata         => 6,
      Pldmfw_Ret_Fail_Auth_fw                        => 7,
      Pldmfw_Ret_Unsupport_Cmd                       => 8,
      Pldmfw_Ret_Send_Non_Idempotent_Resp            => 9,
      Pldmfw_Ret_Fail_Update_Component_Not_Match     => 10,
      Pldmfw_Ret_Invalid_Lens                        => 11,
      Pldmfw_Ret_Pass_3More_Component                => 12,
      Pldmfw_Ret_Pending_Img_Not_Permitted           => 13,
      Pldmfw_Ret_Unsupport_Comp_Id                   => 14,
      Pldmfw_Ret_Recv_Repeat_Pkt                     => 15,
      Pldmfw_Ret_Fail_Set_Timer                      => 16,
      Pldmfw_Ret_Fail_Clear_Timer                    => 17,
      Pldmfw_Ret_Invalid_Response_Size               => 18,
      Pldmfw_Ret_Pass_Comp_Size_Invalid              => 19,
      Pldmfw_Ret_Query_Vdpa_Fail                     => 20,
      Pldmfw_Ret_Image_Write_Fail                    => 21,
      Pldmfw_Ret_Recv_Update_Cmd_From_Diff_Interface => 22,
      Pldmfw_Ret_Ib_Update_Disabled                  => 23,
      Pldmfw_Ret_Background_Copy_In_Progress         => 24,
      Pldmfw_Ret_Bootfsm_Ledger_Program_Fail         => 25,
      Pldmfw_Ret_Set_Write_Protect_Fail              => 26,
      Pldmfw_Ret_Authenticate_Fail                   => 27,
      Pldmfw_Ret_Timeout_Invalid_Pldm_State          => 28,
      Pldmfw_Ret_Variable_Overflow                   => 29,
      Pldmfw_Ret_Ap_Sku_Id_Mismatch                  => 30,
      Pldmfw_Ret_Version_Check_Fail                  => 31,
      Pldmfw_Ret_Wp_On                               => 32,
      Pldmfw_Ret_Get_Pds_State_Fail                  => 33,
      Pldmfw_Ret_Staged_Fw_Identical                 => 34,
      Pldmfw_Ret_Staged_Fw_Lower_Stamp               => 35,
      Pldmfw_Ret_Pass_Zero_Component                 => 36,
      Pldmfw_Ret_Invalid_Ver_str_Len_and_Type        => 37,
      Pldmfw_Ret_Invalid_Class_or_Class_Idx          => 38,
      Pldmfw_Ret_Invalid_authenticate_event          => 39,
      Pldmfw_Ret_Rate_Limit_Reached                  => 40);

   function Uc_Pldmfw_Ret_U16 is new Ada.Unchecked_Conversion
     (Source => Pldmfw_Ret, Target => NvU16);

   function Uc_U16_Pldmfw_Ret is new Ada.Unchecked_Conversion
     (Source => NvU16, Target => Pldmfw_Ret);

   subtype Arr_U8_Idx_16 is ARR_NvU8_IDX32 (0 .. 15) with
       Object_Size => 128;

   MAX_PASS_COMPONENT_ID_NUM : constant := 3;
   subtype Arr_U16_Idx_3 is ARR_NvU16_IDX32 (0 .. (MAX_PASS_COMPONENT_ID_NUM - 1)) with
       Object_Size => 48;

   type Non_Idempotent_Resp is record
      Data : Arr_U8_Idx_16;
   end record with
     Size => 128, Object_Size => 128, Alignment => 1;

   type Iid_U8 is record
      Iid : NvU5;
      Rsv : NvU3;
   end record with
     Size => 8, Object_Size => 8, Pack;

   type Non_Idempotent_Iid_Cmd_Resp is record
      Iid  : Iid_U8;
      Cmd  : NvU8;
      Len  : NvU32;
      Resp : Non_Idempotent_Resp;
   end record with
     Size => 176, Object_Size => 176, Alignment => 1;

   type Comp_Size_Record is record
      Valid_Size : NvU8;
      Comp_Arr   : Arr_U16_Idx_3;
   end record with
     Size => 56, Object_Size => 56, Alignment => 1;

   type Iid_Offset_Pair is record
      Iid    : Iid_U8;
      Offset : NvU32;
   end record with
     Size => 40, Object_Size => 40, Pack;

   IID_PAIR_SIZE : constant := 8;
   type Arr_Iid_Pair_Size_Idx8 is
     array (NvU8 range 0 .. (IID_PAIR_SIZE - 1)) of Iid_Offset_Pair;

   type Iid_Offset_List is record
      Idx   : NvU8;
      Pairs : Arr_Iid_Pair_Size_Idx8;
   end record with
     Size => 328, Object_Size => 328, Alignment => 1;

   type Pldmfw_Setting_Table is record
      Is_Inband_Supported : NvU1;
      Rsv                 : NvU7;
   end record with
     Size => 8, Object_Size => 8, Pack;

   subtype Arr_Pldm_Fw_Data_Buffer is ARR_NvU8_IDX32 (0 .. 255) with
       Object_Size => 2_048;

   type Arr_Pldm_Fw_Data_Buffer_Record is record
      Data     : Arr_Pldm_Fw_Data_Buffer;
      Cur_Size : NvU32; --  current size of data
   end record with
     Size => 2_080, Object_Size => 2_080, Alignment => 1;

   subtype Arr_Metadata_Buffer is ARR_NvU8_IDX16 (0 .. 63) with
       Object_Size => 512;
   type Pldmfw_Metadata is record
      Data : Arr_Metadata_Buffer; --  metadata version info
   end record with
     Size => 512, Object_Size => 512, Alignment => 1;

   type Pldm_Context is record
      Pldm_State              : Pldmfw_State;                    --  pldm state machine
      Pldm_Previous_State     : Pldmfw_State;                    --  pldm previouse state
      Idle_Reason_Code        : NvU8;                            --  idle reason
      Max_Transfer_Size : NvU32;                           --  UA requests max transfer size
      Comp_Id                 : NvU16;                           --  component id
      Fw_Size                 : NvU32;                           --  Fw size
      Fw_Current_Offset       : NvU32;                           --  Current fw offset
      Fw_Update_Size          : NvU32;                           --  Update size
      Last_Transfer_Size : NvU32;                           --  Transfer size of RequestFwData requested by FD
      Wr_Spi_Addr             : NvU32;                           --  Write spi address
      Is_Force_Update         : Boolean;                         --  is force update or not
      Iid                     : Iid_U8;                          --  iid for req cmd
      Composer_Buffer         : Arr_Pldm_Tx_Msg_Buffer;          --  composer msg payload
      Slot_State              : Pldmfw_Slot_State;               --  Recording pldm slot state
      Iid_Cmd                 : Non_Idempotent_Iid_Cmd_Resp;     --  non idempotent iid response
      Pass_Comp_Size          : Comp_Size_Record;                --  Pass component size record
      Verify_Comp_Size : Comp_Size_Record;                --  Verify complete component size record
      Activate_Comp_Size : Comp_Size_Record;                --  Activate component size record
      Req_Fw_Data_List : Iid_Offset_List;                 --  iid offset pairs for request fw data
      Req_Retry               : NvU8;                            --  requester retry count
      Spi_Flash_Retry         : NvU8;                            --  spiflash retry count
      Total_Retry             : NvU32;                           --  total retry
      Is_Inforom_Ro_Activate  : Boolean;                         --  If inforomro activate
      Rx_Msg                  : Arr_Pldm_Rx_Msg_Buffer_Record;   --  Rx memory from UA
      Rx_Interface            : Pldmfw_Interface_Info;           --  Rx fwupdate interface info
      Update_Interface : Pldmfw_Interface_Info;           --  keep fwupdate interface info
      Is_Inband_Enable        : aliased Ib_Fw_Update_Flag_Type;  --  Inband update
      Is_Backup_In_Progress   : aliased Boolean;                 --  Is bgcopy in progress
      Pldm_Settings           : Pldmfw_Setting_Table;            --  pldm setting config
      Dl_Send_Time            : NvU32;                           --  request fw data time
      Dl_M0_Time              : NvU32;                           --  transfer 4k time
      Dl_M1_Time              : NvU32;                           --  erase/program 4k time
      Transfer_Fail_CC : NvU8;                            --  record fail CC during download state
      Verify_Fail_CC          : NvU8;                            --  record fail cc for verify
      Current_Rate            : NvU8;                            --  current rate
      Limited_Rate : NvU8;                            --  limited rate 16 => 8 => 4 => 2 => 2 => 2 ...
      Is_Limited_Rate_Reached : Boolean;                         --  is rate limit reached
      Rate_Limit_Timestamp    : NvU32;                           --  rate limit timestamp
      Is_Recv_All_Metadata : Boolean;                         -- record if receive all version info from metadata
      Is_Metadata_Check_Done : Boolean;                   -- record if finish check the version info from metadata
      Fw_Data_Buffer          : Arr_Pldm_Fw_Data_Buffer_Record; --  Fw data buffer
      Metadata                : Pldmfw_Metadata; --  Metadata version info
      Auth_Request_Id         : NvU32;                           --  authentication request id
      Write_Fail_Retry        : NvU8;                            --  is write fail retry
   end record;

   pragma Convention (C, Pldm_Context);

end Pdk.Pldm.Fwupdate;

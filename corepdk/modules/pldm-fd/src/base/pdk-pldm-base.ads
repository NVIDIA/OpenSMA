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
--  This package contains PLDM - T0 Base define.
--

with Ada.Unchecked_Conversion;
with Nv_Types;                      use Nv_Types;
with Nv_Types.Array_Types;          use Nv_Types.Array_Types;
with Pdk.Pldm.Fwupdate;             use Pdk.Pldm.Fwupdate;
with Pdk.Pldm.Interfaces.Plattypes; use Pdk.Pldm.Interfaces.Plattypes;

package Pdk.Pldm.Base is

   --  PLDM Base command defines.
   PLDM_BASE_CMD_LSB_VER : constant := 16#F1F0_F000#;

   BASE_COMMAND_SET_TID        : constant := 1;
   BASE_COMMAND_GET_TID        : constant := 2;
   BASE_COMMAND_GET_PLDM_VER   : constant := 3;
   BASE_COMMAND_GET_PLDM_TYPES : constant := 4;
   BASE_COMMAND_GET_PLDM_CMDS  : constant := 5;

   PLDM_BASE_CC_INVALID_DATA_TRANSFER_HANDLE         : constant := 16#80#;
   PLDM_BASE_CC_INVALID_TRANSFER_OPERATION_FLAG      : constant := 16#81#;
   PLDM_BASE_CC_INVALID_PLDM_TYPE_IN_REQUEST_DATA    : constant := 16#83#;
   PLDM_BASE_CC_INVALID_PLDM_VERSION_IN_REQUEST_DATA : constant := 16#84#;

   MAX_PLDM_TYPES     : constant := 8;
   MAX_PLDM_CMDS      : constant := 32;
   PLDM_VER_SIZE      : constant := 4;
   MAX_PLDM_RESP_SIZE : constant := 37;

   subtype Arr_Pldm_Types is ARR_NvU8_IDX32 (0 .. MAX_PLDM_TYPES - 1) with
       Object_Size => 64;

   subtype Arr_Pldm_Cmds is ARR_NvU8_IDX32 (0 .. MAX_PLDM_CMDS - 1) with
       Object_Size => 256;

   subtype Arr_Pldm_Ver is ARR_NvU8_IDX32 (0 .. PLDM_VER_SIZE - 1) with
       Object_Size => 32;

   function Uc_Pldm_Ver_To_Array is new Ada.Unchecked_Conversion
     (Source => NvU32, Target => Arr_Pldm_Ver);

   subtype Arr_Max_Pldm_Base_Resp is ARR_NvU8_IDX32 (0 .. MAX_PLDM_RESP_SIZE - 1) with
       Object_Size => 296;

   type Arr_Max_Pldm_Base_Resp_Record is record
      Data : Arr_Max_Pldm_Base_Resp;
   end record with
     Size => 296, Object_Size => 296, Alignment => 1;

   P_Base_Cmd_Arr : constant ARR_NvU8_IDX32 :=
     [BASE_COMMAND_SET_TID,
     BASE_COMMAND_GET_TID,
     BASE_COMMAND_GET_PLDM_VER,
     BASE_COMMAND_GET_PLDM_TYPES,
     BASE_COMMAND_GET_PLDM_CMDS];

   P_Fw_Cmd_Arr : constant ARR_NvU8_IDX32 :=
     [PLDMFW_COMMAND_QUERY_DEVICE_ID,
     PLDMFW_COMMAND_GET_FW_PARAMETERS,
     PLDMFW_COMMAND_REQUEST_UPDATE,
     PLDMFW_COMMAND_PASS_COMPONENT_TABLE,
     PLDMFW_COMMAND_UPDATE_COMPONENT,
     PLDMFW_COMMAND_REQUEST_FW_DATA,
     PLDMFW_COMMAND_TRANSFER_COMPLETE,
     PLDMFW_COMMAND_VERIFY_COMPLETE,
     PLDMFW_COMMAND_APPLY_COMPLETE,
     PLDMFW_COMMAND_ACTIVATE_FW,
     PLDMFW_COMMAND_GET_STATUS,
     PLDMFW_COMMAND_CANCEL_UPDATE_COMPONENT,
     PLDMFW_COMMAND_CANCEL_UPDATE];

   type Tx_Op_Flag is (GET_NEXT_PART, GET_FIRST_PART) with
     Size => 8, Object_Size => 8;

   for Tx_Op_Flag use
     (GET_NEXT_PART  => 0,
      GET_FIRST_PART => 1);

   type Tx_Flag is (INVALID, START, MIDDLE, TX_END, START_AND_END) with
     Size => 8, Object_Size => 8;

   for Tx_Flag use
     (INVALID       => 0,
      START         => 1,
      MIDDLE        => 2,
      TX_END        => 4,
      START_AND_END => 5);

   type Pldm_Capability is record
      P_Type : NvU8;
      Ver    : NvU32;  --  We don't support multi - version
      Cmds   : Arr_Pldm_Cmds;
   end record with
     Size => 296, Object_Size => 296, Alignment => 1;

   type Pldm_Cap_Arr is array (NvU8 range 0 .. 1) of Pldm_Capability;

   type Pldm_Base_Context is record
      Tid          : NvU8;
      Req          : Arr_Max_Pldm_Base_Resp_Record;
      Caps         : Pldm_Cap_Arr;
      Rx_Interface : Pldmfw_Interface_Info;
   end record;

   pragma Convention (C, Pldm_Base_Context);

end Pdk.Pldm.Base;

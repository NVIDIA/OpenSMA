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
--  This package contains procedure to perform PLDM - T0 Base operations.
--

with Ada.Unchecked_Conversion;
with Pdk.Pldm.Packet; use Pdk.Pldm.Packet;

package Pdk.Pldm.Base.Cmd with
  SPARK_Mode => On
is

   type Pldm_Error_Res is record
      Hdr : Pldm_Header;
      Cc  : NvU8;
   end record with
     Size => 32, Object_Size => 32, Alignment => 1;

   type Pldm_Set_Eid_Req is record
      Hdr : Pldm_Header;
      Tid : NvU8;
   end record with
     Size => 32, Object_Size => 32, Alignment => 1;

   type Pldm_Set_Eid_Resp is record
      Hdr : Pldm_Header;
      Cc  : NvU8;
   end record with
     Size => 32, Object_Size => 32, Alignment => 1;

   type Pldm_Get_Eid_Req is record
      Hdr : Pldm_Header;
   end record with
     Size => 24, Object_Size => 24, Alignment => 1;

   type Pldm_Get_Eid_Res is record
      Hdr : Pldm_Header;
      Cc  : NvU8;
      Tid : NvU8;
   end record with
     Size => 40, Object_Size => 40, Alignment => 1;

   type Pldm_Get_Pldm_Ver_Req is record
      Hdr    : Pldm_Header;
      Handle : NvU32;
      Flag   : Tx_Op_Flag;
      P_Type : NvU8;
   end record with
     Size => 72, Object_Size => 72, Alignment => 1;

   type Pldm_Get_Pldm_Ver_Res is record
      Hdr    : Pldm_Header;
      Cc     : NvU8;
      Handle : NvU32;
      Flag   : Tx_Flag;
      Ver    : NvU32;  --  We don't support multi - version.
      Crc    : NvU32;
   end record with
     Size => 136, Object_Size => 136, Alignment => 1;

   type Pldm_Get_Pldm_Types_Req is record
      Hdr : Pldm_Header;
   end record with
     Size => 24, Object_Size => 24, Alignment => 1;

   type Pldm_Get_Pldm_Types_Res is record
      Hdr     : Pldm_Header;
      Cc      : NvU8;
      P_Types : Arr_Pldm_Types; -- 8bytes
   end record with
     Size => 96, Object_Size => 96, Alignment => 1;

   type Pldm_Get_Pldm_Cmds_Req is record
      Hdr    : Pldm_Header;
      P_Type : NvU8;
      Ver    : NvU32;
   end record with
     Size => 64, Object_Size => 64, Alignment => 1;

   type Pldm_Get_Pldm_Cmds_Res is record
      Hdr  : Pldm_Header;
      Cc   : NvU8;
      Cmds : Arr_Pldm_Cmds;
   end record with
     Size => 288, Object_Size => 288, Alignment => 1;

   subtype Arr_Pldm_Error_Res is ARR_NvU8_IDX32 (0 .. (Pldm_Error_Res'Size / NvU8'Size) - 1);

   subtype Arr_Pldm_Set_Eid_Req is
     ARR_NvU8_IDX32 (0 .. (Pldm_Set_Eid_Req'Size / NvU8'Size) - 1);

   subtype Arr_Pldm_Set_Eid_Resp is
     ARR_NvU8_IDX32 (0 .. (Pldm_Set_Eid_Resp'Size / NvU8'Size) - 1);

   subtype Arr_Pldm_Get_Eid_Req is
     ARR_NvU8_IDX32 (0 .. (Pldm_Get_Eid_Req'Size / NvU8'Size) - 1);

   subtype Arr_Pldm_Get_Eid_Res is
     ARR_NvU8_IDX32 (0 .. (Pldm_Get_Eid_Res'Size / NvU8'Size) - 1);

   subtype Arr_Pldm_Get_Pldm_Ver_Req is
     ARR_NvU8_IDX32 (0 .. (Pldm_Get_Pldm_Ver_Req'Size / NvU8'Size) - 1);

   subtype Arr_Pldm_Get_Pldm_Ver_Res is
     ARR_NvU8_IDX32 (0 .. (Pldm_Get_Pldm_Ver_Res'Size / NvU8'Size) - 1);

   subtype Arr_Pldm_Get_Pldm_Types_Req is
     ARR_NvU8_IDX32 (0 .. (Pldm_Get_Pldm_Types_Req'Size / NvU8'Size) - 1);

   subtype Arr_Pldm_Get_Pldm_Types_Res is
     ARR_NvU8_IDX32 (0 .. (Pldm_Get_Pldm_Types_Res'Size / NvU8'Size) - 1);

   subtype Arr_Pldm_Get_Pldm_Cmds_Req is
     ARR_NvU8_IDX32 (0 .. (Pldm_Get_Pldm_Cmds_Req'Size / NvU8'Size) - 1);

   subtype Arr_Pldm_Get_Pldm_Cmds_Res is
     ARR_NvU8_IDX32 (0 .. (Pldm_Get_Pldm_Cmds_Res'Size / NvU8'Size) - 1);

   function Uc_Pldm_Set_Eid_Req_To_Array is new Ada.Unchecked_Conversion
     (Source => Pldm_Set_Eid_Req, Target => Arr_Pldm_Set_Eid_Req);

   function Uc_Pldm_Error_Res_To_Array is new Ada.Unchecked_Conversion
     (Source => Pldm_Error_Res, Target => Arr_Pldm_Error_Res);

   function Uc_Pldm_Set_Eid_Resp_To_Array is new Ada.Unchecked_Conversion
     (Source => Pldm_Set_Eid_Resp, Target => Arr_Pldm_Set_Eid_Resp);

   function Uc_Pldm_Get_Eid_Req_To_Array is new Ada.Unchecked_Conversion
     (Source => Pldm_Get_Eid_Req, Target => Arr_Pldm_Get_Eid_Req);

   function Uc_Pldm_Get_Eid_Res_To_Array is new Ada.Unchecked_Conversion
     (Source => Pldm_Get_Eid_Res, Target => Arr_Pldm_Get_Eid_Res);

   function Uc_Pldm_Get_Pldm_Ver_Req_To_Array is new Ada.Unchecked_Conversion
     (Source => Pldm_Get_Pldm_Ver_Req, Target => Arr_Pldm_Get_Pldm_Ver_Req);

   function Uc_Pldm_Get_Pldm_Ver_Res_To_Array is new Ada.Unchecked_Conversion
     (Source => Pldm_Get_Pldm_Ver_Res, Target => Arr_Pldm_Get_Pldm_Ver_Res);

   function Uc_Pldm_Get_Pldm_Types_Req_To_Array is new Ada.Unchecked_Conversion
     (Source => Pldm_Get_Pldm_Types_Req, Target => Arr_Pldm_Get_Pldm_Types_Req);

   function Uc_Pldm_Get_Pldm_Types_Res_To_Array is new Ada.Unchecked_Conversion
     (Source => Pldm_Get_Pldm_Types_Res, Target => Arr_Pldm_Get_Pldm_Types_Res);

   function Uc_Pldm_Get_Pldm_Cmds_Req_To_Array is new Ada.Unchecked_Conversion
     (Source => Pldm_Get_Pldm_Cmds_Req, Target => Arr_Pldm_Get_Pldm_Cmds_Req);

   function Uc_Pldm_Get_Pldm_Cmds_Res_To_Array is new Ada.Unchecked_Conversion
     (Source => Pldm_Get_Pldm_Cmds_Res, Target => Arr_Pldm_Get_Pldm_Cmds_Res);

   function Uc_Array_To_Pldm_Set_Eid_Req is new Ada.Unchecked_Conversion
     (Source => Arr_Pldm_Set_Eid_Req, Target => Pldm_Set_Eid_Req);

   function Uc_Array_To_Pldm_Error_Res is new Ada.Unchecked_Conversion
     (Source => Arr_Pldm_Error_Res, Target => Pldm_Error_Res);
   pragma Annotate
     (GNATprove, False_Positive, "type is unsuitable as a target for unchecked conversion",
      "Fix me");

   function Uc_Array_To_Pldm_Set_Eid_Resp is new Ada.Unchecked_Conversion
     (Source => Arr_Pldm_Set_Eid_Resp, Target => Pldm_Set_Eid_Resp);
   pragma Annotate
     (GNATprove, False_Positive, "type is unsuitable as a target for unchecked conversion",
      "Fix me");

   function Uc_Array_To_Pldm_Get_Eid_Req is new Ada.Unchecked_Conversion
     (Source => Arr_Pldm_Get_Eid_Req, Target => Pldm_Get_Eid_Req);

   function Uc_Array_To_Pldm_Get_Eid_Res is new Ada.Unchecked_Conversion
     (Source => Arr_Pldm_Get_Eid_Res, Target => Pldm_Get_Eid_Res);
   pragma Annotate
     (GNATprove, False_Positive, "type is unsuitable as a target for unchecked conversion",
      "Fix me");

   function Uc_Array_To_Pldm_Get_Pldm_Ver_Req is new Ada.Unchecked_Conversion
     (Source => Arr_Pldm_Get_Pldm_Ver_Req, Target => Pldm_Get_Pldm_Ver_Req);
   pragma Annotate
     (GNATprove, False_Positive, "type is unsuitable as a target for unchecked conversion",
      "Fix me");

   function Uc_Array_To_Pldm_Get_Pldm_Ver_Res is new Ada.Unchecked_Conversion
     (Source => Arr_Pldm_Get_Pldm_Ver_Res, Target => Pldm_Get_Pldm_Ver_Res);
   pragma Annotate
     (GNATprove, False_Positive, "type is unsuitable as a target for unchecked conversion",
      "Fix me");

   function Uc_Array_To_Pldm_Get_Pldm_Types_Req is new Ada.Unchecked_Conversion
     (Source => Arr_Pldm_Get_Pldm_Types_Req, Target => Pldm_Get_Pldm_Types_Req);

   function Uc_Array_To_Pldm_Get_Pldm_Types_Res is new Ada.Unchecked_Conversion
     (Source => Arr_Pldm_Get_Pldm_Types_Res, Target => Pldm_Get_Pldm_Types_Res);
   pragma Annotate
     (GNATprove, False_Positive, "type is unsuitable as a target for unchecked conversion",
      "Fix me");

   function Uc_Array_To_Pldm_Get_Pldm_Cmds_Req is new Ada.Unchecked_Conversion
     (Source => Arr_Pldm_Get_Pldm_Cmds_Req, Target => Pldm_Get_Pldm_Cmds_Req);

   function Uc_Array_To_Pldm_Get_Pldm_Cmds_Res is new Ada.Unchecked_Conversion
     (Source => Arr_Pldm_Get_Pldm_Cmds_Res, Target => Pldm_Get_Pldm_Cmds_Res);
   pragma Annotate
     (GNATprove, False_Positive, "type is unsuitable as a target for unchecked conversion",
      "Fix me");

   procedure Process_Base_Commands
     (Pldm_Ctx : aliased in out Pldm_Base_Context;
      Size     :                NvU32) with
     Export => True, Convention => CPP, External_Name => "ada_process_base_commands";

   procedure Fill_Resp_Header
     (Req  :     Pldm_Header;
      Resp : out Pldm_Header);

   procedure Respond_Error
     (Pldm_Ctx : aliased Pldm_Base_Context;
      Req      :         Pldm_Header;
      Cc       :         NvU8);

   procedure Crc32
     (Buffer  :     ARR_NvU8_IDX32;
      Ret_Val : out NvU32);

   procedure On_Set_Tid
     (Pldm_Ctx : aliased in out Pldm_Base_Context;
      Buffer   :                Arr_Pldm_Set_Eid_Req);

   procedure On_Get_Tid
     (Pldm_Ctx : aliased Pldm_Base_Context;
      Buffer   :         Arr_Pldm_Get_Eid_Req);

   procedure Get_Pldm_Version
     (Pldm_Ctx : aliased Pldm_Base_Context;
      Buffer   :         Arr_Pldm_Get_Pldm_Ver_Req);

   procedure Get_Pldm_Types
     (Pldm_Ctx : aliased Pldm_Base_Context;
      Buffer   :         Arr_Pldm_Get_Pldm_Types_Req);

   procedure Get_Pldm_Cmds
     (Pldm_Ctx : aliased Pldm_Base_Context;
      Buffer   :         Arr_Pldm_Get_Pldm_Cmds_Req);

   procedure Init_Pldm_Base_Ctx (Pldm_Ctx : aliased out Pldm_Base_Context) with
     Export => True, Convention => CPP, External_Name => "ada_init_pldm_base_ctx";

end Pdk.Pldm.Base.Cmd;

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

with Ada.Unchecked_Conversion;
with Pdk.Pldm.Packet;         use Pdk.Pldm.Packet;
with Nv_Types;                use Nv_Types;
with Nv_Types.Array_Types;    use Nv_Types.Array_Types;
with Pdk.Pldm.Plattypes;      use Pdk.Pldm.Plattypes;
with Pdk.Pldm.Hook.Plattypes; use Pdk.Pldm.Hook.Plattypes;

package Pdk.Pldm.Plat with
  SPARK_Mode => On
is

   Max_Size : constant NvU8 := 5; -- Define maximum queue size
   type Queue_Array is array (1 .. Max_Size) of Queue_Element;
   type Queue is record
      Data  : Queue_Array;
      Front : NvU8 := 1;
      Rear  : NvU8 := 0;
      Count : NvU8 := 0;
   end record;

   function Is_Empty
     (Q : Queue)
      return Boolean;

   function Is_Full
     (Q : Queue)
      return Boolean;

   procedure Enqueue
     (Q    : in out Queue;
      Item :        ARR_NvU8_IDX32);

   procedure Dequeue
     (Q    : in out Queue;
      Item :    out Arr_Pldm_Tx_Msg_Buffer;
      Len  :    out NvU32);

   type Pldm_Posix_Context is record
      Rx_Queue : Queue;
   end record;

   Posix_Ctx : Pldm_Posix_Context;

   --  log
   --  below definition needs to as same as nv/logger/common.h
   LOG_PLDM_ERROR             : constant := 16#0800#;
   LOG_PLDM_CHANGE_STATE      : constant := 16#0801#;
   LOG_PLDM_TRANSFER_COMPLETE : constant := 16#0802#;
   LOG_PLDM_AUTH              : constant := 16#0803#;
   LOG_PLDM_ACTIVATE          : constant := 16#0804#;
   LOG_PLDM_TIMEOUT           : constant := 16#0805#;
   LOG_PLDM_TOTAL_RETRY       : constant := 16#0806#;
   LOG_PLDM_CANCEL            : constant := 16#0807#;
   LOG_PLDM_M0_TIME           : constant := 16#0808#;
   LOG_PLDM_M1_TIME           : constant := 16#0809#;
   LOG_PLDM_UPDATE_OFFSET     : constant := 16#080a#;
   LOG_PLDM_IDLE_REASON       : constant := 16#080b#;
   LOG_PLDM_STAGE_UPDATE      : constant := 16#080c#;

   Pldm_Vendor_Defined_Descriptor_Title_String_Apsku : constant ARR_NvU8_IDX32 (0 .. 4) :=
     [16#41#,
     16#50#,
     16#53#,
     16#4b#,
     16#55#];

   Pldm_Enterprise_Id : constant ARR_NvU8_IDX32 (0 .. 3) :=
     [16#47#,
     16#16#,
     16#00#,
     16#00#];

   type Update_State is (Idle, InProgress, Complete, Invalid, Stage) with
     Size => 8, Object_Size => 8, Alignment => 1;

   for Update_State use
     (Idle       => 0,
      InProgress => 1,
      Complete   => 2,
      Invalid    => 3,
      Stage      => 4);

   function Uc_Update_State_To_U8 is new Ada.Unchecked_Conversion
     (Source => Update_State, Target => NvU8);

   --  //////////
   --  // Tool //
   --  //////////

   subtype Array_U8_String is ARR_NvU8_IDX32 (0 .. 1);
   subtype Array_U16_String is ARR_NvU8_IDX32 (0 .. 3);

   procedure To_Decimal_String
     (Value          :     NvU8;
      Decimal_String : out Array_U8_String);

   procedure To_Decimal_String
     (Value          :     NvU16;
      Decimal_String : out Array_U16_String);

   procedure Populate_Stamp
     (Minor :     NvU8;
      Patch :     NvU16;
      Build :     NvU16;
      Stamp : out NvU32) with
     Export => True, Convention => CPP, External_Name => "ada_populate_stamp";

   procedure Populate_Version_String
     (Major          :     NvU16;
      Minor          :     NvU8;
      Patch          :     NvU16;
      Build          :     NvU16;
      Version_String : out Array_Version_String;
      Length         : out NvU8);

   procedure Get_Active_Stamp (Stamp : out NvU32);

   procedure Get_Active_Version_String
     (Version_String : out Array_Version_String;
      Length         : out NvU8);

   procedure Get_Pending_Stamp (Stamp : out NvU32);

   procedure Get_Pending_Version_String
     (Version_String : out Array_Version_String;
      Length         : out NvU8);

end Pdk.Pldm.Plat;

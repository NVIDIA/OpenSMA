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

with Nv_Types; use Nv_Types;
with Nv_Types.Array_Types; use Nv_Types.Array_Types;
with Ada.Unchecked_Conversion;
with Pdk.Pldm.Hook.Plattypes; use Pdk.Pldm.Hook.Plattypes;
with System;

package Pdk.Pldm.Plat
with SPARK_Mode => On
is

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

   Pldm_Vendor_Defined_Descriptor_Title_String_Apsku :
      constant ARR_NvU8_IDX32 (0 .. 4) :=
         [16#41#, 16#50#, 16#53#, 16#4b#, 16#55#];

   Pldm_Vendor_Defined_Descriptor_Title_String_Axsku :
      constant ARR_NvU8_IDX32 (0 .. 4) :=
         [16#41#, 16#58#, 16#53#, 16#4b#, 16#55#];

   Pldm_Enterprise_Id : constant ARR_NvU8_IDX32 (0 .. 3) :=
         [16#47#, 16#16#, 16#00#, 16#00#];

   type Update_State is (Idle,
                         InProgress,
                         Complete,
                         Invalid,
                         Stage)
     with Size => 8, Object_Size => 8, Alignment => 1;

   for Update_State use (Idle        => 0,
                         InProgress  => 1,
                         Complete    => 2,
                         Invalid     => 3,
                         Stage       => 4);

   function Uc_Update_State_To_U8 is new
         Ada.Unchecked_Conversion (Source => Update_State,
                                   Target => NvU8);

   --  //////////
   --  // Tool //
   --  //////////
   subtype Array_U8_String is ARR_NvU8_IDX32 (0 .. 1);
   subtype Array_U16_String is ARR_NvU8_IDX32 (0 .. 3);

   procedure To_Decimal_String (Value          : NvU8;
                                Decimal_String : out Array_U8_String);

   procedure To_Decimal_String (Value          : NvU16;
                                Decimal_String : out Array_U16_String);

   procedure Populate_Stamp (Minor    : NvU8;
                             Patch    : NvU16;
                             Build    : NvU16;
                             Stamp    : out NvU32)
     with
       export       => True, 
       convention   => CPP, 
       external_name => "ada_populate_stamp";

   procedure Populate_Ap_Stamp (Major    : NvU16;
                                Minor    : NvU8;
                                Patch    : NvU16;
                                Build    : NvU16;
                                Stamp    : out NvU32)
     with
       export       => True, 
       convention   => CPP, 
       external_name => "ada_populate_ap_stamp";

   procedure Populate_Version_String (Major          : NvU16;
                                      Minor          : NvU8;
                                      Patch          : NvU16;
                                      Build          : NvU16;
                                      Version_String : out Array_Version_String;
                                      Length         : out NvU8);

   procedure Populate_Ap_Version_String (Major          : NvU16;
                                         Minor          : NvU8;
                                         Patch          : NvU16;
                                         Build          : NvU16;
                                         Version_String : out Array_Version_String;
                                         Length         : out NvU8);

   procedure Get_Active_Stamp (Stamp : out NvU32);

   procedure Get_Active_Version_String (Version_String : out Array_Version_String;
                                                  Length         : out NvU8);

   procedure Get_Pending_Stamp (Stamp : out NvU32);

   procedure Get_Pending_Version_String (Version_String : out Array_Version_String;
                                                   Length         : out NvU8);

   procedure Get_Ap_Active_Stamp (Stamp : in out NvU32);
   procedure Get_Ap_Active_Version_String (Version_String : out Array_Version_String;
                                                  Length         : out NvU8);

   procedure Get_Ap_Pending_Stamp (Stamp : in out NvU32);
   procedure Get_Ap_Pending_Version_String (Version_String : out Array_Version_String;
                                                   Length         : out NvU8);

   --  @todo should remove this
   function Pldm_Get_Device_Identity (ArrAddress   : in System.Address) return NvU32
     with
       export       => True, 
       convention   => CPP, 
       external_name => "ada_get_pldm_device_identity";

end Pdk.Pldm.Plat;



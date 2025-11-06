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
--  This package contains PLDM device identities record and related define
--

with Nv_Types;             use Nv_Types;
with Nv_Types.Array_Types; use Nv_Types.Array_Types;
with Pdk.Pldm.Packet;      use Pdk.Pldm.Packet;

package Pdk.Pldm.Device with
  SPARK_Mode => On
is

   MAX_DESCRIPTOR_LENGTH : constant := 8;
   MAX_COMPONENT_LENGTH  : constant := 8;

   subtype Descriptor_Count is NvU8 range 0 .. (MAX_DESCRIPTOR_LENGTH - 1);
   subtype Component_Count is NvU8 range 0 .. (MAX_COMPONENT_LENGTH - 1);

   subtype Array_Descriptor_Data is ARR_NvU8_IDX32 (0 .. 15);
   subtype Array_Vendor_Descriptor_String is ARR_NvU8_IDX32 (0 .. 31);

   type Device_Descriptor is record
      Descriptor_Type : NvU16;
      Length          : NvU16;
      Data            : Array_Descriptor_Data;
   end record with
     Size => 160, Object_Size => 160, Alignment => 1;
   DEFAULT_DEVICE_DESCRIPTOR : constant Device_Descriptor :=
     (0,
      0,
      [others => 0]);

   type Device_Descriptor_List is
     array (NvU8 range 0 .. (MAX_DESCRIPTOR_LENGTH - 1)) of Device_Descriptor with
     Pack;

   type Vendor_Descriptor is record
      String_Type   : NvU8;
      String_Length : NvU8;
      String        : Array_Vendor_Descriptor_String;
      Data_Length   : NvU16;
      Data          : Array_Descriptor_Data;
   end record with
     Size => 416, Object_Size => 416, Alignment => 1;
   DEFAULT_VENDOR_DESCRIPTOR : constant Vendor_Descriptor :=
     (0,
      0,
      [others => 0],
      0,
      [others => 0]);

   type Vendor_Descriptor_List is
     array (NvU8 range 0 .. (MAX_DESCRIPTOR_LENGTH - 1)) of Vendor_Descriptor with
     Pack;

   type Pldmfw_Resp_Query_Device_Id is record
      Pldm_Hdr         : Pldm_Header;
      CC               : NvU8;
      Device_Id_Length : NvU32;
      Descriptor_Count : NvU8;
   end record with
     Size => 72, Object_Size => 72, Alignment => 1;

   DEFAULT_PLDMFW_RESP_QUERY_DEVICE_ID : constant Pldmfw_Resp_Query_Device_Id :=
     (DEFAULT_PLDM_HEADER,
      PLDM_CC_SUCCESS,
      0,
      0);

   type Pldmfw_Active_Method is record
      Automatic      : NvU1;
      Self_Contain   : NvU1;
      Medium_Reset   : NvU1;
      System_Reboot  : NvU1;
      Dc_Power_Cycle : NvU1;
      Ac_Power_Cycle : NvU1;
      Rsv            : NvU10;
   end record with
     Size => 16, Object_Size => 16, Alignment => 1;

   for Pldmfw_Active_Method use record
      Automatic      at 0 range 0 ..  0;
      Self_Contain   at 0 range 1 ..  1;
      Medium_Reset   at 0 range 2 ..  2;
      System_Reboot  at 0 range 3 ..  3;
      Dc_Power_Cycle at 0 range 4 ..  4;
      Ac_Power_Cycle at 0 range 5 ..  5;
      Rsv            at 0 range 6 .. 15;
   end record;

   DEFAULT_PLDMFW_ACTIVE_METHOD : constant Pldmfw_Active_Method :=
     (0,
      0,
      0,
      0,
      0,
      1,
      0);

end Pdk.Pldm.Device;

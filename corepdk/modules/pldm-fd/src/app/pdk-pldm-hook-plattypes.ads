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
with Nv_Types;             use Nv_Types;
with Nv_Types.Array_Types; use Nv_Types.Array_Types;
with Pdk.Pldm.Device;      use Pdk.Pldm.Device;

package Pdk.Pldm.Hook.Plattypes is

   type Log_Event is
     (Change_State, Transfer_Complete, Auth, Activate, Timeout, Cancel, Stage_Update) with
     Size => 8, Object_Size => 8, Alignment => 1;

   subtype Arr_64_Buffer is ARR_NvU8_IDX32 (0 .. 63);
   subtype Arr_4k_Buffer is ARR_NvU8_IDX32 (0 .. 4_095);
   subtype Arr_256B_Buffer is ARR_NvU8_IDX32 (0 .. 255);

   subtype Array_Version_String is ARR_NvU8_IDX32 (0 .. 31);

   type Component_Info is record
      Id                            : NvU16;
      Fw_Size                       : NvU32;
      Fw_Offset                     : NvU32;
      Ap_Sku_Id                     : NvU32;
      Stamp                         : NvU32;
      Pending_Stamp                 : NvU32;
      Version_String                : Array_Version_String;
      Version_String_Length         : NvU8;
      Pending_Version_String        : Array_Version_String;
      Pending_Version_String_Length : NvU8;
      Active_Method                 : Pldmfw_Active_Method;
   end record with
     Size => 720, Object_Size => 720, Alignment => 1;
   DEFAULT_COMPONENT_INFO : constant Component_Info :=
     (0,
      0,
      0,
      0,
      0,
      0,
      [others => 0],
      0,
      [others => 0],
      0,
      DEFAULT_PLDMFW_ACTIVE_METHOD);

   type Component_Info_List is
     array (NvU8 range 0 .. (MAX_COMPONENT_LENGTH - 1)) of Component_Info with
     Pack;

   type Limited_Rate_Info is record
      Min  : NvU8;
      Max  : NvU8;
      Time : NvU32;
   end record with
     Size => 48, Object_Size => 48, Alignment => 1;
   DEFAULT_LIMITED_RATE_INFO : constant Limited_Rate_Info :=
     (2,
      16,
      3_600_000); --  1 hour

end Pdk.Pldm.Hook.Plattypes;

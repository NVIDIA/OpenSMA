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
with Nv_Types.Get_Size;       use Nv_Types.Get_Size;
with Nv_Types.Shift_Right_Op; use Nv_Types.Shift_Right_Op;
with Pdk.Pldm.Hook.Plat;      use Pdk.Pldm.Hook.Plat;

package body Pdk.Pldm.Device.Cmd with
  SPARK_Mode => Off
is

   procedure Get_Device_Id_Length
     (Device_Descriptors       :     Device_Descriptor_List;
      Vendor_Descriptors       :     Vendor_Descriptor_List;
      Device_Descriptors_Count :     Descriptor_Count;
      Vendor_Descriptors_Count :     Descriptor_Count;
      Device_Id_Length         : out NvU32)
   is
      --  descriptor type <2> + descriptor length <2>
      DEVICE_DESCRIPTOR_TYPE_LENS : constant := 4;
      --  string type <1> + string lens <1>
      VENDOR_STRING_TYPE_LENS     : constant := 2;
   begin

      Device_Id_Length := 0;

      for i in 0 .. (Device_Descriptors_Count - 1)
      loop
         Device_Id_Length :=
           Device_Id_Length + DEVICE_DESCRIPTOR_TYPE_LENS +
           NvU32 (Device_Descriptors (i).Length);
      end loop;

      for i in 0 .. (Vendor_Descriptors_Count - 1)
      loop
         Device_Id_Length :=
           Device_Id_Length + DEVICE_DESCRIPTOR_TYPE_LENS + VENDOR_STRING_TYPE_LENS +
           NvU32 (Vendor_Descriptors (i).String_Length) +
           NvU32 (Vendor_Descriptors (i).Data_Length);
      end loop;

   end Get_Device_Id_Length;

   procedure Pldm_Fill_Device_Identity_Resp
     (Pldm_Hdr :     Pldm_Header;
      Tx_Msg   : out Arr_Pldm_Tx_Msg_Buffer_Record;
      Offset   : out NvU32)
   is
      Resp : Pldmfw_Resp_Query_Device_Id := DEFAULT_PLDMFW_RESP_QUERY_DEVICE_ID;
      Device_Descriptors       : Device_Descriptor_List;
      Vendor_Descriptors       : Vendor_Descriptor_List;
      Device_Descriptors_Count : NvU8;
      Vendor_Descriptors_Count : NvU8;
      Vendor_Descriptor_Size   : NvU16;
      Device_Id_Length         : NvU32;

      VENDOR_STRING_TYPE_LENS : constant := 2;

      subtype Arr_Resp_Buffer is
        ARR_NvU8_IDX32 (0 .. (Pldmfw_Resp_Query_Device_Id'Size / NvU8'Size - 1)) with
          Object_Size => 72;
      function Uc_Resp_To_Array is new Ada.Unchecked_Conversion
        (Source => Pldmfw_Resp_Query_Device_Id, Target => Arr_Resp_Buffer);
   begin

      Tx_Msg.Data := [others => 0];
      Offset      := 0;

      Resp.Pldm_Hdr := Pldm_Hdr;

      Resp.CC := PLDM_CC_SUCCESS;

      Pldm_Hook_Populate_Descriptors
        (Device_Descriptors => Device_Descriptors, Vendor_Descriptors => Vendor_Descriptors,
         Device_Descriptors_Count => Device_Descriptors_Count,
         Vendor_Descriptors_Count => Vendor_Descriptors_Count);

      Get_Device_Id_Length
        (Device_Descriptors => Device_Descriptors, Vendor_Descriptors => Vendor_Descriptors,
         Device_Descriptors_Count => Device_Descriptors_Count,
         Vendor_Descriptors_Count => Vendor_Descriptors_Count,
         Device_Id_Length         => Device_Id_Length);

      Resp.Device_Id_Length := Device_Id_Length;
      Resp.Descriptor_Count := Device_Descriptors_Count + Vendor_Descriptors_Count;

      --  Resp
      Tx_Msg.Data (Offset .. (Offset + Get_Size_Bytes (Resp'Size) - 1)) :=
        Uc_Resp_To_Array (Resp);
      Offset := Offset + Get_Size_Bytes (Resp'Size);

      --  Device Descriptor
      for i in 0 .. (Device_Descriptors_Count - 1)
      loop

         --  type
         Tx_Msg.Data (Offset) := NvU8 (Device_Descriptors (i).Descriptor_Type and 16#ff#);
         Offset               := Offset + 1;
         Tx_Msg.Data (Offset) :=
           NvU8 (Safe_Shift_Right (Device_Descriptors (i).Descriptor_Type, 8) and 16#ff#);
         Offset               := Offset + 1;
         --  lens
         Tx_Msg.Data (Offset) := NvU8 (Device_Descriptors (i).Length and 16#ff#);
         Offset               := Offset + 2;
         --  data
         if Device_Descriptors (i).Length /= 0
         then
            Tx_Msg.Data (Offset .. (Offset + NvU32 (Device_Descriptors (i).Length - 1))) :=
              Device_Descriptors (i).Data (0 .. NvU32 (Device_Descriptors (i).Length - 1));
            Offset := Offset + NvU32 (Device_Descriptors (i).Length);
         end if;
      end loop;

      for i in 0 .. (Vendor_Descriptors_Count - 1)
      loop

         --  vendor defined type
         Tx_Msg.Data (Offset)   := 16#ff#;
         Offset                 := Offset + 1;
         Tx_Msg.Data (Offset)   := 16#ff#;
         Offset                 := Offset + 1;
         --  vendor defined length
         Vendor_Descriptor_Size :=
           VENDOR_STRING_TYPE_LENS + NvU16 (Vendor_Descriptors (i).String_Length) +
           Vendor_Descriptors (i).Data_Length;
         Tx_Msg.Data (Offset)   := NvU8 (Vendor_Descriptor_Size and 16#ff#);
         Offset                 := Offset + 1;
         Tx_Msg.Data (Offset) := NvU8 (Safe_Shift_Right (Vendor_Descriptor_Size, 8) and 16#ff#);
         Offset                 := Offset + 1;
         --  string
         Tx_Msg.Data (Offset)   := Vendor_Descriptors (i).String_Type;
         Offset                 := Offset + 1;
         Tx_Msg.Data (Offset)   := Vendor_Descriptors (i).String_Length;
         Offset                 := Offset + 1;
         if Vendor_Descriptors (i).String_Length /= 0
         then
            Tx_Msg.Data
              (Offset .. (Offset + NvU32 (Vendor_Descriptors (i).String_Length - 1))) :=
              Vendor_Descriptors (i).String
                (0 .. NvU32 (Vendor_Descriptors (i).String_Length - 1));
            Offset := Offset + NvU32 (Vendor_Descriptors (i).String_Length);
         end if;
         --  data
         if Vendor_Descriptors (i).Data_Length /= 0
         then
            Tx_Msg.Data (Offset .. (Offset + NvU32 (Vendor_Descriptors (i).Data_Length - 1))) :=
              Vendor_Descriptors (i).Data (0 .. NvU32 (Vendor_Descriptors (i).Data_Length - 1));
            Offset := Offset + NvU32 (Vendor_Descriptors (i).Data_Length);
         end if;

      end loop;

   end Pldm_Fill_Device_Identity_Resp;

end Pdk.Pldm.Device.Cmd;

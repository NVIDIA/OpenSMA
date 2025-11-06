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
with Nv_Types;             use Nv_Types;
with Nv_Types.Array_Types; use Nv_Types.Array_Types;

package Pdk.Pldm.Packet with
  SPARK_Mode => On
is

   NV_PLDM_RESERVE_HEADER_SIZE : constant := 32;
   NV_PLDM_MAX_TRANSFER_SIZE   : constant := 16#1000#;
   NV_PLDM_MAX_TX_SIZE         : constant := 16#100#;

   type Pldm_Header is record
      Iid          : NvU5;
      Rsv          : NvU1;
      D            : NvU1;
      Rq           : NvU1;
      Command_Type : NvU8;
      Command_Code : NvU8;
   end record with
     Size => 24, Object_Size => 24, Alignment => 1;

   for Pldm_Header use record
      Iid          at 0 range  0 ..  4;
      Rsv          at 0 range  5 ..  5;
      D            at 0 range  6 ..  6;
      Rq           at 0 range  7 ..  7;
      Command_Type at 0 range  8 .. 15;
      Command_Code at 0 range 16 .. 23;
   end record;

   DEFAULT_PLDM_HEADER : constant Pldm_Header :=
     (0,
      0,
      0,
      0,
      0,
      0);

   --  Rx msg: 16 + 4k payload
   subtype Arr_Pldm_Rx_Msg_Buffer is
     ARR_NvU8_IDX32
       (0 .. NvU32 (NV_PLDM_RESERVE_HEADER_SIZE + NV_PLDM_MAX_TRANSFER_SIZE - 1)) with
       Object_Size => 33_024;

   type Arr_Pldm_Rx_Msg_Buffer_Record is record
      Data : Arr_Pldm_Rx_Msg_Buffer;
   end record with
     Size => 33_024, Object_Size => 33_024, Alignment => 1;

   subtype Arr_Pldm_Tx_Msg_Buffer is ARR_NvU8_IDX32 (0 .. NvU32 (NV_PLDM_MAX_TX_SIZE - 1)) with
       Object_Size => 2_048;

   type Arr_Pldm_Tx_Msg_Buffer_Record is record
      Data : Arr_Pldm_Tx_Msg_Buffer;
   end record with
     Size => 2_048, Object_Size => 2_048, Alignment => 1;

   subtype Arr_Pldm_Header is
     ARR_NvU8_IDX32 (0 .. NvU32 (Pldm_Header'Size / NvU8'Size - 1)) with
       Object_Size => 24;

   function Uc_Pldm_Header_To_Array is new Ada.Unchecked_Conversion
     (Source => Pldm_Header, Target => Arr_Pldm_Header);

   function Uc_Array_To_Pldm_Header is new Ada.Unchecked_Conversion
     (Source => Arr_Pldm_Header, Target => Pldm_Header);

   type Mctp_Transport_Header is record
      Hdr_Version      : NvU4;
      Reserved         : NvU4;
      Destination_Eid  : NvU8;
      Source_Eid       : NvU8;
      Message_Tag      : NvU3;
      Tag_Owner        : NvU1;
      Packet_Sequence  : NvU2;
      End_Of_Message   : NvU1;
      Start_Of_Message : NvU1;
   end record with
     Size => 32, Object_Size => 32, Alignment => 1;

   for Mctp_Transport_Header use record
      Hdr_Version      at 0 range  0 ..  3;
      Reserved         at 0 range  4 ..  7;
      Destination_Eid  at 0 range  8 .. 15;
      Source_Eid       at 0 range 16 .. 23;
      Message_Tag      at 0 range 24 .. 26;
      Tag_Owner        at 0 range 27 .. 27;
      Packet_Sequence  at 0 range 28 .. 29;
      End_Of_Message   at 0 range 30 .. 30;
      Start_Of_Message at 0 range 31 .. 31;
   end record;

   INVALID_MCTP_TRANSPORT_HEADER : constant Mctp_Transport_Header :=
     (0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0);

   type Mctp_Pldm_Header is record
      Transport_Header : Mctp_Transport_Header;
      Msg_Type         : NvU7;
      Ic               : NvU1;
   end record with
     Size => 40, Object_Size => 40, Alignment => 1;

   for Mctp_Pldm_Header use record
      Transport_Header at 0 range  0 .. 31;
      Msg_Type         at 0 range 32 .. 38;
      Ic               at 0 range 39 .. 39;
   end record;

   DEFAULT_MCTP_PLDM_HEADER : constant Mctp_Pldm_Header :=
     (INVALID_MCTP_TRANSPORT_HEADER,
      0,
      0);

   subtype Arr_Mctp_Pldm_Header is
     ARR_NvU8_IDX32 (0 .. (Mctp_Pldm_Header'Size / NvU8'Size) - 1);

   function Uc_Mctp_Pldm_Header_To_Arr is new Ada.Unchecked_Conversion
     (Source => Mctp_Pldm_Header, Target => Arr_Mctp_Pldm_Header);

end Pdk.Pldm.Packet;

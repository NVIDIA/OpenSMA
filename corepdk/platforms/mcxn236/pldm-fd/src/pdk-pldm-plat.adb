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

with Pldm_Wrap; use Pldm_Wrap;
with Pdk.Pldm.Packet;     use Pdk.Pldm.Packet;
with Pdk.Pldm.Device.Cmd; use Pdk.Pldm.Device.Cmd;

package body Pdk.Pldm.Plat
with SPARK_Mode => On
is

   --  //////////
   --  // Tool //
   --  //////////

   --  Convert Uint8 value to a decimal ASCII string
   procedure To_Decimal_String (Value          : NvU8;
                                Decimal_String : out Array_U8_String)
   is
      -- Maximum length for an 8-bit integer is 2 characters (0 to 99)
      Index  : NvU32 := Decimal_String'Last;
      Temp   : NvU8 := Value;
   begin

      Decimal_String := [others => 16#30#];

      --  transfer to 0 .. 99
      Temp := Temp mod 100; 

      -- Handle zero as a special case
      if Temp = 0 then
         goto Exit_Point;
      end if;

      -- Extract digits from least significant to most significant
      while Temp > 0 loop
         declare
            Digit : constant NvU8 := Temp mod 10;
         begin
            Decimal_String(Index) := Character'Pos('0') + Digit;
            Temp := Temp / 10;
            Index := Index - 1;
         end;
      end loop;

      <<Exit_Point>>

   end To_Decimal_String;


   --  Convert Uint16 value to a decimal ASCII string
   procedure To_Decimal_String (Value          : NvU16;
                                Decimal_String : out Array_U16_String)
   is
      -- Maximum length for an 8-bit integer is 2 characters (0 to 9999)
      Index  : NvU32 := Decimal_String'Last;
      Temp   : NvU16 := Value;
   begin

      Decimal_String := [others => 16#30#];

      --  transfer to 0 .. 9999
      Temp := Temp mod 10000; 

      -- Handle zero as a special case
      if Temp = 0 then
         goto Exit_Point;
      end if;

      -- Extract digits from least significant to most significant
      while Temp > 0 loop
         declare
            Digit : constant NvU8 := NvU8 ((Temp mod 10) and 16#ff#);
         begin
            Decimal_String(Index) := Character'Pos('0') + Digit;
            Temp := Temp / 10;
            Index := Index - 1;
         end;
      end loop;

      <<Exit_Point>>

   end To_Decimal_String;

   procedure Populate_Stamp (Minor    : NvU8;
                             Patch    : NvU16;
                             Build    : NvU16;
                             Stamp    : out NvU32)
   is
      type Stamp_Rec is record
         Build : NvU8;
         Patch : NvU16;
         Minor : NvU8;
      end record;

      for Stamp_Rec use record
         Build at 0 range 0 .. 7;
         Patch at 0 range 8 .. 23;
         Minor at 0 range 24 .. 31;
      end record;

      Stamp_Record : Stamp_Rec;

      function Uc_Stamp_Rec_To_U32 is new
         Ada.Unchecked_Conversion (Source => Stamp_Rec,
                                   Target => NvU32);
   begin

      Stamp_Record.Minor := Minor;
      Stamp_Record.Patch := Patch;
      Stamp_Record.Build := NvU8 (Build and 16#ff#);

      Stamp := Uc_Stamp_Rec_To_U32 (Stamp_Record);

   end Populate_Stamp;

   procedure Populate_Version_String (Major          : NvU16;
                                      Minor          : NvU8;
                                      Patch          : NvU16;
                                      Build          : NvU16;
                                      Version_String : out Array_Version_String;
                                      Length         : out NvU8)
   is
      Offset : NvU32 := 0;

   begin

      Version_String := [others => 0];

      To_Decimal_String
        (Value          => Major,
         Decimal_String => Version_String (Offset .. (Offset + 3)));
      Offset := Offset + 4;
      Version_String (Offset) := Character'Pos ('.');
      Offset := Offset + 1;
      To_Decimal_String
        (Value          => Minor,
         Decimal_String => Version_String (Offset .. (Offset + 1)));
      Offset := Offset + 2;
      Version_String (Offset) := Character'Pos ('.');
      Offset := Offset + 1;
      To_Decimal_String
        (Value          => Patch,
         Decimal_String => Version_String (Offset .. (Offset + 3)));
      Offset := Offset + 4;
      Version_String (Offset) := Character'Pos ('.');
      Offset := Offset + 1;
      To_Decimal_String
        (Value          => Build,
         Decimal_String => Version_String (Offset .. (Offset + 3)));
      Offset := Offset + 4;

      Length := NvU8 (Offset and 16#FF#);

   end Populate_Version_String;

   procedure Get_Active_Stamp (Stamp : out NvU32)
   is

      Major    : NvU16  := 0;
      Minor    : NvU8  := 0;
      Patch    : NvU16 := 0;
      Build    : NvU16 := 0;

   begin

      Pldm_Get_Active_Version (Major  => Major,
                               Minor  => Minor,
                               Patch  => Patch,
                               Build  => Build);

      Populate_Stamp (Minor  => Minor,
                      Patch  => Patch,
                      Build  => Build,
                      Stamp  => Stamp);

   end Get_Active_Stamp;

   procedure Get_Active_Version_String (Version_String : out Array_Version_String;
                                                  Length         : out NvU8)
   is

      Major    : NvU16  := 0;
      Minor    : NvU8  := 0;
      Patch    : NvU16 := 0;
      Build    : NvU16 := 0;
   begin

      Pldm_Get_Active_Version (Major  => Major,
                               Minor  => Minor,
                               Patch  => Patch,
                               Build  => Build);

      Populate_Version_String (Major          => Major,
                               Minor          => Minor,
                               Patch          => Patch,
                               Build          => Build,
                               Version_String => Version_String,
                               Length         => Length);

   end Get_Active_Version_String;

   procedure Get_Pending_Stamp (Stamp : out NvU32)
   is

      Major    : NvU16  := 0;
      Minor    : NvU8  := 0;
      Patch    : NvU16 := 0;
      Build    : NvU16 := 0;

   begin

      Pldm_Get_Pending_Version (Major  => Major,
                                Minor  => Minor,
                                Patch  => Patch,
                                Build  => Build);

      Populate_Stamp (Minor  => Minor,
                      Patch  => Patch,
                      Build  => Build,
                      Stamp  => Stamp);

   end Get_Pending_Stamp;

   procedure Get_Pending_Version_String (Version_String : out Array_Version_String;
                                                   Length         : out NvU8)
   is
      Major    : NvU16  := 0;
      Minor    : NvU8  := 0;
      Patch    : NvU16 := 0;
      Build    : NvU16 := 0;
   begin

      Pldm_Get_Pending_Version (Major  => Major,
                                Minor  => Minor,
                                Patch  => Patch,
                                Build  => Build);

      Populate_Version_String (Major          => Major,
                               Minor          => Minor,
                               Patch          => Patch,
                               Build          => Build,
                               Version_String => Version_String,
                               Length         => Length);

   end Get_Pending_Version_String;

   --  @todo should remove this
   function Pldm_Get_Device_Identity (ArrAddress   : in System.Address) return NvU32
   is
      Pldm_Hdr                  : constant Pldm_Header := DEFAULT_PLDM_HEADER;
      Tx_Msg                    : Arr_Pldm_Tx_Msg_Buffer_Record  with Address => ArrAddress;
      Offset                    : NvU32 := 0;

   begin
      Pldm_Fill_Device_Identity_Resp (Pldm_Hdr => Pldm_Hdr,
                                      Tx_Msg   => Tx_Msg,
                                      Offset   => Offset);
      return Offset;
   end Pldm_Get_Device_Identity;

end Pdk.Pldm.Plat;


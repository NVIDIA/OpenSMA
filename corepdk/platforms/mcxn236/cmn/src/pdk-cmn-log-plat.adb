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
with Ada.Characters.Latin_1;
with Pdk.Cmn.Types; use Pdk.Cmn.Types;
with System;

package body Pdk.Cmn.Log.Plat is

   -- External C functions
   procedure cmn_log_plat_puts (Msg : System.Address)
     with Import => True, Convention => C, External_Name => "cmn_log_plat_puts";

   procedure cmn_log_plat_putc (C : Integer)
     with Import => True, Convention => C, External_Name => "cmn_log_plat_putc";

   -- Console logging with level
   function Print(Lvl: Level; Msg: String) return Status is
      pragma Unreferenced (Lvl);
      Append_Newline : constant Boolean :=
        (Msg'Length = 0)
        or else (Msg(Msg'Last) /= Ada.Characters.Latin_1.LF);
      Extra_Len     : constant Natural := (if Append_Newline then 1 else 0);
      Buffer        : Uint8_Array (Uint32(0) .. Uint32(Msg'Length + Extra_Len));
   begin
      for I in Msg'Range loop
         Buffer(Uint32(I - Msg'First)) := Character'Pos(Msg(I));
      end loop;
      if Append_Newline then
         Buffer(Uint32(Msg'Length)) := Character'Pos(Ada.Characters.Latin_1.LF);
      end if;
      Buffer(Buffer'Last) := 0;  -- null terminator
      cmn_log_plat_puts(Buffer'Address);
      return Ok;
   end Print;

   -- Persistent logging with level
   function Print_P(Lvl: Level; Msg: String) return Status is
      pragma Unreferenced (Lvl);
      Append_Newline : constant Boolean :=
        (Msg'Length = 0)
        or else (Msg(Msg'Last) /= Ada.Characters.Latin_1.LF);
      Extra_Len     : constant Natural := (if Append_Newline then 1 else 0);
      Buffer        : Uint8_Array (Uint32(0) .. Uint32(Msg'Length + Extra_Len));
   begin
      for I in Msg'Range loop
         Buffer(Uint32(I - Msg'First)) := Character'Pos(Msg(I));
      end loop;
      if Append_Newline then
         Buffer(Uint32(Msg'Length)) := Character'Pos(Ada.Characters.Latin_1.LF);
      end if;
      Buffer(Buffer'Last) := 0;  -- null terminator
      cmn_log_plat_puts(Buffer'Address);
      return Ok;
   end Print_P;

   -- Print a string (without level)
   procedure Print (Msg : String) is
      Append_Newline : constant Boolean :=
        (Msg'Length = 0)
        or else (Msg(Msg'Last) /= Ada.Characters.Latin_1.LF);
      Extra_Len     : constant Natural := (if Append_Newline then 1 else 0);
      Buffer        : Uint8_Array (Uint32(0) .. Uint32(Msg'Length + Extra_Len));
   begin
      for I in Msg'Range loop
         Buffer(Uint32(I - Msg'First)) := Character'Pos(Msg(I));
      end loop;
      if Append_Newline then
         Buffer(Uint32(Msg'Length)) := Character'Pos(Ada.Characters.Latin_1.LF);
      end if;
      Buffer(Buffer'Last) := 0;  -- null terminator
      cmn_log_plat_puts(Buffer'Address);
   end Print;

   -- Print a single character
   procedure Putchar (C : Character) is
   begin
      cmn_log_plat_putc(Character'Pos(C));
   end Putchar;

   -- Flush output (no-op on MCU)
   procedure Flush is
   begin
      null;
   end Flush;

end Pdk.Cmn.Log.Plat;


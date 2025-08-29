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
with Pdk.Cmn.Types; use Pdk.Cmn.Types;
with System;

package body Pdk.Cmn.Console.Plat is

   -- External C function: void plat_print(const char* msg);
   procedure plat_print (Msg : System.Address)
     with Import => True, Convention => C, External_Name => "cmn_console_plat_print";

   -- Print a string as null-terminated C-style string
   procedure Print (Msg : String) is
      Buffer : Uint8_Array (Uint32(0) .. Uint32(Msg'Length));
   begin
      for I in Msg'Range loop
         Buffer(Uint32(I - Msg'First)) := Character'Pos(Msg(I));
      end loop;

      Buffer(Buffer'Last) := 0;  -- null terminator
      plat_print(Buffer'Address);
   end Print;

   -- Print a single character
   procedure Putchar (C : Character) is
      Buffer : Uint8_Array (Uint32(0) .. Uint32(1));
   begin
      Buffer(Uint32(0)) := Character'Pos(C);
      Buffer(Uint32(1)) := 0;
      plat_print(Buffer'Address);
   end Putchar;

   procedure Flush is
   begin
      null;
   end Flush;

end Pdk.Cmn.Console.Plat;
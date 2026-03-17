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
with Interfaces.C;             use Interfaces.C;
with Interfaces.C.Extensions;  use Interfaces.C.Extensions;
with Interfaces.C.Strings;     use Interfaces.C.Strings;
with Pdk.Cmn.Flowcontrol.Plat; use Pdk.Cmn.Flowcontrol.Plat;
with Pdk.Cmn.Log.Plat;

package body Ubs.Unittest is

   use Pdk.Cmn;

   function Assertion_Check(Fatal, Cond: bool; Source_Location: chars_ptr) return Integer is

   begin
      if Cond then
         On_Pass(Value(Source_Location));
         return 0;
      end if;

      On_Fail(Boolean(Fatal), Value(Source_Location));
      return 1;
   end;

   procedure On_Fail(Fatal: Boolean; Loc: String := Gnat.Source_Info.Source_Location) is
   begin
      if Fatal then
         Log.Plat.Putchar('F');
      else
         Log.Plat.Putchar('W');
      end if;

      Log.Plat.Print(":" & Loc);
      Log.Plat.Putchar(Character'Val(10));

      if Fatal then
         Log.Plat.Flush;
         Exit_Program(1);  -- keep it straight forward and just exit
      end if;

   end;

   procedure On_Pass(Loc: String := Gnat.Source_Info.Source_Location) is
   begin
      Log.Plat.Print("P:" & Loc);
      Log.Plat.Putchar(Character'Val(10));
   end;
end;

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
with Pdk.Cmn.Console.Plat;
with Pdk.Cmn.Console.Colors;

package body Pdk.Cmn.Console is
   New_Line: constant Character := Character'Val(10);

   procedure Fatal(Msg: String) is
   begin
      Plat.Print(Colors.Bold_Red);
      Plat.Print("FATAL : ");
      Plat.Print(Colors.Normal);
      Plat.Print(Msg);
      Plat.Putchar(New_Line);
   end;

   procedure Error(Msg: String) is
   begin
      Plat.Print(Colors.Red);
      Plat.Print("ERROR : ");
      Plat.Print(Colors.Normal);
      Plat.Print(Msg);
      Plat.Putchar(New_Line);
   end;

   procedure Warning(Msg: String) is
   begin
      Plat.Print(Colors.Bold_Orange);
      Plat.Print("WARNING: ");
      Plat.Print(Colors.Normal);
      Plat.Print(Msg);
      Plat.Putchar(New_Line);
   end;

   procedure Debug(Msg: in String) is
   begin
      Plat.Print(Colors.Orange);
      Plat.Print("DEBUG : ");
      Plat.Print(Colors.Normal);
      Plat.Print(Msg);
      Plat.Putchar(New_Line);
   end;

   procedure Info(Msg: in String) is
   begin
      Plat.Print("INFO: ");
      Plat.Print(Colors.Dim);
      Plat.Print(Msg);
      Plat.Putchar(New_Line);
   end;

end;

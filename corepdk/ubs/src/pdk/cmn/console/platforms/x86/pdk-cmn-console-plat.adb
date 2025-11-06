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
with Ada.Text_IO;

package body Pdk.Cmn.Console.Plat is

   procedure Print(Msg: String) is
   begin
      Ada.Text_Io.Put(Msg);
   end;

   procedure Putchar(C: Character) is
   begin
      Ada.Text_Io.Put(C);
   end;

   procedure Flush is
   begin
      Ada.Text_Io.Flush;
   end;
end;

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
with Interfaces.C; use Interfaces.C;

-- TODO: This is temporary and will be removed
package body Nv.Common.Console is
   procedure Print (S : String) is
      escape : Boolean := False;
      procedure putchar (c : in int) with
        Import => True, Convention => C, External_Name => "DbgConsole_Putchar";
   begin
      for I in S'Range loop
         if S (I) = '\' then
            escape := True;
         else
            if escape then
               case S (I) is
                  when 'n' => putchar (10);
                  when 'r' => putchar (13);
                  when others => null;
               end case;
               escape := False;
            else
               putchar (int'Val (Character'Pos (S (I))));
            end if;
         end if;
      end loop;
   end Print;
end Nv.Common.Console;

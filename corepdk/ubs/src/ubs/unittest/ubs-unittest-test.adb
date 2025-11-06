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
with Ubs.Unittest.Assert; use Ubs.Unittest.Assert;

--- @summary Self test for CorePDK::UBS::Unittest
package body Ubs.Unittest.Test is

   procedure Assert_Strings is
      Abc: constant String := "abc";
   begin
      Eq("Abc", "Abc");
      Eq("", "");
      -- Eq("abc", "def", Fatal => False);
      -- Eq("abc", "def", Fatal => True);
      Eq(Abc, Abc);
      Ne(Abc, "Abc");
      Ne(Abc, "abcdef");
   end;

   procedure Assert_Booleans is
   begin
      Eq(True, True);
      Eq(False, False);
      Ne(False, True);
      Ne(True, False);
      Is_True(True);
      Is_False(False);
   end;

   procedure Assert_Integers is
   begin
      for I in 1 .. 8 loop
         Eq(I, I);
         Ne(I, I + 1);
         Gt(I, 0);
         Ge(I, 1);
         Lt(-I, 0);
         Le(-I, -1);
      end loop;
   end;

   procedure Assert_Floats is
      I: Float := 0.0;
   begin
      while I <= 1.0 loop
         Eq(I, I);
         Ne(I, I + Float'Epsilon);
         Gt(I, 0.0 - Float'Epsilon);
         Ge(I, 0.0);
         Lt(-I, Float'Small);
         Le(-I, 0.0);
         I := I + 0.1;
      end loop;
   end;

begin
   Assert_Strings;
   Assert_Booleans;
   Assert_Integers;
   Assert_Floats;
end;

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
with Ubs.Unittest.Assert; use Ubs.Unittest;
with Ubs.Features;

package body Test2.Test is
   procedure Featuretest is
   begin

#if Ubs_Global_Format_Test = 0 then
      Assert.Eq(Ubs.Features.Regtable, True);
#elsif Ubs_Global_Format_Test = 1 then
      Assert.Eq(Ubs.Features.Regtable, False);
#elsif Ubs_Global_Format_Test = 2 then
      Assert.Eq(Ubs.Features.Regtable, True);
#elsif Ubs_Global_Format_Test = 3 then
      Assert.Eq(Ubs.Features.Regtable, False);
#elsif Ubs_Global_Format_Test = 4 then
      Assert.Eq(Ubs.Features.Regtable, True);
#else
      Assert.Is_True(False);  -- this should not be reached
#end if;
      Assert.Eq(Ubs.Features.Count, 5);
   end;

begin
   Featuretest;
end;

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
with Interfaces.C;
with Interfaces.C.Extensions;
with Interfaces.C.Strings;
with GNAT.Source_Info;

package Ubs.Unittest with
  spark_mode => Off
is

   --- Called by assert routines to handle a fail.
   --- If fatal is true this will immediately exit
   --- @param Fatal    Consider as a warning only if False
   --- @param Loc      Source info. Should be in path:line format
   procedure On_Fail(Fatal: Boolean; Loc: String := Gnat.Source_Info.Source_Location);

   --- Called by assert routines to handle a pass.
   --- @param Loc      Source info. Should be in path:line format
   procedure On_Pass(Loc: String := Gnat.Source_Info.Source_Location);

   -- C API

   --- Called by C++ unittest framework
   --- @param Loc      Source info. Should be in path:line format
   function Assertion_Check
     (Fatal, Cond    : Interfaces.C.Extensions.Bool;
      Source_Location: Interfaces.C.Strings.Chars_Ptr) return Integer with
     export => True, convention => C, external_name => "ubs_assertion_check";

end;

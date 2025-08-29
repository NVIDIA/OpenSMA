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
with GNAT.Source_Info;

--- @summary
--- Generic package for assertions.
--- @description
--- Contains assertions used to verify the code behaviour to be called directly from inside unittests.
package Ubs.Unittest.Assert_Generics is
   generic
      type T(<>) is limited private;
      with function Image(A: T) return String;
      with function "="(A, B: T) return Boolean is <>;
      with function "<"(A, B: T) return Boolean is <>;
      with function "<="(A, B: T) return Boolean is <>;
      pragma unreferenced(Image);
   package Helpers is
    --- Tests A and B for equality.
    --- @param A        Left hand side of equality.
    --- @param B        Right hand side of equality.
    --- @param Source   Calling point source file and line.
    --- @param Fatal    May be set to False to turn assert into a warning.
      procedure Eq
        (A, B: T; Fatal: Boolean := True; Source: String := Gnat.Source_Info.Source_Location);

    --- Tests A and B for inequality.
    --- @param A        Left hand side of inequality.
    --- @param B        Right hand side of inequality.
    --- @param Source   Calling point source file and line.
    --- @param Fatal    May be set to False to turn assert into a warning.
      procedure Ne
        (A, B: T; Fatal: Boolean := True; Source: String := Gnat.Source_Info.Source_Location);

    --- Ensures that A is greater than B.
    --- @param A        Left hand side of comparison.
    --- @param B        Right hand side of comparison.
    --- @param Source   Calling point source file and line.
    --- @param Fatal    May be set to False to turn assert into a warning.
      procedure Gt
        (A, B: T; Fatal: Boolean := True; Source: String := Gnat.Source_Info.Source_Location);

    --- Ensures that A is greater than or equal to B.
    --- @param A        Left hand side of comparison.
    --- @param B        Right hand side of comparison.
    --- @param Source   Calling point source file and line.
    --- @param Fatal    May be set to False to turn assert into a warning.
      procedure Ge
        (A, B: T; Fatal: Boolean := True; Source: String := Gnat.Source_Info.Source_Location);

    --- Ensures that A is less than B.
    --- @param A        Left hand side of comparison.
    --- @param B        Right hand side of comparison.
    --- @param Source   Calling point source file and line.
    --- @param Fatal    May be set to False to turn assert into a warning.
      procedure Lt
        (A, B: T; Fatal: Boolean := True; Source: String := Gnat.Source_Info.Source_Location);

    --- Ensures that A is less than or equal to B.
    --- @param A        Left hand side of comparison.
    --- @param B        Right hand side of comparison.
    --- @param Source   Calling point source file and line.
    --- @param Fatal    May be set to False to turn assert into a warning.
      procedure Le
        (A, B: T; Fatal: Boolean := True; Source: String := Gnat.Source_Info.Source_Location);
   end;
end;

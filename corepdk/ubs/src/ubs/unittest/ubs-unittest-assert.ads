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
with Ubs.Unittest.Assert_Generics;

--- @summary
--- Renames of Boolean, String, Integer and Float Asserts.
package Ubs.Unittest.Assert with
  spark_mode => Off
is

  --- Assert that the condition is true.
  --- @param Cond     Condition to be asserted.
  --- @param Source   Calling point source file and line.
  --- @param Fatal    May be set to False to turn assert into a warning.
   procedure Is_True
     (Cond  : Boolean;
      Fatal : Boolean := True;
      Source: String  := Gnat.Source_Info.Source_Location);

  --- Assert that the condition is false.
  --- @param Cond     Condition to be asserted.
  --- @param Source   Calling point source file and line.
  --- @param Fatal    May be set to False to turn assert into a warning.
   procedure Is_False
     (Cond  : Boolean;
      Fatal : Boolean := True;
      Source: String  := Gnat.Source_Info.Source_Location);

  -- Booleans ----------------------------------------------------------------
   package Booleans is new Assert_Generics.Helpers(Boolean, Boolean'Image);
   procedure Eq
     (A, B  : Boolean;
      Fatal : Boolean := True;
      Source: String  := Gnat.Source_Info.Source_Location) renames
     Booleans.Eq;
   procedure Ne
     (A, B  : Boolean;
      Fatal : Boolean := True;
      Source: String  := Gnat.Source_Info.Source_Location) renames
     Booleans.Ne;

  -- Strings -----------------------------------------------------------------
   package Strings is new Assert_Generics.Helpers(String, String'Image);
   procedure Eq
     (A, B  : String;
      Fatal : Boolean := True;
      Source: String  := Gnat.Source_Info.Source_Location) renames
     Strings.Eq;
   procedure Ne
     (A, B  : String;
      Fatal : Boolean := True;
      Source: String  := Gnat.Source_Info.Source_Location) renames
     Strings.Ne;

  -- Integers ----------------------------------------------------------------
   package Integers is new Assert_Generics.Helpers(Integer, Integer'Image);
   procedure Eq
     (A, B  : Integer;
      Fatal : Boolean := True;
      Source: String  := Gnat.Source_Info.Source_Location) renames
     Integers.Eq;
   procedure Ne
     (A, B  : Integer;
      Fatal : Boolean := True;
      Source: String  := Gnat.Source_Info.Source_Location) renames
     Integers.Ne;
   procedure Gt
     (A, B  : Integer;
      Fatal : Boolean := True;
      Source: String  := Gnat.Source_Info.Source_Location) renames
     Integers.Gt;
   procedure Ge
     (A, B  : Integer;
      Fatal : Boolean := True;
      Source: String  := Gnat.Source_Info.Source_Location) renames
     Integers.Ge;
   procedure Lt
     (A, B  : Integer;
      Fatal : Boolean := True;
      Source: String  := Gnat.Source_Info.Source_Location) renames
     Integers.Lt;
   procedure Le
     (A, B  : Integer;
      Fatal : Boolean := True;
      Source: String  := Gnat.Source_Info.Source_Location) renames
     Integers.Le;

  -- Floats ------------------------------------------------------------------
   package Floats is new Assert_Generics.Helpers(Float, Float'Image);
   procedure Eq
     (A, B  : Float;
      Fatal : Boolean := True;
      Source: String  := Gnat.Source_Info.Source_Location) renames
     Floats.Eq;
   procedure Ne
     (A, B  : Float;
      Fatal : Boolean := True;
      Source: String  := Gnat.Source_Info.Source_Location) renames
     Floats.Ne;
   procedure Gt
     (A, B  : Float;
      Fatal : Boolean := True;
      Source: String  := Gnat.Source_Info.Source_Location) renames
     Floats.Gt;
   procedure Ge
     (A, B  : Float;
      Fatal : Boolean := True;
      Source: String  := Gnat.Source_Info.Source_Location) renames
     Floats.Ge;
   procedure Lt
     (A, B  : Float;
      Fatal : Boolean := True;
      Source: String  := Gnat.Source_Info.Source_Location) renames
     Floats.Lt;
   procedure Le
     (A, B  : Float;
      Fatal : Boolean := True;
      Source: String  := Gnat.Source_Info.Source_Location) renames
     Floats.Le;
end;

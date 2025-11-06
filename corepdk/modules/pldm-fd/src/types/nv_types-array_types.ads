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

--  @summary
--  Defining types
--
--  @description
--  Nv_Types.Array_Types is a child package of Nv_Types which declares different array types.

with Ada.Unchecked_Conversion;

package Nv_Types.Array_Types with
  SPARK_Mode => On
is

   --  ARR_NvU32_IDX7 is an unconstrained array of NvU32s whose index can be in range 0 to 127.
   --  Refer to  https://learn.adacore.com/courses/intro-to-ada/chapters/arrays.html#unconstrained-arrays.
   type ARR_NvU32_IDX7 is array (NvU7 range <>) of NvU32 with
     Pack;

   --  ARR_NvU32_IDX8 is an unconstrained array of NvU32s whose index can be in range 0 to 255.
   --  Refer to  https://learn.adacore.com/courses/intro-to-ada/chapters/arrays.html#unconstrained-arrays.
   type ARR_NvU32_IDX8 is array (NvU8 range <>) of NvU32 with
     Pack;

   --  ARR_NvU8_IDX8 is an unconstrained array of NvU8s whose index can be in range 0 to 255.
   --  Refer to  https://learn.adacore.com/courses/intro-to-ada/chapters/arrays.html#unconstrained-arrays.
   type ARR_NvU8_IDX8 is array (NvU8 range <>) of NvU8 with
     Pack;

   --  ARR_NvU8_IDX16 is an unconstrained array of NvU8s whose index can be in range 0 to 2^16-1.
   --  Refer to  https://learn.adacore.com/courses/intro-to-ada/chapters/arrays.html#unconstrained-arrays.
   type ARR_NvU8_IDX16 is array (NvU16 range <>) of NvU8 with
     Pack;

   --  ARR_NvU8_IDX32 is an unconstrained array of NvU8s whose index can be in range 0 to 2^32-1.
   --  Refer to  https://learn.adacore.com/courses/intro-to-ada/chapters/arrays.html#unconstrained-arrays.
   type ARR_NvU8_IDX32 is array (NvU32 range <>) of NvU8 with
     Pack;

   --  ARR_NvU16_IDX8 is an unconstrained array of NvU16s whose index can be in range 0 to 255.
   --  Refer to  https://learn.adacore.com/courses/intro-to-ada/chapters/arrays.html#unconstrained-arrays.
   type ARR_NvU16_IDX8 is array (NvU8 range <>) of NvU16 with
     Pack;

   --  ARR_NvU16_IDX16 is an unconstrained array of NvU16s whose index can be in range 0 to 2^16-1.
   --  Refer to  https://learn.adacore.com/courses/intro-to-ada/chapters/arrays.html#unconstrained-arrays.
   type ARR_NvU16_IDX16 is array (NvU16 range <>) of NvU16 with
     Pack;

   --  ARR_NvU32_IDX16 is an unconstrained array of NvU32s whose index can be in range 0 to 2^16-1.
   --  Refer to  https://learn.adacore.com/courses/intro-to-ada/chapters/arrays.html#unconstrained-arrays.
   type ARR_NvU32_IDX16 is array (NvU16 range <>) of NvU32 with
     Pack;

   --  ARR_NvU32_IDX32 is an unconstrained array of NvU32s whose index can be in range 0 to 2^32-1.
   --  Refer to  https://learn.adacore.com/courses/intro-to-ada/chapters/arrays.html#unconstrained-arrays.
   type ARR_NvU32_IDX32 is array (NvU32 range <>) of NvU32 with
     Pack;

   --  ARR_NvU24_IDX32 is an unconstrained array of NvU24s whose index can be in range 0 to 2^32-1.
   --  Refer to  https://learn.adacore.com/courses/intro-to-ada/chapters/arrays.html#unconstrained-arrays.
   type ARR_NvU24_IDX32 is array (NvU32 range <>) of NvU24 with
     Pack;

   --  ARR_NvU32_IDX32 is an unconstrained array of NvU32s whose index can be in range 0 to 2^32-1.
   --  Refer to  https://learn.adacore.com/courses/intro-to-ada/chapters/arrays.html#unconstrained-arrays.
   type ARR_BOOL_IDX32 is array (NvU32 range <>) of Boolean with
     Pack;

   --  ARR_NvU16_IDX32 is an unconstrained array of NvU16s whose index can be in range 0 to 2^32-1.
   --  Refer to  https://learn.adacore.com/courses/intro-to-ada/chapters/arrays.html#unconstrained-arrays.
   type ARR_NvU16_IDX32 is array (NvU32 range <>) of NvU16;

   type Nv_DWords_Generic is array (NvU32 range <>) of NvU32 with
     Predicate =>
      Nv_DWords_Generic'First = 0 and then Nv_DWords_Generic'First < Nv_DWords_Generic'Last;

   subtype Nv_DWord_Pack64_Range is NvU32 range 0 .. 1;
   --  The DWords of QWord
   type Nv_DWord_Pack64 is new Nv_DWords_Generic (Nv_DWord_Pack64_Range) with
     Pack, Object_Size => 64, Alignment => 8;

   pragma Assert (Nv_DWord_Pack64'Size = NvU64'Size);
   function To_DWord_Pack64 is new Ada.Unchecked_Conversion
     (Source => NvU64, Target => Nv_DWord_Pack64);

   pragma Annotate
     (GNATprove, False_Positive, "type is unsuitable as a target for unchecked conversion",
      "Fix me");

end Nv_Types.Array_Types;

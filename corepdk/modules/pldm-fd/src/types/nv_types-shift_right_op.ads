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
--  Nv_Types.Shift_Right_Op is a child package of Nv_Types which declares right shift operations on NvU8, NvU16, NvU32
--  and NvU64.
package Nv_Types.Shift_Right_Op with
  SPARK_Mode => On
is

   --  Requirement: This procedure shall do the following:
   --               1) Takes NvU8 as input and right shift by the amount specified.
   --  @param  Value Input parameter : Value to be right shifted
   --  @param  Amount Input parameter : Amount of right shift
   --  @return Right shifted value by the amount specified
   function Safe_Shift_Right
     (Value  : NvU8;
      Amount : Natural)
      return NvU8 is (Shift_Right (Value, Amount)) with
     Pre     => Amount >= 0 and then Amount <= NvU8'Size, Global => null,
     Depends => (Safe_Shift_Right'Result =>
         (Value,
          Amount)), Inline_Always;

   --  Requirement: This procedure shall do the following:
   --               1) Takes NvU16 as input and right shift by the amount specified.
   --  @param  Value Input parameter : Value to be right shifted
   --  @param  Amount Input parameter : Amount of right shift
   --  @return Right shifted value by the amount specified
   function Safe_Shift_Right
     (Value  : NvU16;
      Amount : Natural)
      return NvU16 is (Shift_Right (Value, Amount)) with
     Pre     => Amount >= 0 and then Amount <= NvU16'Size, Global => null,
     Depends => (Safe_Shift_Right'Result =>
         (Value,
          Amount)), Inline_Always;

   --  Requirement: This procedure shall do the following:
   --               1) Takes NvU32 as input and right shift by the amount specified.
   --  @param  Value Input parameter : Value to be right shifted
   --  @param  Amount Input parameter : Amount of right shift
   --  @return Right shifted value by the amount specified
   function Safe_Shift_Right
     (Value  : NvU32;
      Amount : Natural)
      return NvU32 is (Shift_Right (Value, Amount)) with
     Pre     => Amount >= 0 and then Amount <= NvU32'Size, Global => null,
     Depends => (Safe_Shift_Right'Result =>
         (Value,
          Amount)), Inline_Always;

   --  Requirement: This procedure shall do the following:
   --               1) Takes NvU64 as input and right shift by the amount specified.
   --  @param  Value Input parameter : Value to be right shifted
   --  @param  Amount Input parameter : Amount of right shift
   --  @return Right shifted value by the amount specified
   function Safe_Shift_Right
     (Value  : NvU64;
      Amount : Natural)
      return NvU64 is (Shift_Right (Value, Amount)) with
     Pre     => Amount >= 0 and then Amount <= NvU64'Size, Global => null,
     Depends => (Safe_Shift_Right'Result =>
         (Value,
          Amount)), Inline_Always;

end Nv_Types.Shift_Right_Op;

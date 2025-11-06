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
--  Implement sizeof operations for different types.
--
--  @description
--  Defines functions which return size of a particular type.

package Nv_Types.Get_Size with
  SPARK_Mode => On
is

   --  Requirement: This procedure shall do the following:
   --               1) Takes size in bits as input
   --               2) Outputs the equivalent Size in Bytes.
   --  @param  Size Input parameter : Size in bits to be converted
   --  @return Output the size in bytes
   function Get_Size_Bytes
     (Size : NvU32)
      return NvU32 is (Size / NvU8'Size) with
     Pre     => (Size >= NvU8'Size and then Size mod NvU8'Size = 0), Global => null,
     Depends => (Get_Size_Bytes'Result => Size), Inline_Always;

   --  Requirement: This procedure shall do the following:
   --               1) Takes size in bits as input
   --               2) Outputs the equivalent Size in Dwords.
   --  @param  Size Input parameter : Size in bits to be converted
   --  @return Output the size in Dwords
   function Get_Size_Dwords
     (Size : NvU32)
      return NvU32 is (Size / NvU32'Size) with
     Pre     => (Size >= NvU32'Size and then Size mod NvU32'Size = 0), Global => null,
     Depends => (Get_Size_Dwords'Result => Size), Inline_Always;

end Nv_Types.Get_Size;

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
--  Declaring modular types in SPARK with overflow check.
--  https://docs.adacore.com/live/wave/spark2014/html/spark2014_ug/en/appendix/additional_annotate_pragmas.html
--  #using-annotations-to-request-overflow-checking-on-modular-types
package Nv_Types with
  SPARK_Mode => On
is

   type NvU1 is mod 2**1 with
     Size => 1, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU2 is mod 2**2 with
     Size => 2, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU3 is mod 2**3 with
     Size => 3, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU4 is mod 2**4 with
     Size => 4, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU5 is mod 2**5 with
     Size => 5, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU6 is mod 2**6 with
     Size => 6, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU7 is mod 2**7 with
     Size => 7, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU8 is mod 2**8 with
     Size => 8, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU9 is mod 2**9 with
     Size => 9, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU10 is mod 2**10 with
     Size => 10, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU11 is mod 2**11 with
     Size => 11, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU12 is mod 2**12 with
     Size => 12, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU13 is mod 2**13 with
     Size => 13, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU14 is mod 2**14 with
     Size => 14, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU15 is mod 2**15 with
     Size => 15, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU16 is mod 2**16 with
     Size => 16, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU17 is mod 2**17 with
     Size => 17, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU18 is mod 2**18 with
     Size => 18, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU19 is mod 2**19 with
     Size => 19, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU20 is mod 2**20 with
     Size => 20, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU21 is mod 2**21 with
     Size => 21, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU22 is mod 2**22 with
     Size => 22, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU23 is mod 2**23 with
     Size => 23, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU24 is mod 2**24 with
     Size => 24, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU25 is mod 2**25 with
     Size => 25, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU26 is mod 2**26 with
     Size => 26, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU27 is mod 2**27 with
     Size => 27, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU28 is mod 2**28 with
     Size => 28, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU29 is mod 2**29 with
     Size => 29, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU30 is mod 2**30 with
     Size => 30, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU31 is mod 2**31 with
     Size => 31, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU32 is mod 2**32 with
     Size => 32, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU33 is mod 2**33 with
     Size => 33, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU34 is mod 2**34 with
     Size => 34, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU35 is mod 2**35 with
     Size => 35, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU36 is mod 2**36 with
     Size => 36, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU37 is mod 2**37 with
     Size => 37, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU38 is mod 2**38 with
     Size => 38, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU39 is mod 2**39 with
     Size => 39, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU40 is mod 2**40 with
     Size => 40, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU41 is mod 2**41 with
     Size => 41, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU42 is mod 2**42 with
     Size => 42, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU43 is mod 2**43 with
     Size => 43, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU44 is mod 2**44 with
     Size => 44, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU45 is mod 2**45 with
     Size => 45, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU46 is mod 2**46 with
     Size => 46, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU47 is mod 2**47 with
     Size => 47, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU48 is mod 2**48 with
     Size => 48, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU49 is mod 2**49 with
     Size => 49, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU50 is mod 2**50 with
     Size => 50, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU51 is mod 2**51 with
     Size => 51, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU52 is mod 2**52 with
     Size => 52, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU53 is mod 2**53 with
     Size => 53, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU54 is mod 2**54 with
     Size => 54, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU55 is mod 2**55 with
     Size => 55, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU56 is mod 2**56 with
     Size => 56, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU57 is mod 2**57 with
     Size => 57, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU58 is mod 2**58 with
     Size => 58, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU59 is mod 2**59 with
     Size => 59, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU60 is mod 2**60 with
     Size => 60, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU61 is mod 2**61 with
     Size => 61, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU62 is mod 2**62 with
     Size => 62, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU63 is mod 2**63 with
     Size => 63, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvU64 is mod 2**64 with
     Size => 64, Annotate => (GNATprove,
       No_Wrap_Around);

   type NvS16 is range -2**15 .. 2**15 - 1 with
     Size => 16;

   type NvS24 is range -2**23 .. 2**23 - 1 with
     Size => 24;

   type NvS32 is range -2**31 .. 2**31 - 1 with
     Size => 32;

   type NvU64_Align32 is record
      Low  : NvU32;
      High : NvU32;
   end record with
     Size => 64, Object_Size => 64, Alignment => 4;

   type HBool is new Boolean;
   for HBool use
     (16#55aa#,
      16#aa55#);
   for HBool'Size use 16;

   --  WARNING WARNING WARNING: Unless you also use Nv_Types.Shift_Left_Op and Nv_Types.Shift_Right_Op, shift
   --  operations will permit shift amount greater than or equal to the number of bits in the operand. Furthermore,
   --  there could be loss of bits. However, those additional constraints are being kept as a separate package to
   --  ensure that scenarios that intentionally want to violate one or more of such constraints while still using NvUXX
   --  types instead of NvUxx_Wrap types can do so without needing to create their own copy of Nv_Types. Usage of
   --  Nv_Types.Shift_Left_Op and Nv_Types.Shift_Right_Op is highly recommended.
   --  Every usage violating this must be thoroughly reviewed.
   pragma Provide_Shift_Operators (NvU8);
   pragma Provide_Shift_Operators (NvU16);
   pragma Provide_Shift_Operators (NvU32);
   pragma Provide_Shift_Operators (NvU64);

end Nv_Types;

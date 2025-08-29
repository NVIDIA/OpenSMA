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
package Pdk.Cmn.Types is
   -- Define bit-limited integer types
   type Uint1 is mod 2**1;
   type Uint2 is mod 2**2;
   type Uint3 is mod 2**3;
   type Uint4 is mod 2**4;
   type Uint5 is mod 2**5;
   type Uint7 is mod 2**7;
   type Uint8 is mod 2**8;
   type Uint16 is mod 2**16;
   type Uint32 is mod 2**32;
   type Uint64 is mod 2**64;
   -- Define Array of bit-limited integers
   type Uint8_Array is array(Uint32 range <>) of Uint8;
end Pdk.Cmn.Types;

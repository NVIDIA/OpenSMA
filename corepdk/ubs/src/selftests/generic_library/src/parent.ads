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
generic
   type Src_Media_Size_T is mod <>;
package Parent with
  spark_mode
is

   type Buf_Element_T is mod 2**8;
   subtype Src_Media_Offset_T is Src_Media_Size_T range 0 .. Src_Media_Size_T'Last - 1;
   type Src_Media_Buf_T is array(Src_Media_Offset_T range <>) of Buf_Element_T with
     component_size => Buf_Element_T'Size, alignment => 1;

   generic
      type Payload_T is private;
   procedure Read_Payload(P: aliased out Payload_T);

   procedure Read_Media(P: out Src_Media_Buf_T);

end Parent;

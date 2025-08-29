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
with Ada.Text_IO; use Ada.Text_IO;
package body Parent with
  spark_mode => On
is

   procedure Read_Payload(P: aliased out Payload_T) is

      subtype Payload_Buf_T is
        Src_Media_Buf_T(0 .. Src_Media_Offset_T(Payload_T'Size / Buf_Element_T'Size - 1));
      Src_Media_Buf: aliased Payload_Buf_T with
        alignment => 1, import, address => P'Address;
   begin
      Put_Line("Payload size: " & Payload_T'Size'Image);
      Read_Media(Src_Media_Buf);
   end Read_Payload;

   procedure Read_Media(P: out Src_Media_Buf_T) is
   begin
      null;
   end Read_Media;

end Parent;

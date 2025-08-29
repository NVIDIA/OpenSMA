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
with Concrete;
with Ada.Text_IO; use Ada.Text_IO;

procedure Mainconcrete is
   -- Define a type to use with Read_Payload
   type My_Payload is new Integer;
   My_Payload_Instance: aliased My_Payload := 0;
   procedure Read_My_Record is new Concrete.Parent_Instance.Read_Payload(My_Payload);

begin
   -- Call the Read_Payload procedure from the Parent_Instance
   Read_My_Record(My_Payload_Instance);

   -- Output a message
   Put_Line("Mainconcrete Payload processing complete.");
end Mainconcrete;

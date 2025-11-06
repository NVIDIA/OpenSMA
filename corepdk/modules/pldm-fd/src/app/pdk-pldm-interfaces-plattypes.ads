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
with Nv_Types; use Nv_Types;

package Pdk.Pldm.Interfaces.Plattypes with
  SPARK_Mode => On
is

   type Pldmfw_Interface_Info is record
      Client         : NvU8;
      Src_Eid        : NvU8;
      Header_Version : NvU8;
      Message_Tag    : NvU8;
   end record with
     Size => 32, Object_Size => 32, Alignment => 1;

   DEFAULT_PLDMFW_INTERFACE_INFO : constant Pldmfw_Interface_Info :=
     (0,
      0,
      0,
      0);

end Pdk.Pldm.Interfaces.Plattypes;

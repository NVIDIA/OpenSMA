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

with Pdk.Pldm.Packet;      use Pdk.Pldm.Packet;
with Nv_Types;             use Nv_Types;
with Nv_Types.Array_Types; use Nv_Types.Array_Types;

package Pdk.Pldm.Plattypes with
  SPARK_Mode => On
is

   type Queue_Element is record
      Data   : Arr_Pldm_Tx_Msg_Buffer;
      Length : NvU32 := 0; -- Actual used length
   end record;

end Pdk.Pldm.Plattypes;

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

package Pdk.Pldm.Device.Cmd with
  SPARK_Mode => Off
is

   procedure Get_Device_Id_Length
     (Device_Descriptors       :     Device_Descriptor_List;
      Vendor_Descriptors       :     Vendor_Descriptor_List;
      Device_Descriptors_Count :     Descriptor_Count;
      Vendor_Descriptors_Count :     Descriptor_Count;
      Device_Id_Length         : out NvU32);

   procedure Pldm_Fill_Device_Identity_Resp
     (Pldm_Hdr :     Pldm_Header;
      Tx_Msg   : out Arr_Pldm_Tx_Msg_Buffer_Record;
      Offset   : out NvU32);

end Pdk.Pldm.Device.Cmd;

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

with Nv_Types.Array_Types; use Nv_Types.Array_Types;
with Nv_Types; use Nv_Types;
with System;
with Pdk.Pldm.Fwupdate; use Pdk.Pldm.Fwupdate;

package Pldm_Wrap
with SPARK_Mode => On
is

   procedure Send_Pldm_Msg_To_Mctp (Buffer         : ARR_NvU8_IDX32;
                                    Client         : NvU8;
                                    Src_Eid        : NvU8;
                                    Header_Version : NvU8;
                                    Message_Tag    : NvU8;
                                    Size           : NvU32;
                                    Is_Request     : NvU8);
   pragma Import (C, Send_Pldm_Msg_To_Mctp, "send_pldm_msg_to_mctp");

   procedure Pldm_Write (Component_Id : NvU16;
                            Buffer      : ARR_NvU8_IDX32;
                            Offset      : NvU32;
                            Size       : NvU32;
                            Err         : in out NvU16);
   pragma Import (C, Pldm_Write, "pldm_write");

   procedure Pldm_Update_Pds (Component_Id  : NvU16;
                              Status   : NvU8;
                              Err      : in out NvU16);
   pragma Import (C, Pldm_Update_Pds, "pldm_update_pds");

   procedure Pldm_Get_Active_Version (Major    : in out NvU16;
                                      Minor    : in out NvU8;
                                      Patch    : in out NvU16;
                                      Build    : in out NvU16);
   pragma Import (C, Pldm_Get_Active_Version, "pldm_get_active_version");

   procedure Pldm_Get_Ap_Active_Version (Major    : in out NvU16;
                                      Minor    : in out NvU8;
                                      Patch    : in out NvU16;
                                      Build    : in out NvU16);
   pragma Import (C, Pldm_Get_Ap_Active_Version, "pldm_get_ap_active_version");

   procedure Pldm_Get_Pending_Version (Major    : in out NvU16;
                                       Minor    : in out NvU8;
                                       Patch    : in out NvU16;
                                       Build    : in out NvU16);
   pragma Import (C, Pldm_Get_Pending_Version, "pldm_get_pending_version");

   procedure Pldm_Get_Ap_Pending_Version (Major    : in out NvU16;
                                      Minor    : in out NvU8;
                                      Patch    : in out NvU16;
                                      Build    : in out NvU16);
   pragma Import (C, Pldm_Get_Ap_Pending_Version, "pldm_get_ap_pending_version");

   procedure Set_Timer (Ns : NvU64);
   pragma Import (C, Set_Timer, "set_timer");

   procedure Clear_Timer;
   pragma Import (C, Clear_Timer, "clear_timer");

   procedure Get_Ticks (Ticks : in out NvU32);
   pragma Import (C, Get_Ticks, "get_ticks");

   procedure Set_Log (Event : NvU16;
                      Info0 : NvU8;
                      Info1 : NvU8;
                      Info2 : NvU8;
                      Info3 : NvU8);
   pragma Import (C, Set_Log, "set_log");

   procedure Get_Fw_Info (Input_Component_Id : NvU16;
                          Output_Component_Id : in out NvU16;
                          Fw_Size      : in out NvU32;
                          Fw_Offset    : in out NvU32;
                          Ap_Sku_Id    : in out NvU32;
                          Build_Mode   : in out NvU8;
                          Fw_State     : in out NvU8;
                          Is_Valid     : in out NvU8);
   pragma Import (C, Get_Fw_Info, "get_fw_info");

   procedure Get_FW_Info_From_Metadata (Metadata : in ARR_NvU8_IDX16;
                                        Minor : out NvU8;
                                        Patch : out NvU16;
                                        Build : out NvU16;
                                        Ap_Sku_Id : out NvU32);
   pragma Import (C, Get_FW_Info_From_Metadata, "get_fw_info_from_metadata");

   procedure Parse_Header (Component_Id     : NvU16;
                           Current_fw_offset : NvU32;
                           Transfer_size     : NvU32;
                           Buffer     : ARR_NvU8_IDX32;
                           Metadata : in out ARR_NvU8_IDX16;
                           Is_Recv_All_Metadata : out NvU8);
   pragma Import (C, Parse_Header, "parse_header");

   procedure Get_Pcie_Info (Vendor_Id           : in out NvU16;
                            Device_Id           : in out NvU16;
                            Subsystem_Vendor_Id : in out NvU16;
                            Subsystem_Device_Id : in out NvU16);
   pragma Import (C, Get_Pcie_Info, "get_pcie_info");

   procedure Get_Uuid_Info (Uuid : in out ARR_NvU8_IDX32);
   pragma Import (C, Get_Uuid_Info, "get_uuid_info");

   procedure Request_Authentication (Comp_id: NvU16;
                                     Auth_Request_id : in out NvU32);
   pragma Import (C, Request_Authentication, "request_authentication");

   procedure Print_Msg (Data : NvU32);
   pragma Import (C, Print_Msg, "print_msg");

   procedure Get_Arr_Comp_Id_Len (ArrLen     : in out NvU32);
   pragma Import (C, Get_Arr_Comp_Id_Len, "get_arr_comp_id_len");

   function  Get_Arr_Comp_Id_Address return System.Address;
   pragma Import(C,Get_Arr_Comp_Id_Address,"get_arr_comp_id_address");

   subtype Comp_Id_Array is ARR_NvU16_IDX32 (0 .. MAX_PASS_COMPONENT_ID_NUM - 1);

   function Get_Write_Fail_Retry (Comp_id: NvU16) return NvU8;
   pragma Import (C, Get_Write_Fail_Retry, "get_write_fail_retry");

end Pldm_Wrap;

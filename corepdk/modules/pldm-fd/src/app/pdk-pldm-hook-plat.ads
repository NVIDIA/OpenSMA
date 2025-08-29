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

with Nv_Types;                      use Nv_Types;
with Nv_Types.Array_Types;          use Nv_Types.Array_Types;
with Pdk.Pldm.Device;               use Pdk.Pldm.Device;
with Pdk.Pldm.Fwupdate;             use Pdk.Pldm.Fwupdate;
with Pdk.Pldm.Hook.Plattypes;       use Pdk.Pldm.Hook.Plattypes;
with Pdk.Pldm.Interfaces.Plattypes; use Pdk.Pldm.Interfaces.Plattypes;

package Pdk.Pldm.Hook.Plat with
  SPARK_Mode => On
is
   --  ///////////////////
   --  /// Hook Up API ///
   --  ///////////////////
   --    Users can modify the implementation to define their own actions in the
   --    following hook up procedure. For example, users can implement custom logging,
   --    metrics collection, or state verification.

   --  Procedure: Pldm_Hook_Set_State_Cmd_Debug
   --
   --  Purpose:
   --  This procedure is a customizable hook that allows users to implement their own
   --  actions when a PLDM state change or command occurs. The default behavior
   --  logs errors if the return value (`Ret`) is not `Pldmfw_Ret_Success`.
   --
   --  Parameters:
   --    State     : The current state of the PLDM firmware (type `Pldmfw_State`).
   --                Represents the state to which the firmware should transition.
   --
   --    Cmd       : The PLDM command (type `NvU8`). Specifies the command being sent
   --                to the firmware during state transition.
   --
   --    Ret       : The return status (type `Pldmfw_Ret`). If the status indicates
   --                an error, the procedure logs the error information (default behavior).
   --
   --    Extra_Ret : (Optional) Additional return data (type `NvU32`). Can be used for
   --                extra debugging or additional information handling.
   --                The default value is `0` if not explicitly provided.

   procedure Pldm_Hook_Set_State_Cmd_Debug
     (State     : Pldmfw_State;
      Cmd       : NvU8;
      Ret       : Pldmfw_Ret;
      Extra_Ret : NvU32 := 0);

   --  @todo write the document for following api
   procedure Pldm_Hook_Debug
     (Msg  : String;
      data : NvU32);

   generic
      type Payload_Type is private;

   procedure Pldm_Hook_Send_Buffer
     (Data         : Payload_Type;
      Rx_Interface : Pldmfw_Interface_Info;
      Size         : NvU32;
      Is_Request   : Boolean := False);

   generic
      type Payload_Type is private;

   procedure Pldm_Hook_Image_Write
     (Data   :     Payload_Type;
      Offset :     NvU32;
      Size   :     NvU32;
      Ret    : out Pldmfw_Ret);

   procedure Pldm_Hook_Get_Time_Milli_Seconds (Timestamp : out NvU32);

   procedure Pldm_Hook_Set_Timer
     (Ret          : out Pldmfw_Ret;
      Timestamp_Ns :     NvU64);

   procedure Pldm_Hook_Clear_Timer (Ret : out Pldmfw_Ret);

   procedure Pldm_Hook_Get_Component_Info_List
     (Component_Infos  : out Component_Info_List;
      Component_Length : out Component_Count);

   UPDATE_INPROGRESS : constant := 0;
   UPDATE_COMPLETE   : constant := 1;
   UPDATE_STAGED     : constant := 2;

   procedure Pldm_Hook_Set_Update_State
     (Status :     NvU8;
      Ret    : out Pldmfw_Ret) with
     Global => null;

   procedure Pldm_Hook_Populate_Descriptors
     (Device_Descriptors       : out Device_Descriptor_List;
      Vendor_Descriptors       : out Vendor_Descriptor_List;
      Device_Descriptors_Count : out Descriptor_Count;
      Vendor_Descriptors_Count : out Descriptor_Count) with
     Global => null;

   procedure Pldm_Hook_Collect_Metadata
     (Component_Id         :        NvU16;
      Current_fw_offset    :        NvU32;
      Transfer_size        :        NvU32;
      Data                 :        Arr_4k_Buffer;
      Is_Recv_All_Metadata :    out NvU8;
      Metadata             : in out ARR_NvU8_IDX16);

   procedure Pldm_Hook_Auth_Metadata
     (Component_Id    :     NvU16;
      Is_Force_Update :     Boolean;
      Metadata        :     ARR_NvU8_IDX16;
      Ret             : out Pldmfw_Ret);

   procedure Pldm_Hook_Handle_Activate
     (Component_Id :     NvU16;
      Ret          : out Pldmfw_Ret);

   procedure Pldm_Hook_Set_Log
     (Event : Log_Event;
      Info0 : NvU8 := 0;
      Info1 : NvU8 := 0;
      Info2 : NvU8 := 0;
      Info3 : NvU8 := 0);

   procedure Pldm_Hook_Log_Fw_Update_Report
     (Fw_Update_Offset : NvU32;
      Total_Retry      : NvU32;
      Idle_Reason_Code : NvU32;
      M0_Time          : NvU32;
      M1_Time          : NvU32);

   procedure Pldm_Hook_Handle_Stage_Update
     (Component_Id :     NvU16;
      Ret          : out Pldmfw_Ret);

   procedure Pldm_Hook_Is_Staged
     (Component_Id :     NvU16;
      Is_Staged    : out Boolean);

   procedure Pldm_Hook_Request_Authenticate;

   procedure Pldm_Hook_Get_Limited_Rate_Info (Info : out Limited_Rate_Info);

end Pdk.Pldm.Hook.Plat;

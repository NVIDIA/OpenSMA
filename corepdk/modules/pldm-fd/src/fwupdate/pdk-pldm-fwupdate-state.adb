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

with Nv_Types.Get_Size;       use Nv_Types.Get_Size;
with Pdk.Pldm.Fwupdate.Cmd;   use Pdk.Pldm.Fwupdate.Cmd;
with Pdk.Pldm.Hook.Plattypes; use Pdk.Pldm.Hook.Plattypes;
with Pdk.Pldm.Hook.Plat;      use Pdk.Pldm.Hook.Plat;
with Pdk.Pldm.Packet;         use Pdk.Pldm.Packet;

package body Pdk.Pldm.Fwupdate.State with
  SPARK_Mode => On
is

   procedure Pldmfw_Change_State
     (Ret       :    out Pldmfw_Ret;
      Context   : in out Pldm_Context;
      New_State :        Pldmfw_State)
   is
   begin

      Context.Pldm_Previous_State := Context.Pldm_State;
      Context.Pldm_State          := New_State;

      case Context.Pldm_State is
         when PLDMFW_STATE_IDLE =>

            if Context.Idle_Reason_Code /= PLDMFW_CMD_GET_STATUS_REASON_INIT_FW_DEVICE
              and then Context.Idle_Reason_Code /= PLDMFW_CMD_GET_STATUS_REASON_ACTIVE_FW
            then
               Pldmfw_Generate_Update_Report (Context => Context);
            end if;

            Pldm_Hook_Clear_Timer (Ret);
         when PLDMFW_STATE_DL =>
            Pldm_Hook_Set_Log
              (Event => Change_State, Info0 => Context.Pldm_Previous_State,
               Info2 => Context.Pldm_State);
            Pldm_Hook_Set_Timer (Ret, PLDMFW_TIMEING_PT2 * MSEC_TO_NSEC);
         when PLDMFW_STATE_VERIFY | PLDMFW_STATE_APPLY =>
            Pldm_Hook_Set_Timer (Ret, PLDMFW_TIMEING_PT2 * MSEC_TO_NSEC);
         when PLDMFW_STATE_LC | PLDMFW_STATE_RDY_XFER | PLDMFW_STATE_ACTIVATE =>
            Pldm_Hook_Set_Timer (Ret, PLDMFW_FD_TIMEOUT_T1 * MSEC_TO_NSEC);
      end case;

   end Pldmfw_Change_State;

   procedure Pldmfw_State_Idle_Handler
     (Context : in out Pldm_Context;
      Size    :        NvU32)
   is

      Ret : Pldmfw_Ret := Pldmfw_Ret_Success;
      Hdr : Pldm_Header;
   begin

      Hdr :=
        Uc_Array_To_Pldm_Header
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Pldm_Header'Size) - 1)));

      if Hdr.Command_Code = PLDMFW_COMMAND_ACTIVATE_FW
      then
         Pldmfw_Response_Non_Idempotent_Cmd
           (Ret => Ret, Context => Context, Iid => Hdr.Iid, Cmd => Hdr.Command_Code);
         if Ret /= Pldmfw_Ret_Success
         then
            goto Exit_Point;
         end if;
      end if;

      case Hdr.Command_Code is
         when PLDMFW_COMMAND_QUERY_DEVICE_ID =>
            Pldmfw_Cmd_Query_Device_Id (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_GET_FW_PARAMETERS =>
            Pldmfw_Cmd_Get_Fw_Parameters (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_REQUEST_UPDATE =>
            Pldmfw_Cmd_Request_Update (Ret => Ret, Context => Context, Size => Size);
            if Ret = Pldmfw_Ret_Success
            then
               Pldmfw_Change_State
                 (Ret => Ret, Context => Context, New_State => PLDMFW_STATE_LC);
            end if;
         when PLDMFW_COMMAND_GET_STATUS =>
            Pldmfw_Cmd_Get_Status (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_PASS_COMPONENT_TABLE | PLDMFW_COMMAND_UPDATE_COMPONENT
           | PLDMFW_COMMAND_ACTIVATE_FW | PLDMFW_COMMAND_CANCEL_UPDATE_COMPONENT
           | PLDMFW_COMMAND_CANCEL_UPDATE =>
            Pldmfw_Response_Error_Code (Context => Context, CC => PLDMFW_CC_NOT_IN_UPDATE_MODE);
            Ret := Pldmfw_Ret_Unsupport_Cmd;
         when PLDMFW_COMMAND_REQUEST_FW_DATA | PLDMFW_COMMAND_TRANSFER_COMPLETE
           | PLDMFW_COMMAND_VERIFY_COMPLETE | PLDMFW_COMMAND_APPLY_COMPLETE =>
            --  do nothing for response cmd
            null;
         when others =>
            Pldmfw_Response_Error_Code (Context => Context, CC => PLDM_CC_ERR_UNSUPPORTED_CMD);
            Ret := Pldmfw_Ret_Unsupport_Cmd;
      end case;

      <<Exit_Point>>
      Pldm_Hook_Set_State_Cmd_Debug
        (State => PLDMFW_STATE_IDLE, Cmd => Hdr.Command_Code, Ret => Ret);

   end Pldmfw_State_Idle_Handler;

   procedure Pldmfw_State_Lc_Handler
     (Context : in out Pldm_Context;
      Size    :        NvU32)
   is
      Ret : Pldmfw_Ret := Pldmfw_Ret_Success;
      Hdr : Pldm_Header;
   begin

      Hdr :=
        Uc_Array_To_Pldm_Header
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Pldm_Header'Size) - 1)));

      if Hdr.Command_Code = PLDMFW_COMMAND_REQUEST_UPDATE
      then
         Pldmfw_Response_Non_Idempotent_Cmd
           (Ret => Ret, Context => Context, Iid => Hdr.Iid, Cmd => Hdr.Command_Code);
         if Ret /= Pldmfw_Ret_Success
         then
            goto Exit_Point;
         end if;
      end if;

      case Hdr.Command_Code is
         when PLDMFW_COMMAND_QUERY_DEVICE_ID =>
            Pldmfw_Cmd_Query_Device_Id (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_GET_FW_PARAMETERS =>
            Pldmfw_Cmd_Get_Fw_Parameters (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_PASS_COMPONENT_TABLE =>
            Pldmfw_Cmd_Pass_Component_Table (Ret => Ret, Context => Context, Size => Size);
         when PLDMFW_COMMAND_GET_STATUS =>
            Pldmfw_Cmd_Get_Status (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_CANCEL_UPDATE =>
            Pldmfw_Cmd_Cancel_Update (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_REQUEST_UPDATE | PLDMFW_COMMAND_UPDATE_COMPONENT
           | PLDMFW_COMMAND_ACTIVATE_FW | PLDMFW_COMMAND_CANCEL_UPDATE_COMPONENT =>
            Pldmfw_Response_Error_Code
              (Context => Context, CC => PLDMFW_CC_INVALID_STATE_FOR_CMD);
            Ret := Pldmfw_Ret_Unsupport_Cmd;
         when PLDMFW_COMMAND_REQUEST_FW_DATA | PLDMFW_COMMAND_TRANSFER_COMPLETE
           | PLDMFW_COMMAND_VERIFY_COMPLETE | PLDMFW_COMMAND_APPLY_COMPLETE =>
            --  do nothing for response cmd
            null;
         when others =>
            Pldmfw_Response_Error_Code (Context => Context, CC => PLDM_CC_ERR_UNSUPPORTED_CMD);
            Ret := Pldmfw_Ret_Unsupport_Cmd;
      end case;

      <<Exit_Point>>
      Pldm_Hook_Set_State_Cmd_Debug
        (State => PLDMFW_STATE_LC, Cmd => Hdr.Command_Code, Ret => Ret);

   end Pldmfw_State_Lc_Handler;

   procedure Pldmfw_State_Rdy_Xfer_Handler
     (Context : in out Pldm_Context;
      Size    :        NvU32)
   is
      Ret : Pldmfw_Ret := Pldmfw_Ret_Success;
      Hdr : Pldm_Header;
   begin

      Hdr :=
        Uc_Array_To_Pldm_Header
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Pldm_Header'Size) - 1)));

      if Hdr.Command_Code = PLDMFW_COMMAND_PASS_COMPONENT_TABLE
      then
         Pldmfw_Response_Non_Idempotent_Cmd
           (Ret => Ret, Context => Context, Iid => Hdr.Iid, Cmd => Hdr.Command_Code);
         if Ret /= Pldmfw_Ret_Success
         then
            goto Exit_Point;
         end if;
      end if;

      case Hdr.Command_Code is
         when PLDMFW_COMMAND_QUERY_DEVICE_ID =>
            Pldmfw_Cmd_Query_Device_Id (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_GET_FW_PARAMETERS =>
            Pldmfw_Cmd_Get_Fw_Parameters (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_UPDATE_COMPONENT =>
            Pldmfw_Cmd_Update_Component (Ret => Ret, Context => Context, Size => Size);
            if Ret = Pldmfw_Ret_Success
            then
               Pldmfw_Change_State
                 (Ret => Ret, Context => Context, New_State => PLDMFW_STATE_DL);
               if Ret = Pldmfw_Ret_Success
               then
                  Context.Req_Retry := 0;
                  Pldmfw_Send_Request_Fw_Data (Ret => Ret, Context => Context);
               end if;
            end if;
         when PLDMFW_COMMAND_ACTIVATE_FW =>
            Pldmfw_Cmd_Activate_Fw (Ret => Ret, Context => Context, Size => Size);
            if Ret = Pldmfw_Ret_Success
            then
               --  without self contained directly move to idle mode
               Pldmfw_Change_State
                 (Ret => Ret, Context => Context, New_State => PLDMFW_STATE_ACTIVATE);
               if Ret = Pldmfw_Ret_Success
               then
                  Pldmfw_Change_State
                    (Ret => Ret, Context => Context, New_State => PLDMFW_STATE_IDLE);
                  Context.Idle_Reason_Code := PLDMFW_CMD_GET_STATUS_REASON_ACTIVE_FW;
               end if;
            end if;
         when PLDMFW_COMMAND_GET_STATUS =>
            Pldmfw_Cmd_Get_Status (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_CANCEL_UPDATE =>
            Pldmfw_Cmd_Cancel_Update (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_REQUEST_UPDATE =>
            Pldmfw_Response_Error_Code
              (Context => Context, CC => PLDMFW_CC_ALREADY_IN_UPDATE_MODE);
            Ret := Pldmfw_Ret_Unsupport_Cmd;
         when PLDMFW_COMMAND_PASS_COMPONENT_TABLE | PLDMFW_COMMAND_CANCEL_UPDATE_COMPONENT =>
            Pldmfw_Response_Error_Code
              (Context => Context, CC => PLDMFW_CC_INVALID_STATE_FOR_CMD);
            Ret := Pldmfw_Ret_Unsupport_Cmd;
         when PLDMFW_COMMAND_REQUEST_FW_DATA | PLDMFW_COMMAND_TRANSFER_COMPLETE
           | PLDMFW_COMMAND_VERIFY_COMPLETE | PLDMFW_COMMAND_APPLY_COMPLETE =>
            --  do nothing for response cmd
            null;
         when others =>
            Pldmfw_Response_Error_Code (Context => Context, CC => PLDM_CC_ERR_UNSUPPORTED_CMD);
            Ret := Pldmfw_Ret_Unsupport_Cmd;
      end case;

      <<Exit_Point>>
      Pldm_Hook_Set_State_Cmd_Debug
        (State => PLDMFW_STATE_RDY_XFER, Cmd => Hdr.Command_Code, Ret => Ret);

   end Pldmfw_State_Rdy_Xfer_Handler;

   procedure Pldmfw_State_Dl_Handler
     (Context : in out Pldm_Context;
      Size    :        NvU32)
   is
      Ret : Pldmfw_Ret := Pldmfw_Ret_Success;
      Hdr : Pldm_Header;
   begin

      Hdr :=
        Uc_Array_To_Pldm_Header
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Pldm_Header'Size) - 1)));

      if Hdr.Command_Code = PLDMFW_COMMAND_UPDATE_COMPONENT
      then
         Pldmfw_Response_Non_Idempotent_Cmd
           (Ret => Ret, Context => Context, Iid => Hdr.Iid, Cmd => Hdr.Command_Code);
         if Ret /= Pldmfw_Ret_Success
         then
            goto Exit_Point;
         end if;
      end if;

      case Hdr.Command_Code is
         when PLDMFW_COMMAND_QUERY_DEVICE_ID =>
            Pldmfw_Cmd_Query_Device_Id (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_GET_FW_PARAMETERS =>
            Pldmfw_Cmd_Get_Fw_Parameters (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_REQUEST_FW_DATA =>
            Pldmfw_Cmd_Request_Fw_Data (Ret => Ret, Context => Context, Size => Size);
         when PLDMFW_COMMAND_TRANSFER_COMPLETE =>
            Pldmfw_Cmd_Transfer_Complete (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_GET_STATUS =>
            Pldmfw_Cmd_Get_Status (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_CANCEL_UPDATE_COMPONENT =>
            Pldmfw_Cmd_Cancel_Update_Component (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_CANCEL_UPDATE =>
            Pldmfw_Cmd_Cancel_Update (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_REQUEST_UPDATE | PLDMFW_COMMAND_PASS_COMPONENT_TABLE
           | PLDMFW_COMMAND_UPDATE_COMPONENT | PLDMFW_COMMAND_ACTIVATE_FW =>
            Pldmfw_Response_Error_Code
              (Context => Context, CC => PLDMFW_CC_INVALID_STATE_FOR_CMD);
            Ret := Pldmfw_Ret_Unsupport_Cmd;
         when PLDMFW_COMMAND_VERIFY_COMPLETE | PLDMFW_COMMAND_APPLY_COMPLETE =>
            --  do nothing for response cmd
            null;
         when others =>
            Pldmfw_Response_Error_Code (Context => Context, CC => PLDM_CC_ERR_UNSUPPORTED_CMD);
            Ret := Pldmfw_Ret_Unsupport_Cmd;
      end case;

      <<Exit_Point>>
      Pldm_Hook_Set_State_Cmd_Debug
        (State => PLDMFW_STATE_DL, Cmd => Hdr.Command_Code, Ret => Ret);

   end Pldmfw_State_Dl_Handler;

   procedure Pldmfw_State_Verify_Handler (Context : in out Pldm_Context) is
      Ret : Pldmfw_Ret := Pldmfw_Ret_Success;
      Hdr : Pldm_Header;
   begin

      Hdr :=
        Uc_Array_To_Pldm_Header
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Pldm_Header'Size) - 1)));

      case Hdr.Command_Code is
         when PLDMFW_COMMAND_QUERY_DEVICE_ID =>
            Pldmfw_Cmd_Query_Device_Id (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_GET_FW_PARAMETERS =>
            Pldmfw_Cmd_Get_Fw_Parameters (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_VERIFY_COMPLETE =>
            Pldmfw_Cmd_Verify_Complete (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_GET_STATUS =>
            Pldmfw_Cmd_Get_Status (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_CANCEL_UPDATE_COMPONENT =>
            Pldmfw_Cmd_Cancel_Update_Component (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_CANCEL_UPDATE =>
            Pldmfw_Cmd_Cancel_Update (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_REQUEST_UPDATE | PLDMFW_COMMAND_PASS_COMPONENT_TABLE
           | PLDMFW_COMMAND_UPDATE_COMPONENT | PLDMFW_COMMAND_ACTIVATE_FW =>
            Pldmfw_Response_Error_Code
              (Context => Context, CC => PLDMFW_CC_INVALID_STATE_FOR_CMD);
            Ret := Pldmfw_Ret_Unsupport_Cmd;
         when PLDMFW_COMMAND_REQUEST_FW_DATA | PLDMFW_COMMAND_TRANSFER_COMPLETE
           | PLDMFW_COMMAND_APPLY_COMPLETE =>
            --  do nothing for response cmd
            null;
         when others =>
            Pldmfw_Response_Error_Code (Context => Context, CC => PLDM_CC_ERR_UNSUPPORTED_CMD);
            Ret := Pldmfw_Ret_Unsupport_Cmd;
      end case;

      Pldm_Hook_Set_State_Cmd_Debug
        (State => PLDMFW_STATE_VERIFY, Cmd => Hdr.Command_Code, Ret => Ret);

   end Pldmfw_State_Verify_Handler;

   procedure Pldmfw_State_Apply_Handler (Context : in out Pldm_Context) is
      Ret : Pldmfw_Ret := Pldmfw_Ret_Success;
      Hdr : Pldm_Header;
   begin

      Hdr :=
        Uc_Array_To_Pldm_Header
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Pldm_Header'Size) - 1)));

      case Hdr.Command_Code is
         when PLDMFW_COMMAND_QUERY_DEVICE_ID =>
            Pldmfw_Cmd_Query_Device_Id (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_GET_FW_PARAMETERS =>
            Pldmfw_Cmd_Get_Fw_Parameters (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_APPLY_COMPLETE =>
            Pldmfw_Cmd_Apply_Complete (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_GET_STATUS =>
            Pldmfw_Cmd_Get_Status (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_CANCEL_UPDATE_COMPONENT =>
            Pldmfw_Cmd_Cancel_Update_Component (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_CANCEL_UPDATE =>
            Pldmfw_Cmd_Cancel_Update (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_REQUEST_UPDATE | PLDMFW_COMMAND_PASS_COMPONENT_TABLE
           | PLDMFW_COMMAND_UPDATE_COMPONENT | PLDMFW_COMMAND_ACTIVATE_FW =>
            Pldmfw_Response_Error_Code
              (Context => Context, CC => PLDMFW_CC_INVALID_STATE_FOR_CMD);
            Ret := Pldmfw_Ret_Unsupport_Cmd;
         when PLDMFW_COMMAND_REQUEST_FW_DATA | PLDMFW_COMMAND_TRANSFER_COMPLETE
           | PLDMFW_COMMAND_VERIFY_COMPLETE =>
            --  do nothing for response cmd
            null;
         when others =>
            Pldmfw_Response_Error_Code (Context => Context, CC => PLDM_CC_ERR_UNSUPPORTED_CMD);
            Ret := Pldmfw_Ret_Unsupport_Cmd;
      end case;

      Pldm_Hook_Set_State_Cmd_Debug
        (State => PLDMFW_STATE_APPLY, Cmd => Hdr.Command_Code, Ret => Ret);

   end Pldmfw_State_Apply_Handler;

   procedure Pldmfw_State_Activate_Handler (Context : in out Pldm_Context) is
      Ret : Pldmfw_Ret := Pldmfw_Ret_Success;
      Hdr : Pldm_Header;
   begin

      Hdr :=
        Uc_Array_To_Pldm_Header
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Pldm_Header'Size) - 1)));

      case Hdr.Command_Code is
         when PLDMFW_COMMAND_QUERY_DEVICE_ID =>
            Pldmfw_Cmd_Query_Device_Id (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_GET_FW_PARAMETERS =>
            Pldmfw_Cmd_Get_Fw_Parameters (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_GET_STATUS =>
            Pldmfw_Cmd_Get_Status (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_CANCEL_UPDATE =>
            Pldmfw_Cmd_Cancel_Update (Ret => Ret, Context => Context);
         when PLDMFW_COMMAND_REQUEST_FW_DATA | PLDMFW_COMMAND_TRANSFER_COMPLETE
           | PLDMFW_COMMAND_VERIFY_COMPLETE | PLDMFW_COMMAND_APPLY_COMPLETE =>
            --  do nothing for response cmd
            null;
         when others =>
            Pldmfw_Response_Error_Code (Context => Context, CC => PLDM_CC_ERR_UNSUPPORTED_CMD);
            Ret := Pldmfw_Ret_Unsupport_Cmd;
      end case;

      Pldm_Hook_Set_State_Cmd_Debug
        (State => PLDMFW_STATE_ACTIVATE, Cmd => Hdr.Command_Code, Ret => Ret);

   end Pldmfw_State_Activate_Handler;

   procedure Pldmfw_State_Runner
     (Context : in out Pldm_Context;
      Size    :        NvU32)
   is
   begin

      case Context.Pldm_State is
         when PLDMFW_STATE_IDLE =>
            Pldmfw_State_Idle_Handler (Context => Context, Size => Size);
         when PLDMFW_STATE_LC =>
            Pldmfw_State_Lc_Handler (Context => Context, Size => Size);
         when PLDMFW_STATE_RDY_XFER =>
            Pldmfw_State_Rdy_Xfer_Handler (Context => Context, Size => Size);
         when PLDMFW_STATE_DL =>
            Pldmfw_State_Dl_Handler (Context => Context, Size => Size);
         when PLDMFW_STATE_VERIFY =>
            Pldmfw_State_Verify_Handler (Context => Context);
         when PLDMFW_STATE_APPLY =>
            Pldmfw_State_Apply_Handler (Context => Context);
         when PLDMFW_STATE_ACTIVATE =>
            Pldmfw_State_Activate_Handler (Context => Context);
      end case;

   end Pldmfw_State_Runner;

   procedure Pldmfw_Timeout_Handler (Context : in out Pldm_Context) is
      Ret : Pldmfw_Ret;
   begin
      case Context.Pldm_State is
         when PLDMFW_STATE_IDLE | PLDMFW_STATE_ACTIVATE =>
            Pldm_Hook_Clear_Timer (Ret);
         when PLDMFW_STATE_LC =>
            Context.Idle_Reason_Code := PLDMFW_CMD_GET_STATUS_REASON_TIMEOUT_LC;
            Pldm_Hook_Set_Log (Event => Timeout, Info0 => Context.Pldm_State);
            Pldmfw_Change_State
              (Ret => Ret, Context => Context, New_State => PLDMFW_STATE_IDLE);
         when PLDMFW_STATE_RDY_XFER =>
            Context.Idle_Reason_Code := PLDMFW_CMD_GET_STATUS_REASON_TIMEOUT_RDY_XFER;
            Pldm_Hook_Set_Log (Event => Timeout, Info0 => Context.Pldm_State);
            --  staging
            if Context.Verify_Comp_Size.Valid_Size /= 0
            then

               for i in 0 .. (Context.Verify_Comp_Size.Valid_Size - 1)
               loop

                  if NvU32 (i) < Context.Verify_Comp_Size.Comp_Arr'Length
                  then

                     Pldm_Hook_Handle_Stage_Update
                       (Component_Id => Context.Verify_Comp_Size.Comp_Arr (NvU32 (i)),
                        Ret          => Ret);
                  end if;

               end loop;

               --  clear verify complete component
               Context.Verify_Comp_Size.Valid_Size := 0;
            end if;

            Pldmfw_Change_State
              (Ret => Ret, Context => Context, New_State => PLDMFW_STATE_IDLE);
         --  requester timeout handler DL/Verify/Apply
         when PLDMFW_STATE_DL =>
            Add_U8 (Context.Req_Retry, 1, Context.Req_Retry);
            Add_U32 (Context.Total_Retry, 1, Context.Total_Retry);
            --  over retry and back to idle mode
            if Context.Req_Retry > PLDMFW_TIMEING_PN1
            then
               Context.Idle_Reason_Code := PLDMFW_CMD_GET_STATUS_REASON_TIMEOUT_DL;
               Pldm_Hook_Set_Log (Event => Timeout, Info0 => Context.Pldm_State);
               Pldmfw_Change_State
                 (Ret => Ret, Context => Context, New_State => PLDMFW_STATE_IDLE);
            --  end of retry wait for UA response
            elsif Context.Req_Retry = PLDMFW_TIMEING_PN1
            then
               Pldm_Hook_Set_Timer
                 (Ret,
                  (PLDMFW_FD_TIMEOUT_T1 - (PLDMFW_TIMEING_PN1 * PLDMFW_TIMEING_PT2)) *
                  MSEC_TO_NSEC);
            --  spi flash programing fail over retry times
            elsif Context.Spi_Flash_Retry > Context.Write_Fail_Retry
            then
               Context.Idle_Reason_Code := PLDMFW_CMD_GET_STATUS_REASON_TIMEOUT_DL;
               Context.Slot_State       := Pldmfw_Slot_State_Spiflash_Error;
               Pldmfw_Send_Transfer_Complete (Ret => Ret, Context => Context);
               if Ret /= Pldmfw_Ret_Success
               then
                  Pldm_Hook_Set_State_Cmd_Debug
                    (State => PLDMFW_STATE_DL, Cmd => PLDMFW_COMMAND_TRANSFER_COMPLETE,
                     Ret   => Ret);
               end if;
               Pldm_Hook_Set_Log (Event => Timeout, Info0 => Context.Pldm_State);
               Pldmfw_Change_State
                 (Ret => Ret, Context => Context, New_State => PLDMFW_STATE_IDLE);
            --  send retry request msg
            elsif Context.Slot_State /= Pldmfw_Slot_State_Init
            then
               Pldmfw_Send_Transfer_Complete (Ret => Ret, Context => Context);
            else
               Pldm_Hook_Set_Timer (Ret, PLDMFW_TIMEING_PT2 * MSEC_TO_NSEC);
               if Ret /= Pldmfw_Ret_Success
               then
                  Pldm_Hook_Set_State_Cmd_Debug
                    (State => PLDMFW_STATE_DL, Cmd => PLDMFW_COMMAND_INVALID, Ret => Ret);
               end if;
               Pldmfw_Send_Request_Fw_Data (Ret => Ret, Context => Context);
            end if;
         when PLDMFW_STATE_VERIFY =>
            Add_U8 (Context.Req_Retry, 1, Context.Req_Retry);
            --  over retry and back to idle mode
            if Context.Req_Retry > PLDMFW_TIMEING_PN1
            then
               Context.Idle_Reason_Code := PLDMFW_CMD_GET_STATUS_REASON_TIMEOUT_VERIFY;
               Pldm_Hook_Set_Log (Event => Timeout, Info0 => Context.Pldm_State);
               Pldmfw_Change_State
                 (Ret => Ret, Context => Context, New_State => PLDMFW_STATE_IDLE);
            --  end of retry wait for UA response
            elsif Context.Req_Retry = PLDMFW_TIMEING_PN1
            then
               Pldm_Hook_Set_Timer
                 (Ret,
                  (PLDMFW_FD_TIMEOUT_T1 - (PLDMFW_TIMEING_PN1 * PLDMFW_TIMEING_PT2)) *
                  MSEC_TO_NSEC);
            --  send retry request msg
            else
               Pldmfw_Send_Verify_Complete (Ret => Ret, Context => Context);
            end if;
         when PLDMFW_STATE_APPLY =>
            Add_U8 (Context.Req_Retry, 1, Context.Req_Retry);
            --  over retry and back to idle mode
            if Context.Req_Retry > PLDMFW_TIMEING_PN1
            then
               Context.Idle_Reason_Code := PLDMFW_CMD_GET_STATUS_REASON_TIMEOUT_APPLY;
               Pldm_Hook_Set_Log (Event => Timeout, Info0 => Context.Pldm_State);
               Pldmfw_Change_State
                 (Ret => Ret, Context => Context, New_State => PLDMFW_STATE_IDLE);
            --  end of retry wait for UA response
            elsif Context.Req_Retry = PLDMFW_TIMEING_PN1
            then
               Pldm_Hook_Set_Timer
                 (Ret,
                  (PLDMFW_FD_TIMEOUT_T1 - (PLDMFW_TIMEING_PN1 * PLDMFW_TIMEING_PT2)) *
                  MSEC_TO_NSEC);
            --  send retry request msg
            else
               Pldmfw_Send_Apply_Complete (Ret => Ret, Context => Context);
            end if;
      end case;

      Pldm_Hook_Set_State_Cmd_Debug (Context.Pldm_State, PLDMFW_COMMAND_INVALID, Ret);

   end Pldmfw_Timeout_Handler;

   procedure Pldmfw_Authenticate_Handler
     (Context   : in out Pldm_Context;
      Verify_Cc :        NvU8)
   is
      Ret : Pldmfw_Ret;
   begin
      if Context.Pldm_State /= PLDMFW_STATE_VERIFY
      then
         Ret := Pldmfw_Ret_Invalid_authenticate_event;
         goto Exit_Point;
      end if;

      Pldm_Hook_Set_Log (Event => Auth, Info0 => Verify_Cc);

      if Verify_Cc = PLDMFW_CMD_VERIFY_COMPLETE_RESULT_SUCCESS
      then
         Context.Slot_State := Pldmfw_Slot_State_Auth_Success;
      else
         Context.Slot_State     := Pldmfw_Slot_State_Auth_Fail;
         Context.Verify_Fail_CC := Verify_Cc;
      end if;

      if Context.Verify_Comp_Size.Valid_Size < MAX_PASS_COMPONENT_ID_NUM
      then
         Context.Verify_Comp_Size.Comp_Arr (NvU32 (Context.Verify_Comp_Size.Valid_Size)) :=
           Context.Comp_Id;
         Context.Verify_Comp_Size.Valid_Size := Context.Verify_Comp_Size.Valid_Size + 1;
      end if;

      Pldm_Hook_Set_Timer (Ret, PLDMFW_TIMEING_PT2 * MSEC_TO_NSEC);
      if Ret /= Pldmfw_Ret_Success
      then
         goto Exit_Point;
      end if;

      Pldmfw_Send_Verify_Complete (Ret => Ret, Context => Context);

      <<Exit_Point>>

      Pldm_Hook_Set_State_Cmd_Debug
        (State => Context.Pldm_State, Cmd => PLDMFW_COMMAND_INVALID, Ret => Ret);

   end Pldmfw_Authenticate_Handler;

end Pdk.Pldm.Fwupdate.State;

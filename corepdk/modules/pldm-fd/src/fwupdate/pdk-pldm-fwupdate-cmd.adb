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
with Nv_Types.Shift_Right_Op; use Nv_Types.Shift_Right_Op;
with Pdk.Pldm.Device.Cmd;     use Pdk.Pldm.Device.Cmd;
with Pdk.Pldm.Fwupdate.State; use Pdk.Pldm.Fwupdate.State;
with Pdk.Pldm.Hook.Plattypes; use Pdk.Pldm.Hook.Plattypes;
with Pdk.Pldm.Hook.Plat;      use Pdk.Pldm.Hook.Plat;

package body Pdk.Pldm.Fwupdate.Cmd with
  SPARK_Mode => On
is

   --  tool
   procedure Lower_Power_Of_2
     (x   :     NvU32;
      Ret : out NvU32)
   is
      Val : NvU32;
   begin
      Val := x;
      Val := Val or (Safe_Shift_Right (Val, 1));
      Val := Val or (Safe_Shift_Right (Val, 2));
      Val := Val or (Safe_Shift_Right (Val, 4));
      Val := Val or (Safe_Shift_Right (Val, 8));
      Val := Val or (Safe_Shift_Right (Val, 16));

      Minus_U32 (Val, Safe_Shift_Right (Val, 1), Ret);
   end Lower_Power_Of_2;

   procedure Add_U32
     (a :     NvU32;
      b :     NvU32;
      c : out NvU32)
   is
   begin
      if (a > 16#ffff_ffff# - b)
      then
         c := 16#ffff_ffff#;
      else
         c := (a + b);
      end if;
   end Add_U32;

   procedure Add_U8
     (a :     NvU8;
      b :     NvU8;
      c : out NvU8)
   is
   begin
      if (a > NvU8'Last - b)
      then
         c := NvU8'Last;
      else
         c := (a + b);
      end if;
   end Add_U8;

   procedure Minus_U32
     (a :     NvU32;
      b :     NvU32;
      c : out NvU32)
   is
   begin
      if b > a
      then
         c := 0;
      else
         c := (a - b);
      end if;

   end Minus_U32;

   procedure Minus_U8
     (a :     NvU8;
      b :     NvU8;
      c : out NvU8)
   is
   begin
      if b > a
      then
         c := 0;
      else
         c := (a - b);
      end if;
   end Minus_U8;

   procedure Le_U32
     (buffer :     Arr_U8_Idx_4;
      Ret    : out NvU32)
   is
      type U32_Rec is record
         V0 : NvU8;
         V1 : NvU8;
         V2 : NvU8;
         V3 : NvU8;
      end record;

      for U32_Rec use record
         V0 at 0 range  0 ..  7;
         V1 at 0 range  8 .. 15;
         V2 at 0 range 16 .. 23;
         V3 at 0 range 24 .. 31;
      end record;

      function Uc_U32_Rec_To_U32 is new Ada.Unchecked_Conversion
        (Source => U32_Rec, Target => NvU32);

      Rec : U32_Rec;
   begin
      Rec :=
        (V0 => buffer (0),
         V1 => buffer (1),
         V2 => buffer (2),
         V3 => buffer (3));
      Ret := Uc_U32_Rec_To_U32 (Rec);

   end Le_U32;

   procedure Pldmfw_Generate_Update_Report (Context : Pldm_Context) is
      Count   : NvU32 := 0;
      M0_Time : NvU32;
      M1_Time : NvU32;
   begin
      Count := Context.Fw_Update_Size / 4_096;
      if Count = 0
      then
         Count := 1;
      end if;

      M0_Time := Context.Dl_M0_Time / Count;
      M1_Time := Context.Dl_M1_Time / Count;

      Pldm_Hook_Log_Fw_Update_Report
        (Fw_Update_Offset => Context.Fw_Update_Size, Total_Retry => Context.Total_Retry,
         Idle_Reason_Code => NvU32 (Context.Idle_Reason_Code), M0_Time => M0_Time,
         M1_Time          => M1_Time);

   end Pldmfw_Generate_Update_Report;

   procedure Pldmfw_Is_Rate_Limit_Reached
     (Context                 : in out Pldm_Context;
      Is_Limited_Rate_Reached :    out Boolean)
   is
      Info              : Limited_Rate_Info;
      Current_Timestamp : NvU32 := 0;
      Difference_Time   : NvU32 := 0;
      Next_Rate         : NvU8  := 0;
   begin

      Is_Limited_Rate_Reached := False;

      Pldm_Hook_Get_Limited_Rate_Info (Info => Info);

      Pldm_Hook_Get_Time_Milli_Seconds (Timestamp => Current_Timestamp);
      --  query timestamp fail disable rate limit function
      if Current_Timestamp = 0
      then
         goto Exit_Point;
      end if;

      if Current_Timestamp >= Context.Rate_Limit_Timestamp
      then
         Difference_Time := Current_Timestamp - Context.Rate_Limit_Timestamp;
      else
         Difference_Time := 16#ffff_ffff# - (Context.Rate_Limit_Timestamp - Current_Timestamp);
      end if;

      if Difference_Time > Info.Time
      then
         --  Reach rate time slot, reset current rate
         Context.Current_Rate := 0;

         if Context.Is_Limited_Rate_Reached = True
         then
            --  If limited rate reached previously, set next rate to half of current rate
            Next_Rate := Context.Limited_Rate / 2;
            if Next_Rate > Info.Min
            then
               Context.Limited_Rate := Next_Rate;
            else
               Context.Limited_Rate := Info.Min;
            end if;

         end if;

      end if;

      --  If current rate is 0, set rate limit timestamp
      if Context.Current_Rate = 0
      then
         Context.Rate_Limit_Timestamp := Current_Timestamp;
      end if;

      if Context.Current_Rate >= Context.Limited_Rate
      then
         Context.Is_Limited_Rate_Reached := True;
      else
         Context.Is_Limited_Rate_Reached := False;
      end if;

      <<Exit_Point>>

      Is_Limited_Rate_Reached := Context.Is_Limited_Rate_Reached;

   end Pldmfw_Is_Rate_Limit_Reached;

   --  pldm init
   procedure Pldmfw_Context_Init (Pldm_Ctx : out Pldm_Context) is
      Info : Limited_Rate_Info;
   begin

      Pldm_Hook_Get_Limited_Rate_Info (Info => Info);

      Pldm_Ctx :=
        (Pldm_State              => PLDMFW_STATE_IDLE,
         Pldm_Previous_State     => PLDMFW_STATE_IDLE,
         Idle_Reason_Code        => PLDMFW_CMD_GET_STATUS_REASON_INIT_FW_DEVICE,
         Max_Transfer_Size       => 0,
         Comp_Id                 => 0,
         Fw_Size                 => 0,
         Fw_Current_Offset       => 0,
         Fw_Update_Size          => 0,
         Last_Transfer_Size      => 0,
         Wr_Spi_Addr             => 0,
         Is_Force_Update         => False,
         Iid                     =>
           (Iid => 0,
            Rsv => 0),
         Composer_Buffer         =>
           [others => 0],
         Slot_State              => Pldmfw_Slot_State_Init,
         Iid_Cmd                 =>
           (Iid  =>
              (Iid => 0,
               Rsv => 0),
            Cmd  => PLDMFW_COMMAND_QUERY_DEVICE_ID,
            Len  => 0,
            Resp =>
              (Data =>
                 [others => 0])),
         Pass_Comp_Size          =>
           (Valid_Size => 0,
            Comp_Arr   =>
              [others => 0]),
         Verify_Comp_Size        =>
           (Valid_Size => 0,
            Comp_Arr   =>
              [others => 0]),
         Activate_Comp_Size      =>
           (Valid_Size => 0,
            Comp_Arr   =>
              [others => 0]),
         Req_Fw_Data_List        =>
           (Idx   => 0,
            Pairs =>
              [others =>
                (Iid    =>
                   (Iid => 0,
                    Rsv => 0),
                 Offset => 0)]),
         Req_Retry               => 0,
         Spi_Flash_Retry         => 0,
         Total_Retry             => 0,
         Is_Inforom_Ro_Activate  => False,
         Rx_Msg                  =>
           (Data =>
              [others => 0]),
         Rx_Interface            => DEFAULT_PLDMFW_INTERFACE_INFO,
         Update_Interface        => DEFAULT_PLDMFW_INTERFACE_INFO,
         Is_Inband_Enable        => IB_FW_UPDATE_FLAG_INB_ALLOWED,
         Is_Backup_In_Progress   => False,
         Pldm_Settings           =>
           (Is_Inband_Supported => 0,
            Rsv                 => 0),
         Dl_Send_Time            => 0,
         Dl_M0_Time              => 0,
         Dl_M1_Time              => 0,
         Transfer_Fail_CC        => 0,
         Verify_Fail_CC          => 0,
         Current_Rate            => 0,
         Limited_Rate            => Info.Max,
         Is_Limited_Rate_Reached => False,
         Rate_Limit_Timestamp    => 0,
         Is_Recv_All_Metadata    => False,
         Is_Metadata_Check_Done  => False,
         Fw_Data_Buffer          =>
           (Data     =>
              [others => 0],
            Cur_Size => 0),
         Metadata                =>
           (Data =>
              [others => 0]),
         Auth_Request_Id         => 0,
         Write_Fail_Retry        => 0);

   end Pldmfw_Context_Init;

   procedure Pldmfw_Increment_Iid
     (Context : in out Pldm_Context;
      Iid     :    out NvU5)
   is
   begin
      if Context.Iid.Iid = 31
      then
         Context.Iid.Iid := 0;
      else
         Context.Iid.Iid := Context.Iid.Iid + 1;
      end if;

      Iid := Context.Iid.Iid;

   end Pldmfw_Increment_Iid;

   procedure Pldmfw_Fill_Req_Header
     (Context : in out Pldm_Context;
      Cmd     :        NvU8;
      Hdr     :    out Pldm_Header)
   is
      Iid : NvU5;
   begin

      Pldmfw_Increment_Iid (Context => Context, Iid => Iid);

      Hdr.Iid          := Iid;
      Hdr.Rsv          := 0;
      Hdr.D            := 0;
      Hdr.Rq           := 1;
      Hdr.Command_Type := PLDM_TYPE_FIRMWARE_UPDATE;
      Hdr.Command_Code := Cmd;

   end Pldmfw_Fill_Req_Header;

   procedure Pldmfw_Response_Error_Code
     (Context : Pldm_Context;
      CC      : NvU8)
   is
      Resp : Pldm_Base_Resp;
      procedure Send_Buffer is new Pldm_Hook_Send_Buffer (Payload_Type => Pldm_Base_Resp);
   begin
      Resp.Pldm_Hdr    :=
        Uc_Array_To_Pldm_Header
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Pldm_Header'Size) - 1)));
      Resp.Pldm_Hdr.Rq := 0;
      Resp.CC          := CC;

      Send_Buffer
        (Data => Resp, Rx_Interface => Context.Rx_Interface,
         Size => Get_Size_Bytes (Resp'Size));

   end Pldmfw_Response_Error_Code;

   procedure Pldmfw_Set_Non_Idempotent_Response
     (Context : in out Pldm_Context;
      Hdr     :        Pldm_Header;
      Resp    :        Payload_Type)
   is
      Buffer : Arr_U8_Idx_16;
      subtype Payload_As_Arr_Type is ARR_NvU8_IDX32 (0 .. (Payload_Type'Size / NvU8'Size - 1));
      function Uc_Resp_To_Array is new Ada.Unchecked_Conversion
        (Source => Payload_Type, Target => Payload_As_Arr_Type);
      function Uc_Array_To_Iid_Resp is new Ada.Unchecked_Conversion
        (Source => Arr_U8_Idx_16, Target => Non_Idempotent_Resp);
   begin
      if Payload_Type'Size / NvU8'Size > Get_Size_Bytes (Context.Iid_Cmd.Resp'Size) or
        Payload_Type'Size / NvU8'Size < Get_Size_Bytes (Pldm_Header'Size)
      then
         --  over or invalid size do nothing
         goto Exit_Point;
      end if;

      if not
        ((Hdr.Command_Code = PLDMFW_COMMAND_REQUEST_UPDATE) or
         (Hdr.Command_Code = PLDMFW_COMMAND_PASS_COMPONENT_TABLE) or
         (Hdr.Command_Code = PLDMFW_COMMAND_UPDATE_COMPONENT) or
         (Hdr.Command_Code = PLDMFW_COMMAND_ACTIVATE_FW))
      then
         --  not support cmd do nothing
         goto Exit_Point;
      end if;

      Context.Iid_Cmd.Iid.Iid                           := Hdr.Iid;
      Context.Iid_Cmd.Cmd                               := Hdr.Command_Code;
      Buffer                                            := [others => 0];
      Buffer (0 .. (Payload_Type'Size / NvU8'Size - 1)) := Uc_Resp_To_Array (Resp);
      Context.Iid_Cmd.Resp                              := Uc_Array_To_Iid_Resp (Buffer);
      Context.Iid_Cmd.Len                               := Resp'Size;

      <<Exit_Point>>

   end Pldmfw_Set_Non_Idempotent_Response;

   procedure Pldmfw_Response_Non_Idempotent_Cmd
     (Ret     : out Pldmfw_Ret;
      Context :     Pldm_Context;
      Iid     :     NvU5;
      Cmd     :     NvU8)
   is
      procedure Send_Buffer is new Pldm_Hook_Send_Buffer (Payload_Type => Non_Idempotent_Resp);

   begin
      Ret := Pldmfw_Ret_Success;
      if Context.Iid_Cmd.Iid.Iid /= Iid or Context.Iid_Cmd.Cmd /= Cmd
      then
         --  different iid or cmd do nothing
         goto Exit_Point;
      end if;

      Ret := Pldmfw_Ret_Send_Non_Idempotent_Resp;

      Send_Buffer
        (Data => Context.Iid_Cmd.Resp, Rx_Interface => Context.Rx_Interface,
         Size => Get_Size_Bytes (Context.Iid_Cmd.Len));

      <<Exit_Point>>

   end Pldmfw_Response_Non_Idempotent_Cmd;

   --  pldm req/resp msg

   procedure Pldmfw_Cmd_Query_Device_Id
     (Ret     : out Pldmfw_Ret;
      Context :     Pldm_Context)
   is
      Pldm_Hdr : Pldm_Header                   := DEFAULT_PLDM_HEADER;
      Tx_Msg   : Arr_Pldm_Tx_Msg_Buffer_Record :=
        (Data =>
           [others => 0]);
      Offset   : NvU32                         := 0;

      procedure Send_Buffer is new Pldm_Hook_Send_Buffer
        (Payload_Type => Arr_Pldm_Tx_Msg_Buffer_Record);
   begin

      Ret := Pldmfw_Ret_Success;

      Pldm_Hdr    :=
        Uc_Array_To_Pldm_Header
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Pldm_Header'Size) - 1)));
      Pldm_Hdr.Rq := 0;

      Pldm_Fill_Device_Identity_Resp (Pldm_Hdr => Pldm_Hdr, Tx_Msg => Tx_Msg, Offset => Offset);

      Send_Buffer (Data => Tx_Msg, Rx_Interface => Context.Rx_Interface, Size => Offset);

   end Pldmfw_Cmd_Query_Device_Id;

   procedure Pldmfw_Cmd_Get_Fw_Parameters
     (Ret     : out Pldmfw_Ret;
      Context :     Pldm_Context)
   is
      Resp             : Pldmfw_Resp_Get_Fw_Parameters;
      Offset           : NvU32                         := 0;
      Comp             : Pldmfw_Component_Parameter_Table;
      Tx_Msg           : Arr_Pldm_Tx_Msg_Buffer_Record :=
        (Data =>
           [others => 0]);
      Component_Infos  : Component_Info_List;
      Component_Length : Component_Count;
      Is_Activate_Fw   : Boolean                       := False;

      subtype Arr_Resp_Buffer is
        ARR_NvU8_IDX32 (0 .. (Pldmfw_Resp_Get_Fw_Parameters'Size / NvU8'Size - 1)) with
          Object_Size => 112;
      subtype Arr_Comp_Buffer is
        ARR_NvU8_IDX32 (0 .. (Pldmfw_Component_Parameter_Table'Size / NvU8'Size - 1)) with
          Object_Size => 312;
      function Uc_Resp_To_Array is new Ada.Unchecked_Conversion
        (Source => Pldmfw_Resp_Get_Fw_Parameters, Target => Arr_Resp_Buffer);
      function Uc_Comp_To_Array is new Ada.Unchecked_Conversion
        (Source => Pldmfw_Component_Parameter_Table, Target => Arr_Comp_Buffer);

      procedure Send_Buffer is new Pldm_Hook_Send_Buffer
        (Payload_Type => Arr_Pldm_Tx_Msg_Buffer_Record);

   begin

      Ret := Pldmfw_Ret_Success;

      Resp.Pldm_Hdr    :=
        Uc_Array_To_Pldm_Header
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Pldm_Header'Size) - 1)));
      Resp.Pldm_Hdr.Rq := 0;
      Resp.CC          := PLDM_CC_SUCCESS;

      Pldm_Hook_Get_Component_Info_List
        (Component_Infos => Component_Infos, Component_Length => Component_Length);

      Resp.Capabilities_During_Update := 0;
      Resp.Component_Count            := NvU16 (Component_Length);
      Resp.Active_Comp_Str_Type       := FW_STR_TYPE_ASCII;
      Resp.Active_Comp_Str_Len        := Component_Infos (0).Version_String_Length;

      if Context.Activate_Comp_Size.Valid_Size /= 0
      then
         for i in 0 .. (Context.Activate_Comp_Size.Valid_Size - 1)
         loop

            if i < Context.Activate_Comp_Size.Comp_Arr'Length
            then
               if Context.Activate_Comp_Size.Comp_Arr (NvU32 (i)) = Component_Infos (0).Id
               then
                  Is_Activate_Fw := True;
                  exit;
               end if;
            end if;

         end loop;
      end if;

      if Is_Activate_Fw
      then
         Resp.Pending_Comp_Str_Type                                        := FW_STR_TYPE_ASCII;
         Resp.Pending_Comp_Str_Len := Component_Infos (0).Pending_Version_String_Length;
         --  Resp
         Tx_Msg.Data (Offset .. (Offset + Get_Size_Bytes (Resp'Size) - 1)) :=
           Uc_Resp_To_Array (Resp);
         Offset := Offset + Get_Size_Bytes (Resp'Size);
         --  Active ver
         if Component_Infos (0).Version_String_Length /= 0
         then
            Tx_Msg.Data
              (Offset .. (Offset + NvU32 (Component_Infos (0).Version_String_Length - 1))) :=
              Component_Infos (0).Version_String
                (0 .. NvU32 (Component_Infos (0).Version_String_Length - 1));
            Offset := Offset + NvU32 (Component_Infos (0).Version_String_Length);
         end if;
         --  Pend ver
         if Component_Infos (0).Pending_Version_String_Length /= 0
         then
            Tx_Msg.Data
              (Offset ..
                   (Offset + NvU32 (Component_Infos (0).Pending_Version_String_Length - 1))) :=
              Component_Infos (0).Pending_Version_String
                (0 .. NvU32 (Component_Infos (0).Pending_Version_String_Length - 1));
            Offset := Offset + NvU32 (Component_Infos (0).Pending_Version_String_Length);
         end if;
      else
         Resp.Pending_Comp_Str_Type := FW_STR_TYPE_UNKNOWN;
         Resp.Pending_Comp_Str_Len                                         := 0;
         --  Resp
         Tx_Msg.Data (Offset .. (Offset + Get_Size_Bytes (Resp'Size) - 1)) :=
           Uc_Resp_To_Array (Resp);
         Offset := Offset + Get_Size_Bytes (Resp'Size);
         --  Active ver
         if Component_Infos (0).Version_String_Length /= 0
         then
            Tx_Msg.Data
              (Offset .. (Offset + NvU32 (Component_Infos (0).Version_String_Length - 1))) :=
              Component_Infos (0).Version_String
                (0 .. NvU32 (Component_Infos (0).Version_String_Length - 1));
            Offset := Offset + NvU32 (Component_Infos (0).Version_String_Length);
         end if;
      end if;

      --  component
      for i in 0 .. (Component_Length - 1)
      loop

         Comp.Comp_Class                 := FIRMWARE_TYPE;
         Comp.Comp_Id                    := Component_Infos (i).Id;
         Comp.Comp_Class_Index           := 0;  --  only update 1 device
         Comp.Active_Comp_Stamp          := Component_Infos (i).Stamp;
         Comp.Active_Comp_Ver_Str_Type   := FW_STR_TYPE_ASCII;
         Comp.Active_Comp_Ver_Str_Len    := Component_Infos (i).Version_String_Length;
         Comp.Active_Comp_Relase_Date    := [others => 0];  --  do not have release date
         Comp.Pending_Comp_Stamp         := Component_Infos (i).Pending_Stamp;
         Comp.Pending_Comp_Relase_Date   := [others => 0];  --  do not have release date
         Comp.Comp_Active_Method         := Component_Infos (i).Active_Method;
         Comp.Capabilities_During_Update := 0;

         Is_Activate_Fw := False;
         if Context.Activate_Comp_Size.Valid_Size /= 0
         then
            for j in 0 .. (Context.Activate_Comp_Size.Valid_Size - 1)
            loop

               if j < Context.Activate_Comp_Size.Comp_Arr'Length
               then
                  if Context.Activate_Comp_Size.Comp_Arr (NvU32 (j)) = Component_Infos (i).Id
                  then
                     Is_Activate_Fw := True;
                     exit;
                  end if;
               end if;

            end loop;
         end if;

         if Is_Activate_Fw
         then
            Comp.Pending_Comp_Ver_Str_Type := FW_STR_TYPE_ASCII;
            Comp.Pending_Comp_Ver_Str_Len := Component_Infos (i).Pending_Version_String_Length;
            --  Comp
            Tx_Msg.Data (Offset .. (Offset + Get_Size_Bytes (Comp'Size) - 1)) :=
              Uc_Comp_To_Array (Comp);
            Offset := Offset + Get_Size_Bytes (Comp'Size);
            --  Active ver
            if Component_Infos (i).Version_String_Length /= 0
            then
               Tx_Msg.Data
                 (Offset .. (Offset + NvU32 (Component_Infos (i).Version_String_Length - 1))) :=
                 Component_Infos (i).Version_String
                   (0 .. NvU32 (Component_Infos (i).Version_String_Length - 1));
               Offset := Offset + NvU32 (Component_Infos (i).Version_String_Length);
            end if;
            --  Pending ver
            if Component_Infos (0).Pending_Version_String_Length /= 0
            then
               Tx_Msg.Data
                 (Offset ..
                      (Offset +
                       NvU32 (Component_Infos (i).Pending_Version_String_Length - 1))) :=
                 Component_Infos (i).Pending_Version_String
                   (0 .. NvU32 (Component_Infos (i).Pending_Version_String_Length - 1));
               Offset := Offset + NvU32 (Component_Infos (i).Pending_Version_String_Length);
            end if;
         else
            Comp.Pending_Comp_Stamp         := 0;
            Comp.Pending_Comp_Ver_Str_Type := FW_STR_TYPE_UNKNOWN;
            Comp.Pending_Comp_Ver_Str_Len                                     := 0;
            --  Comp
            Tx_Msg.Data (Offset .. (Offset + Get_Size_Bytes (Comp'Size) - 1)) :=
              Uc_Comp_To_Array (Comp);
            Offset := Offset + Get_Size_Bytes (Comp'Size);
            --  Active ver
            if Component_Infos (i).Version_String_Length /= 0
            then
               Tx_Msg.Data
                 (Offset .. (Offset + NvU32 (Component_Infos (i).Version_String_Length - 1))) :=
                 Component_Infos (i).Version_String
                   (0 .. NvU32 (Component_Infos (i).Version_String_Length - 1));
               Offset := Offset + NvU32 (Component_Infos (i).Version_String_Length);
            end if;
         end if;

      end loop;

      Send_Buffer (Data => Tx_Msg, Rx_Interface => Context.Rx_Interface, Size => Offset);

   end Pldmfw_Cmd_Get_Fw_Parameters;

   procedure Pldmfw_Cmd_Request_Update
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context;
      Size    :        NvU32)
   is
      Req : Pldmfw_Req_Request_Update  := DEFAULT_PLDM_REQ_REQUEST_UPDARTE;
      Resp : Pldmfw_Resp_Request_Update := DEFAULT_PLDM_RESP_REQUEST_UPDATE;
      PLDMFW_TRANSFER_SIZE_OFFSET : constant                   := 3;
      Is_Limited_Rate_Reached     : Boolean                    := False;

      procedure Send_Buffer is new Pldm_Hook_Send_Buffer
        (Payload_Type => Pldmfw_Resp_Request_Update);

      procedure Set_Non_Idempotent_Response is new Pldmfw_Set_Non_Idempotent_Response
        (Payload_Type => Pldmfw_Resp_Request_Update);
   begin
      Ret              := Pldmfw_Ret_Success;
      Resp.Pldm_Hdr    :=
        Uc_Array_To_Pldm_Header
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Pldm_Header'Size) - 1)));
      Resp.Pldm_Hdr.Rq := 0;

      if Size < Get_Size_Bytes (Pldmfw_Req_Request_Update'Size)
      then
         Resp.CC := PLDM_CC_ERR_INVALID_LENGTH;
         Ret     := Pldmfw_Ret_Invalid_Lens;
         goto Exit_Point;
      else
         Resp.CC := PLDM_CC_SUCCESS;
      end if;

      Req :=
        Uc_Array_To_Req_Request_Update
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Req'Size) - 1)));

      --  If In-band update, then check if allowed
      if Context.Pldm_Settings.Is_Inband_Supported = 1
      then
         if Context.Rx_Interface.Client = 0
           and then Context.Is_Inband_Enable = IB_FW_UPDATE_FLAG_INB_DISALLOWED
         then

            Resp.CC := PLDMFW_CC_INVALID_STATE_FOR_CMD;
            Ret     := Pldmfw_Ret_Ib_Update_Disabled;

            goto Exit_Point;

         end if;
      end if;

      if Context.Is_Backup_In_Progress
      then

         Resp.CC := PLDMFW_CC_UNABLE_TO_INIT_UPDATE;
         Ret     := Pldmfw_Ret_Background_Copy_In_Progress;

         goto Exit_Point;
      end if;

      Pldmfw_Is_Rate_Limit_Reached
        (Context => Context, Is_Limited_Rate_Reached => Is_Limited_Rate_Reached);

      if Is_Limited_Rate_Reached
      then
         Resp.CC := PLDMFW_CC_UNABLE_TO_INIT_UPDATE;
         Ret     := Pldmfw_Ret_Rate_Limit_Reached;

         goto Exit_Point;
      end if;

      --  keep the update interface ex: inband or outband
      Context.Update_Interface := Context.Rx_Interface;
      --  @todo pldm_hook rate limit protection

      Resp.Fw_Device_Metadata_Length    := 0;
      Resp.Fd_Will_Send_Get_Package_Cmd := 0;

      --  set max transfer size
      Le_U32
        (Context.Rx_Msg.Data (PLDMFW_TRANSFER_SIZE_OFFSET .. (PLDMFW_TRANSFER_SIZE_OFFSET + 3)),
         Context.Max_Transfer_Size);
      if Context.Max_Transfer_Size > NV_PLDM_MAX_TRANSFER_SIZE
      then
         Context.Max_Transfer_Size := NV_PLDM_MAX_TRANSFER_SIZE;
      elsif Context.Max_Transfer_Size < PLDMFW_BASE_TRANSFER_SIZE
      then
         Context.Max_Transfer_Size := PLDMFW_BASE_TRANSFER_SIZE;
      end if;
      Lower_Power_Of_2 (Context.Max_Transfer_Size, Context.Max_Transfer_Size);

      -- check for component numbers
      if Req.Num_Component = 0
      then
         Resp.CC := PLDM_CC_ERR_INVALID_DATA;
         Ret     := Pldmfw_Ret_Pass_Zero_Component;
         goto Exit_Point;
      end if;

      if Req.Num_Component > MAX_PASS_COMPONENT_ID_NUM
      then
         Resp.CC := PLDM_CC_ERR_INVALID_DATA;
         Ret     := Pldmfw_Ret_Pass_3More_Component;
         goto Exit_Point;
      end if;

      -- check for version string length = 0 but version string type is not unknown
      if Req.Comp_Version_String_Length = 0 and
        Req.Comp_Version_String_Type /= FW_STR_TYPE_UNKNOWN
      then
         Resp.CC := PLDM_CC_ERR_INVALID_DATA;
         Ret     := Pldmfw_Ret_Invalid_Ver_str_Len_and_Type;
         goto Exit_Point;
      end if;

      --  init pass comp size
      Context.Pass_Comp_Size.Valid_Size := 0;

      Set_Non_Idempotent_Response (Context => Context, Hdr => Resp.Pldm_Hdr, Resp => Resp);

      <<Exit_Point>>

      Send_Buffer
        (Data => Resp, Rx_Interface => Context.Rx_Interface,
         Size => Get_Size_Bytes (Resp'Size));

   end Pldmfw_Cmd_Request_Update;

   procedure Pldmfw_Cmd_Pass_Component_Table
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context;
      Size    :        NvU32)
   is
      Req              : Pldmfw_Req_Pass_Component_Table  := DEFAULT_PLDM_REQ_PASS_COMP_TABLE;
      Resp             : Pldmfw_Resp_Pass_Component_Table := DEFAULT_PLDM_RESP_PASS_COMP_TABLE;
      Component_Infos  : Component_Info_List;
      Component_Length : Component_Count;

      procedure Send_Buffer is new Pldm_Hook_Send_Buffer
        (Payload_Type => Pldmfw_Resp_Pass_Component_Table);

      procedure Set_Non_Idempotent_Response is new Pldmfw_Set_Non_Idempotent_Response
        (Payload_Type => Pldmfw_Resp_Pass_Component_Table);
   begin

      Resp.Pldm_Hdr    :=
        Uc_Array_To_Pldm_Header
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Pldm_Header'Size) - 1)));
      Resp.Pldm_Hdr.Rq := 0;

      Ret := Pldmfw_Ret_Success;

      if Context.Update_Interface.Src_Eid /= Context.Rx_Interface.Src_Eid
      then
         Resp.CC := PLDM_CC_ERROR;
         Ret     := Pldmfw_Ret_Recv_Update_Cmd_From_Diff_Interface;
         goto Exit_Point;
      end if;

      if Size < Get_Size_Bytes (Pldmfw_Req_Pass_Component_Table'Size)
      then
         Resp.CC := PLDM_CC_ERR_INVALID_LENGTH;
         Ret     := Pldmfw_Ret_Invalid_Lens;
         goto Exit_Point;
      else
         Resp.CC := PLDM_CC_SUCCESS;
      end if;

      Req :=
        Uc_Array_To_Req_Pass_Component_Table
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Req'Size) - 1)));

      Pldm_Hook_Get_Component_Info_List
        (Component_Infos => Component_Infos, Component_Length => Component_Length);

      --  init pending trigger comp
      Context.Verify_Comp_Size :=
        (Valid_Size => 0,
         Comp_Arr   =>
           [others => 0]);

      for i in 0 .. (Component_Length - 1)
      loop

         if Req.Id = Component_Infos (i).Id
         then

            --  multi comp support
            if Context.Pass_Comp_Size.Valid_Size < MAX_PASS_COMPONENT_ID_NUM
            then
               Context.Pass_Comp_Size.Comp_Arr (NvU32 (Context.Pass_Comp_Size.Valid_Size)) :=
                 Req.Id;
               Context.Pass_Comp_Size.Valid_Size := Context.Pass_Comp_Size.Valid_Size + 1;
            else
               Resp.CC := PLDM_CC_ERROR;
               --  @todo do we need more component?
               Ret     := Pldmfw_Ret_Pass_3More_Component;
               goto Exit_Point;
            end if;

            if Req.Comp_Stamp > Component_Infos (i).Stamp
            then
               Resp.Component_Resp      := PLDMFW_COMP_COMPATIBILITY_RESPONSE_CAN_BE_UPDATE;
               Resp.Component_Resp_Code :=
                 PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_CAN_BE_UPDATE;
            elsif Req.Comp_Stamp = Component_Infos (i).Stamp
            then
               Resp.Component_Resp      := PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE;
               Resp.Component_Resp_Code :=
                 PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_STAMP_IDENTICAL;
            else
               Resp.Component_Resp      := PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE;
               Resp.Component_Resp_Code := PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_STAMP_LOWER;
            end if;
            exit;
         end if;
         --  if didn't find the match component id, default resp has already set not able to update

      end loop;

      -- check comp classification and comp classification idx
      if Req.Class /= FIRMWARE_TYPE or Req.Class_Index /= 0
      then
         Resp.CC                  := PLDM_CC_ERR_INVALID_DATA;
         Resp.Component_Resp      := PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE;
         Resp.Component_Resp_Code := PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_SUPPORT;
         Ret                      := Pldmfw_Ret_Invalid_Class_or_Class_Idx;
         goto Exit_Point;
      end if;

      -- check version string len = 0 but version string type is not unknown
      if Req.Version_Str_Length = 0 and Req.Version_Str_Type /= FW_STR_TYPE_UNKNOWN
      then
         Resp.CC                  := PLDM_CC_ERR_INVALID_DATA;
         Resp.Component_Resp      := PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE;
         Resp.Component_Resp_Code := PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_SUPPORT;
         Ret                      := Pldmfw_Ret_Invalid_Ver_str_Len_and_Type;
         goto Exit_Point;
      end if;

      case Req.Transfer_Flag is
         when PLDMFW_CMD_PASS_COMP_TRANSFER_FLAG_START
           | PLDMFW_CMD_PASS_COMP_TRANSFER_FLAG_MID =>
            null; --  do nothing
         when PLDMFW_CMD_PASS_COMP_TRANSFER_FLAG_END
           | PLDMFW_CMD_PASS_COMP_TRANSFER_FLAG_START_END =>
            Pldmfw_Change_State
              (Ret => Ret, Context => Context, New_State => PLDMFW_STATE_RDY_XFER);
            if Ret /= Pldmfw_Ret_Success
            then
               Resp.CC := PLDM_CC_ERROR;
            end if;
         when others =>
            Resp.CC := PLDM_CC_ERR_INVALID_DATA;
      end case;

      Set_Non_Idempotent_Response (Context => Context, Hdr => Resp.Pldm_Hdr, Resp => Resp);

      <<Exit_Point>>

      Send_Buffer
        (Data => Resp, Rx_Interface => Context.Rx_Interface,
         Size => Get_Size_Bytes (Resp'Size));

   end Pldmfw_Cmd_Pass_Component_Table;

   procedure Pldmfw_Cmd_Update_Component
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context;
      Size    :        NvU32)
   is
      Req                      : Pldmfw_Req_Update_Component  := DEFAULT_PLDM_REQ_UPDATE_COMP;
      Resp                     : Pldmfw_Resp_Update_Component := DEFAULT_PLDM_RESP_UPDATE_COMP;
      is_match_pass_table      : Boolean                      := False;
      is_find_comp_table       : Boolean                      := False;
      Is_Staged                : Boolean;
      Pass_Comp_Arr_Valid_Size : NvU32;
      Component_Infos          : Component_Info_List;
      Component_Length         : Component_Count;
      Update_Component_Idx     : Component_Count              := 0;

      procedure Send_Buffer is new Pldm_Hook_Send_Buffer
        (Payload_Type => Pldmfw_Resp_Update_Component);

      procedure Set_Non_Idempotent_Response is new Pldmfw_Set_Non_Idempotent_Response
        (Payload_Type => Pldmfw_Resp_Update_Component);
   begin

      Resp.Pldm_Hdr    :=
        Uc_Array_To_Pldm_Header
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Pldm_Header'Size) - 1)));
      Resp.Pldm_Hdr.Rq := 0;

      Ret := Pldmfw_Ret_Success;

      if Context.Update_Interface.Src_Eid /= Context.Rx_Interface.Src_Eid
      then
         Resp.CC := PLDM_CC_ERROR;
         Ret     := Pldmfw_Ret_Recv_Update_Cmd_From_Diff_Interface;
         goto Exit_Point;
      end if;

      if Size < (Get_Size_Bytes (Pldmfw_Req_Update_Component'Size))
      then
         Resp.CC := PLDM_CC_ERR_INVALID_LENGTH;
         Ret     := Pldmfw_Ret_Invalid_Lens;
         goto Exit_Point;
      else
         Resp.CC := PLDM_CC_SUCCESS;
      end if;

      Req :=
        Uc_Array_To_Req_Update_Component
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Req'Size) - 1)));

      -- check comp classification and comp classification idx
      if Req.Class /= FIRMWARE_TYPE or Req.Class_Index /= 0
      then
         Resp.CC                  := PLDM_CC_ERR_INVALID_DATA;
         Resp.Component_Resp      := PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE;
         Resp.Component_Resp_Code :=
           PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_MATCH_PASS_COMP;
         Ret                      := Pldmfw_Ret_Invalid_Class_or_Class_Idx;
         goto Exit_Point;
      end if;
      -- check version string len = 0 but version string type is not unknown
      if Req.Version_Str_Length = 0 and Req.Version_Str_Type /= FW_STR_TYPE_UNKNOWN
      then
         Resp.CC                  := PLDM_CC_ERR_INVALID_DATA;
         Resp.Component_Resp      := PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE;
         Resp.Component_Resp_Code :=
           PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_MATCH_PASS_COMP;
         Ret                      := Pldmfw_Ret_Invalid_Ver_str_Len_and_Type;
         goto Exit_Point;
      end if;

      --  init for download and update
      Context.Comp_Id := Req.Id;
      Context.Fw_Size := Req.Comp_Size;
      if "and" (Req.Update_Options, 1) = 0
      then
         Context.Is_Force_Update := False;
      else
         Context.Is_Force_Update := True;
      end if;

      Pldm_Hook_Get_Component_Info_List
        (Component_Infos => Component_Infos, Component_Length => Component_Length);

      Pldm_Hook_Is_Staged (Component_Id => Context.Comp_Id, Is_Staged => Is_Staged);

      Context.Fw_Current_Offset      := 0;
      Context.Is_Recv_All_Metadata   := False;
      Context.Is_Metadata_Check_Done := False;
      Context.Fw_Update_Size         := 0;
      Context.Total_Retry            := 0;
      Context.Dl_M0_Time             := 0;
      Context.Dl_M1_Time             := 0;
      Context.Slot_State             := Pldmfw_Slot_State_Init;
      Context.Spi_Flash_Retry        := 0;
      Context.Write_Fail_Retry       := Pldm_Hook_Get_Write_Fail_Retry (Comp_Id => Context.Comp_Id);

      Resp.Component_Resp := PLDMFW_COMP_COMPATIBILITY_RESPONSE_CAN_BE_UPDATE;
      Resp.Component_Resp_Code := PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_CAN_BE_UPDATE;
      Resp.Update_Options_Enabled               := Req.Update_Options;
      Resp.Est_Time_Before_Send_Request_Fw_Data := 0;

      --  if match comp id from pass component
      if Context.Pass_Comp_Size.Valid_Size > MAX_PASS_COMPONENT_ID_NUM
      then
         Pass_Comp_Arr_Valid_Size := MAX_PASS_COMPONENT_ID_NUM;
      elsif Context.Pass_Comp_Size.Valid_Size = 0
      then  -- valid size incorrect
         Resp.Component_Resp      := PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE;
         Resp.Component_Resp_Code :=
           PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_MATCH_PASS_COMP;
         Ret                      := Pldmfw_Ret_Pass_Comp_Size_Invalid;
         goto Exit_Point;
      else
         Pass_Comp_Arr_Valid_Size := NvU32 (Context.Pass_Comp_Size.Valid_Size);
      end if;

      for i in 0 .. (Pass_Comp_Arr_Valid_Size - 1)
      loop
         if Context.Pass_Comp_Size.Comp_Arr (i) = Context.Comp_Id
         then
            is_match_pass_table := True;

            for j in 0 .. (Component_Length - 1)
            loop
               if Context.Comp_Id = Component_Infos (j).Id
               then
                  Update_Component_Idx := j;
                  is_find_comp_table   := True;
                  exit;
               end if;

            end loop;

         end if;

      end loop;

      if is_match_pass_table = False
      then
         Resp.Component_Resp      := PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE;
         Resp.Component_Resp_Code :=
           PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_MATCH_PASS_COMP;
         Ret                      := Pldmfw_Ret_Fail_Update_Component_Not_Match;
         goto Exit_Point;
      end if;

      if is_find_comp_table = False
      then
         --  should not reach
         Resp.Component_Resp      := PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE;
         Resp.Component_Resp_Code :=
           PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_MATCH_PASS_COMP;
         Ret                      := Pldmfw_Ret_Unsupport_Comp_Id;
         goto Exit_Point;
      end if;

      --  check if fw over size
      if Context.Fw_Size > Component_Infos (Update_Component_Idx).Fw_Size
      then
         Resp.CC                     := PLDM_CC_ERR_INVALID_DATA;
         Resp.Component_Resp         := PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE;
         Resp.Component_Resp_Code    :=
           PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_MATCH_PASS_COMP;
         Resp.Update_Options_Enabled := 0; --  disable update
         Ret                         := Pldmfw_Ret_Over_Size;
         goto Exit_Point;
      end if;

      --  init component update
      Context.Wr_Spi_Addr                   := Component_Infos (Update_Component_Idx).Fw_Offset;
      Context.Activate_Comp_Size.Valid_Size := 0;

      --  check stamp
      if Context.Is_Force_Update = False and then Is_Staged = False
      then
         if Component_Infos (Update_Component_Idx).Stamp = Req.Comp_Stamp
         then
            Resp.Component_Resp      := PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE;
            Resp.Component_Resp_Code := PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_STAMP_IDENTICAL;
            Ret                      := Pldmfw_Ret_Update_Comp_Version_Identical;
         elsif Component_Infos (Update_Component_Idx).Stamp > Req.Comp_Stamp
         then
            Resp.Component_Resp      := PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE;
            Resp.Component_Resp_Code := PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_STAMP_LOWER;
            Ret                      := Pldmfw_Ret_Update_Comp_Version_Lower;
         end if;
      end if;

      Set_Non_Idempotent_Response (Context => Context, Hdr => Resp.Pldm_Hdr, Resp => Resp);

      <<Exit_Point>>

      Send_Buffer
        (Data => Resp, Rx_Interface => Context.Rx_Interface,
         Size => Get_Size_Bytes (Resp'Size));

   end Pldmfw_Cmd_Update_Component;

   procedure Set_Iid_Offset_List
     (Context : in out Pldm_Context;
      Iid     :        NvU5;
      Offset  :        NvU32)
   is
   begin

      --  overflow protect
      if Context.Req_Fw_Data_List.Idx >= IID_PAIR_SIZE
      then
         Context.Req_Fw_Data_List.Idx := 0;
      end if;

      Context.Req_Fw_Data_List.Pairs (Context.Req_Fw_Data_List.Idx).Iid.Iid := Iid;
      Context.Req_Fw_Data_List.Pairs (Context.Req_Fw_Data_List.Idx).Offset  := Offset;

      Context.Req_Fw_Data_List.Idx := Context.Req_Fw_Data_List.Idx + 1;
      if Context.Req_Fw_Data_List.Idx >= IID_PAIR_SIZE
      then
         Context.Req_Fw_Data_List.Idx := 0;
      end if;

   end Set_Iid_Offset_List;

   procedure Pldmfw_Send_Request_Fw_Data
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context)
   is
      Req       : Pldmfw_Req_Request_Fw_Data;
      Remaining : NvU32;

      procedure Send_Buffer is new Pldm_Hook_Send_Buffer
        (Payload_Type => Pldmfw_Req_Request_Fw_Data);

   begin
      Minus_U32 (Context.Fw_Size, Context.Fw_Current_Offset, Remaining);
      Minus_U32 (Remaining, Context.Fw_Data_Buffer.Cur_Size, Remaining);

      --  @todo force to 4k transfer, since we don't have buffer to keep left bytes
      if Remaining > Context.Max_Transfer_Size
      then
         Context.Last_Transfer_Size := Context.Max_Transfer_Size;
      else
         Context.Last_Transfer_Size := Remaining;
      end if;

      --  DSO0267_1.0.1 - 11.6: UA is expected to pad with zeros beyond firmware size
      if Context.Last_Transfer_Size < PLDMFW_BASE_TRANSFER_SIZE
      then
         Context.Last_Transfer_Size := PLDMFW_BASE_TRANSFER_SIZE;
      end if;

      Pldmfw_Fill_Req_Header
        (Context => Context, Cmd => PLDMFW_COMMAND_REQUEST_FW_DATA, Hdr => Req.Pldm_Hdr);
      -- if < 256B update, need to consider the fw data store in the fw data buffer
      Add_U32
        (Context.Fw_Current_Offset, Context.Fw_Data_Buffer.Cur_Size,
         Req.Offset); -- over 256B don't need consider fw data buffer cur size
      Req.Length := Context.Last_Transfer_Size;
      Set_Iid_Offset_List (Context => Context, Iid => Req.Pldm_Hdr.Iid, Offset => Req.Offset);

      Pldm_Hook_Set_Timer (Ret, PLDMFW_TIMEING_PT2 * MSEC_TO_NSEC);
      if Ret /= Pldmfw_Ret_Success
      then
         goto Exit_Point;
      end if;

      Pldm_Hook_Get_Time_Milli_Seconds (Timestamp => Context.Dl_Send_Time);

      Send_Buffer
        (Data => Req, Rx_Interface => Context.Update_Interface, Is_Request => True,
         Size => Get_Size_Bytes (Req'Size));

      <<Exit_Point>>

   end Pldmfw_Send_Request_Fw_Data;

   function Iid_Get_Offset
     (Context : Pldm_Context;
      Iid     : NvU5)
      return NvU32
   is
   begin
      for i in 0 .. (IID_PAIR_SIZE - 1)
      loop
         if Iid = Context.Req_Fw_Data_List.Pairs (NvU8 (i)).Iid.Iid
         then
            return Context.Req_Fw_Data_List.Pairs (NvU8 (i)).Offset;
         end if;
      end loop;
      return 16#ffff_ffff#;
   end Iid_Get_Offset;

   procedure Pldmfw_Cmd_Request_Fw_Data
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context;
      Size    :        NvU32)
   is
      Base_Resp              : Pldm_Base_Resp;
      Res_Iid_Offset         : NvU32;
      End_Flash_Offset       : NvU32;
      Pldm_Resp_Header_Size  : constant NvU32 := Get_Size_Bytes (Pldm_Base_Resp'Size);
      Dl_Program_Start_Time  : NvU32          := 0;
      Dl_Program_Finish_Time : NvU32          := 0;
      Is_Recv_All_Metadata   : NvU8           := 0;
      Fw_Data_Pkt_Size       : NvU32          := 0;

      procedure Image_Write_256B is new Pldm_Hook_Image_Write (Payload_Type => Arr_256B_Buffer);
      procedure Image_Write_4K is new Pldm_Hook_Image_Write (Payload_Type => Arr_4K_Buffer);

   begin

      --  recv response from UA and reset retry count
      Context.Req_Retry := 0;

      Base_Resp :=
        Uc_Array_To_Pldm_Base_Resp
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Pldm_Base_Resp'Size) - 1)));
      if Base_Resp.CC = PLDMFW_CC_RETRY_REQUEST_FW_DATA
      then
         Context.Spi_Flash_Retry := 0;
         Pldm_Hook_Set_Timer (Ret, PLDMFW_FD_TIMEOUT_T2 * MSEC_TO_NSEC);
         goto Exit_Point;

      elsif Base_Resp.CC /= PLDM_CC_SUCCESS
      then
         Context.Slot_State := Pldmfw_Slot_State_Trans_Error;
         Pldmfw_Send_Transfer_Complete (Ret => Ret, Context => Context);
         goto Exit_Point;
      end if;

      Pldm_Hook_Set_Timer (Ret, PLDMFW_TIMEING_PT2 * MSEC_TO_NSEC);
      if Ret /= Pldmfw_Ret_Success
      then
         goto Exit_Point;
      end if;

      --  check iid offset list for UA retry protection
      Res_Iid_Offset := Iid_Get_Offset (Context => Context, Iid => Base_Resp.Pldm_Hdr.Iid);
      --  @todo if request offset is not same to 4k this part of code need to optimized
      if Res_Iid_Offset < Context.Fw_Current_Offset or --  has already written drop pkt
        Res_Iid_Offset = 16#ffff_ffff# --  invalid offset
      then
         Ret := Pldmfw_Ret_Recv_Repeat_Pkt;
         goto Exit_Point;
      end if;

      Minus_U32 (Size, Pldm_Resp_Header_Size, Fw_Data_Pkt_Size);
      if Context.Last_Transfer_Size /= Fw_Data_Pkt_Size
      then
         --  drop the pkt;
         Ret := Pldmfw_Ret_Recv_Unfulfill_Pkt;
         goto Exit_Point;
      end if;

      -- handle receiving fw data
      Pldm_Hook_Clear_Timer (Ret);
      if Ret /= Pldmfw_Ret_Success
      then
         goto Exit_Point;
      end if;

      Pldm_Hook_Get_Time_Milli_Seconds (Timestamp => Dl_Program_Start_Time);

      --  handle version info
      if Context.Is_Metadata_Check_Done = False
      then
         -- collect metadata
         Pldm_Hook_Collect_Metadata
           (Component_Id => Context.Comp_Id, Current_fw_offset => Context.Fw_Current_Offset,
            Transfer_size        => Context.Max_Transfer_Size,
            Data                 =>
              Context.Rx_Msg.Data
                (Pldm_Resp_Header_Size ..
                     Pldm_Resp_Header_Size + NV_PLDM_MAX_TRANSFER_SIZE - 1),
            Is_Recv_All_Metadata => Is_Recv_All_Metadata, Metadata => Context.Metadata.Data);
         Context.Is_Recv_All_Metadata := (if Is_Recv_All_Metadata = 1 then True else False);

         if Context.Is_Recv_All_Metadata = False
         then
            --  if not all version info received, then request more data
            --  update address
            Add_U32
              (Context.Fw_Current_Offset, Context.Max_Transfer_Size, Context.Fw_Current_Offset);
            Pldmfw_Send_Request_Fw_Data (Ret => Ret, Context => Context);
            goto Exit_Point;
         else
            -- recv all version info, check if the version info is valid
            Pldm_Hook_Auth_Metadata
              (Component_Id => Context.Comp_Id, Is_Force_Update => Context.Is_Force_Update,
               Metadata     => Context.Metadata.Data, Ret => Ret);
            Pldm_Hook_Set_State_Cmd_Debug
              (State => Pldmfw_State_Dl, Cmd => PLDMFW_COMMAND_REQUEST_FW_DATA, Ret => Ret);
            Context.Is_Metadata_Check_Done := True;
            if Ret = Pldmfw_Ret_Ap_Sku_Id_Mismatch
            then
               Context.Slot_State     := Pldmfw_Slot_State_Sec_Ver_Fail;
               Context.Verify_Fail_CC := PLDMFW_CMD_VERIFY_COMPLETE_RESULT_VERSION_MISMATCH;
               Pldmfw_Send_Transfer_Complete (Ret => Ret, Context => Context);
               goto Exit_Point;
            elsif Ret = Pldmfw_Ret_Staged_Fw_Lower_Stamp
            then
               Context.Slot_State     := Pldmfw_Slot_State_Sec_Ver_Fail;
               Context.Verify_Fail_CC := PLDMFW_CMD_VERIFY_COMPLETE_RESULT_STAGE_LOWER_VERSION;
               Pldmfw_Send_Transfer_Complete (Ret => Ret, Context => Context);
               goto Exit_Point;
            elsif Ret = Pldmfw_Ret_Staged_Fw_Identical
            then
               Context.Slot_State := Pldmfw_Slot_State_Trans_Comp;
               Pldmfw_Send_Transfer_Complete (Ret => Ret, Context => Context);
               goto Exit_Point;
            elsif Ret /= Pldmfw_Ret_Success
            then
               Pldm_Hook_Set_Timer (Ret, PLDMFW_TIMEING_PT2 * MSEC_TO_NSEC);
               goto Exit_Point;
            end if;

            -- check if need to request first chunk again to write
            if Context.Fw_Current_Offset /= 0
            then
               Context.Fw_Current_Offset := 0;
               Pldmfw_Send_Request_Fw_Data (Ret => Ret, Context => Context);
               goto Exit_Point;
            end if;
         end if;
      end if;
      if Context.Is_Metadata_Check_Done = True
      then
         --  transfer size < 256B, record the current transfer fw data in the buffer
         if Context.Max_Transfer_Size < Get_Size_Bytes (Context.Fw_Data_Buffer.Data'Size)
         then
            Context.Fw_Data_Buffer.Data
              (Context.Fw_Data_Buffer.Cur_Size ..
                   Context.Fw_Data_Buffer.Cur_Size + Fw_Data_Pkt_Size - 1) :=
              Context.Rx_Msg.Data
                (Pldm_Resp_Header_Size .. Pldm_Resp_Header_Size + Fw_Data_Pkt_Size - 1);
            Context.Fw_Data_Buffer.Cur_Size                                :=
              Context.Fw_Data_Buffer.Cur_Size + Fw_Data_Pkt_Size;

            -- if buffer is full write to flash and clear buffer
            if Context.Fw_Data_Buffer.Cur_Size = (Context.Fw_Data_Buffer.Data'Size / NvU8'Size)
            then
               Image_Write_256B
                 (Context.Comp_Id,
                  Data   =>
                    Context.Fw_Data_Buffer.Data
                      (0 .. (Get_Size_Bytes (Context.Fw_Data_Buffer.Data'Size) - 1)),
                  Offset => Context.Wr_Spi_Addr, Size => Context.Fw_Data_Buffer.Cur_Size,
                  Ret    => Ret);
               if Ret /= Pldmfw_Ret_Success
               then
                  --  handling image write fail
                  Pldm_Hook_Set_State_Cmd_Debug
                    (State => PLDMFW_STATE_DL, Cmd => PLDMFW_COMMAND_REQUEST_FW_DATA,
                     Ret   => Ret);
                  if Ret = Pldmfw_Ret_Wp_On
                  then
                     Context.Transfer_Fail_CC := PLDMFW_CMD_TRANSFER_COMPLETE_RESULT_WP_ERROR;
                  elsif Ret /= Pldmfw_Ret_Success
                  then
                     Context.Transfer_Fail_CC :=
                       PLDMFW_CMD_TRANSFER_COMPLETE_RESULT_INTERNAL_ERROR;
                  end if;

                  Pldm_Hook_Set_Timer (Ret, PLDMFW_TIMEING_PT2 * MSEC_TO_NSEC);
                  Add_U8 (Context.Spi_Flash_Retry, 1, Context.Spi_Flash_Retry);
                  goto Exit_Point;
               elsif Context.Fw_Current_Offset = 0
               then
                  --  first time write
                  Context.Current_Rate := Context.Current_Rate + 1;
               end if;
               --  update address
               Add_U32
                 (Context.Fw_Current_Offset, Context.Fw_Data_Buffer.Cur_Size,
                  Context.Fw_Current_Offset);
               Add_U32
                 (Context.Wr_Spi_Addr, Context.Fw_Data_Buffer.Cur_Size, Context.Wr_Spi_Addr);
               Add_U32
                 (Context.Fw_Update_Size, Context.Fw_Data_Buffer.Cur_Size,
                  Context.Fw_Update_Size);

               --  reset buffer
               Context.Fw_Data_Buffer.Cur_Size                                  := 0;
               Context.Fw_Data_Buffer.Data
                 (0 .. (Get_Size_Bytes (Context.Fw_Data_Buffer.Data'Size) - 1)) :=
                 [others => 0];
            end if;
         else
            -- write to flash
            Image_Write_4K
              (Context.Comp_Id,
               Data   =>
                 Context.Rx_Msg.Data
                   (Pldm_Resp_Header_Size ..
                        Pldm_Resp_Header_Size + NV_PLDM_MAX_TRANSFER_SIZE - 1),
               Offset => Context.Wr_Spi_Addr, Size => Context.Last_Transfer_Size, Ret => Ret);
            if Ret /= Pldmfw_Ret_Success
            then
               --  handling image write fail
               Pldm_Hook_Set_State_Cmd_Debug
                 (State => PLDMFW_STATE_DL, Cmd => PLDMFW_COMMAND_REQUEST_FW_DATA, Ret => Ret);
               if Ret = Pldmfw_Ret_Wp_On
               then
                  Context.Transfer_Fail_CC := PLDMFW_CMD_TRANSFER_COMPLETE_RESULT_WP_ERROR;
               elsif Ret /= Pldmfw_Ret_Success
               then
                  Context.Transfer_Fail_CC :=
                    PLDMFW_CMD_TRANSFER_COMPLETE_RESULT_INTERNAL_ERROR;
               end if;

               Pldm_Hook_Set_Timer (Ret, PLDMFW_TIMEING_PT2 * MSEC_TO_NSEC);
               Add_U8 (Context.Spi_Flash_Retry, 1, Context.Spi_Flash_Retry);
               goto Exit_Point;
            elsif Context.Fw_Current_Offset = 0
            then
               --  first time write
               Context.Current_Rate := Context.Current_Rate + 1;
            end if;

            --  update address
            Add_U32 (Context.Fw_Current_Offset, Fw_Data_Pkt_Size, Context.Fw_Current_Offset);
            Add_U32 (Context.Wr_Spi_Addr, Fw_Data_Pkt_Size, Context.Wr_Spi_Addr);
            Add_U32 (Context.Fw_Update_Size, Fw_Data_Pkt_Size, Context.Fw_Update_Size);
         end if;

         Context.Dl_M0_Time :=
           Context.Dl_M0_Time + Dl_Program_Start_Time - Context.Dl_Send_Time;
         Pldm_Hook_Get_Time_Milli_Seconds (Timestamp => Dl_Program_Finish_Time);
         Context.Dl_M1_Time      :=
           Context.Dl_M1_Time + Dl_Program_Finish_Time - Dl_Program_Start_Time;
         Context.Spi_Flash_Retry := 0;
      end if;

      -- check if finished to flash all fw data
      -- if last transfer size is not 4k, then composer sizewill be > 0
      -- else we have already add max transfer size to fw current offset
      Add_U32 (Context.Fw_Current_Offset, Context.Fw_Data_Buffer.Cur_Size, End_Flash_Offset);
      if End_Flash_Offset < Context.Fw_Size
      then
         --  not finished, request more data
         Pldmfw_Send_Request_Fw_Data (Ret => Ret, Context => Context);
         goto Exit_Point;
      else
         Pldm_Hook_Clear_Timer (Ret);
         if Ret /= Pldmfw_Ret_Success
         then
            goto Exit_Point;
         end if;
         -- finished to recv all fw data but not flash remaining yet
         Pldmfw_Generate_Update_Report (Context => Context);
      end if;

      --  flash any remaining data in the buffer
      if (Context.Fw_Data_Buffer.Cur_Size > 0)
      then
         Image_Write_256B
           (Context.Comp_Id,
            Data   =>
              Context.Fw_Data_Buffer.Data
                (0 .. (Get_Size_Bytes (Context.Fw_Data_Buffer.Data'Size) - 1)),
            Offset => Context.Wr_Spi_Addr, Size => Context.Fw_Data_Buffer.Cur_Size, Ret => Ret);
         if Ret /= Pldmfw_Ret_Success
         then
            goto Exit_Point;
         end if;
         --  update address
         Add_U32
           (Context.Fw_Current_Offset, Context.Fw_Data_Buffer.Cur_Size,
            Context.Fw_Current_Offset);
         Add_U32 (Context.Wr_Spi_Addr, Context.Fw_Data_Buffer.Cur_Size, Context.Wr_Spi_Addr);
         Add_U32
           (Context.Fw_Update_Size, Context.Fw_Data_Buffer.Cur_Size, Context.Fw_Update_Size);

         --  reset buffer
         Context.Fw_Data_Buffer.Cur_Size                                  := 0;
         Context.Fw_Data_Buffer.Data
           (0 .. (Get_Size_Bytes (Context.Fw_Data_Buffer.Data'Size) - 1)) :=
           [others => 0];
      end if;

      Context.Fw_Current_Offset := Context.Fw_Size;

      Context.Slot_State := Pldmfw_Slot_State_Trans_Comp;
      Pldmfw_Send_Transfer_Complete (Ret => Ret, Context => Context);

      <<Exit_Point>>

   end Pldmfw_Cmd_Request_Fw_Data;

   procedure Pldmfw_Send_Transfer_Complete
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context)
   is
      Req : Pldmfw_Req_Transfer_Complete;

      procedure Send_Buffer is new Pldm_Hook_Send_Buffer
        (Payload_Type => Pldmfw_Req_Transfer_Complete);

   begin

      Pldmfw_Fill_Req_Header
        (Context => Context, Cmd => PLDMFW_COMMAND_TRANSFER_COMPLETE, Hdr => Req.Pldm_Hdr);

      case Context.Slot_State is
         when Pldmfw_Slot_State_Trans_Comp | Pldmfw_Slot_State_Sec_Ver_Fail =>
            Req.Transfer_Result := PLDMFW_CMD_TRANSFER_COMPLETE_RESULT_SUCCESS;
         when Pldmfw_Slot_State_Trans_Error =>
            Req.Transfer_Result := PLDMFW_CMD_TRANSFER_COMPLETE_RESULT_ERROR;
         when Pldmfw_Slot_State_Spiflash_Error =>
            Req.Transfer_Result := Context.Transfer_Fail_CC;
         when others =>
            --  invalid state
            Ret := Pldmfw_Ret_Invalid_Slot_State;
            goto Exit_Point;
      end case;

      Pldm_Hook_Set_Log (Event => Transfer_Complete, Info0 => Req.Transfer_Result);

      Pldm_Hook_Set_Timer (Ret, PLDMFW_TIMEING_PT2 * MSEC_TO_NSEC);
      if Ret /= Pldmfw_Ret_Success
      then
         goto Exit_Point;
      end if;

      Send_Buffer
        (Data => Req, Rx_Interface => Context.Update_Interface, Is_Request => True,
         Size => Get_Size_Bytes (Req'Size));

      <<Exit_Point>>

   end Pldmfw_Send_Transfer_Complete;

   procedure Pldmfw_Cmd_Transfer_Complete
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context)
   is
   begin

      --  recv response from UA and reset retry count
      Context.Req_Retry := 0;

      case Context.Slot_State is
         when Pldmfw_Slot_State_Sec_Ver_Fail =>
            Pldmfw_Send_Verify_Complete (Ret => Ret, Context => Context);
            if ret = Pldmfw_Ret_Success
            then
               Pldmfw_Change_State
                 (Ret => Ret, Context => Context, New_State => PLDMFW_STATE_VERIFY);
            end if;
            goto Exit_Point;
         when Pldmfw_Slot_State_Trans_Error =>
            Pldm_Hook_Set_Timer (Ret, PLDMFW_FD_TIMEOUT_T1 * MSEC_TO_NSEC);
            Pldm_Hook_Set_State_Cmd_Debug
              (State => PLDMFW_STATE_DL, Cmd => PLDMFW_COMMAND_TRANSFER_COMPLETE, Ret => Ret);
            --  req_retry to max retry value and next time timeout it will directly go to idle mode
            Context.Req_Retry := PLDMFW_TIMEING_PN1;
            Ret               := Pldmfw_Ret_UA_Send_Error_CC_Req_Fwdata;
            goto Exit_Point;
         when Pldmfw_Slot_State_Trans_Comp =>
            --  do nothing
            null;
         when others =>
            --  invalid state
            Ret := Pldmfw_Ret_Invalid_Slot_State;
            goto Exit_Point;
      end case;

      Pldmfw_Change_State (Ret => Ret, Context => Context, New_State => PLDMFW_STATE_VERIFY);
      if Ret /= Pldmfw_Ret_Success
      then
         goto Exit_Point;
      end if;

      --  set authenticate timeout
      Pldm_Hook_Set_Timer (Ret, PLDMFW_FD_TIMEOUT_VERIFY * MSEC_TO_NSEC);
      if Ret /= Pldmfw_Ret_Success
      then
         goto Exit_Point;
      end if;

      --  @todo set pldm verify progress to 0
      Pldm_Hook_Request_Authenticate
        (Component_Id => Context.Comp_Id, Auth_Request_id => Context.Auth_Request_Id);

      <<Exit_Point>>

   end Pldmfw_Cmd_Transfer_Complete;

   procedure Pldmfw_Send_Verify_Complete
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context)
   is
      Req : Pldmfw_Req_Verify_Complete;

      procedure Send_Buffer is new Pldm_Hook_Send_Buffer
        (Payload_Type => Pldmfw_Req_Verify_Complete);

   begin
      Pldmfw_Fill_Req_Header
        (Context => Context, Cmd => PLDMFW_COMMAND_VERIFY_COMPLETE, Hdr => Req.Pldm_Hdr);

      case Context.Slot_State is
         when Pldmfw_Slot_State_Sec_Ver_Fail | Pldmfw_Slot_State_Auth_Fail =>
            Req.Verify_Result := Context.Verify_Fail_CC;
         when Pldmfw_Slot_State_Auth_Success =>
            Req.Verify_Result := PLDMFW_CMD_VERIFY_COMPLETE_RESULT_SUCCESS;
         when Pldmfw_Slot_State_Trans_Comp =>
            Req.Verify_Result := PLDMFW_CMD_VERIFY_COMPLETE_RESULT_ERROR;
         when others =>
            --  invalid state
            Ret := Pldmfw_Ret_Invalid_Slot_State;
            goto Exit_Point;
      end case;

      Pldm_Hook_Set_Timer (Ret, PLDMFW_TIMEING_PT2 * MSEC_TO_NSEC);
      if Ret /= Pldmfw_Ret_Success
      then
         goto Exit_Point;
      end if;

      Send_Buffer
        (Data => Req, Rx_Interface => Context.Update_Interface, Is_Request => True,
         Size => Get_Size_Bytes (Req'Size));

      <<Exit_Point>>

   end Pldmfw_Send_Verify_Complete;

   procedure Pldmfw_Cmd_Verify_Complete
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context)
   is
   begin

      --  recv response from UA and reset retry count
      Context.Req_Retry := 0;

      case Context.Slot_State is
         when Pldmfw_Slot_State_Sec_Ver_Fail =>
            Pldm_Hook_Set_Timer (Ret, PLDMFW_FD_TIMEOUT_T1 * MSEC_TO_NSEC);
            Pldm_Hook_Set_State_Cmd_Debug
              (State => PLDMFW_STATE_VERIFY, Cmd => PLDMFW_COMMAND_VERIFY_COMPLETE, Ret => Ret);
            Context.Req_Retry := PLDMFW_TIMEING_PN1;
            Ret               := Pldmfw_Ret_Version_Check_Fail;
            goto Exit_Point;
         when Pldmfw_Slot_State_Auth_Fail =>
            Pldm_Hook_Set_Timer (Ret, PLDMFW_FD_TIMEOUT_T1 * MSEC_TO_NSEC);
            --  req_retry to max retry value and next time timeout it will directly go to idle mode
            Pldm_Hook_Set_State_Cmd_Debug
              (State => PLDMFW_STATE_VERIFY, Cmd => PLDMFW_COMMAND_VERIFY_COMPLETE, Ret => Ret);
            Context.Req_Retry := PLDMFW_TIMEING_PN1;
            Ret               := Pldmfw_Ret_Fail_Auth_fw;
            goto Exit_Point;
         when Pldmfw_Slot_State_Auth_Success =>
            --  do nothing
            null;
         when others =>
            --  invalid state
            Ret := Pldmfw_Ret_Invalid_Slot_State;
            goto Exit_Point;
      end case;

      Pldmfw_Send_Apply_Complete (Ret => Ret, Context => Context);
      if Ret = Pldmfw_Ret_Success
      then
         Pldmfw_Change_State (Ret => Ret, Context => Context, New_State => PLDMFW_STATE_APPLY);
      end if;

      <<Exit_Point>>

   end Pldmfw_Cmd_Verify_Complete;

   procedure Pldmfw_Send_Apply_Complete
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context)
   is
      APPLY_SUCCESS_WITH_METHOD : constant := 1;
      Req                       : Pldmfw_Req_Apply_Complete;
      Component_Infos           : Component_Info_List;
      Component_Length          : Component_Count;

      procedure Send_Buffer is new Pldm_Hook_Send_Buffer
        (Payload_Type => Pldmfw_Req_Apply_Complete);

   begin
      Pldmfw_Fill_Req_Header
        (Context => Context, Cmd => PLDMFW_COMMAND_APPLY_COMPLETE, Hdr => Req.Pldm_Hdr);

      Pldm_Hook_Get_Component_Info_List
        (Component_Infos => Component_Infos, Component_Length => Component_Length);

      Req.Apply_Result    := APPLY_SUCCESS_WITH_METHOD;
      Req.Activate_Method := DEFAULT_PLDMFW_ACTIVE_METHOD;

      for i in 0 .. (Component_Length - 1)
      loop

         if Context.Comp_Id = Component_Infos (i).Id
         then
            Req.Activate_Method := Component_Infos (i).Active_Method;
            exit;
         end if;
      end loop;

      Pldm_Hook_Set_Timer (Ret, PLDMFW_TIMEING_PT2 * MSEC_TO_NSEC);
      if Ret /= Pldmfw_Ret_Success
      then
         goto Exit_Point;
      end if;

      Send_Buffer
        (Data => Req, Rx_Interface => Context.Update_Interface, Is_Request => True,
         Size => Get_Size_Bytes (Req'Size));

      <<Exit_Point>>
   end Pldmfw_Send_Apply_Complete;

   procedure Pldmfw_Cmd_Apply_Complete
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context)
   is
   begin
      --  recv response from UA and reset retry count
      Context.Req_Retry := 0;

      Pldmfw_Change_State (Ret => Ret, Context => Context, New_State => PLDMFW_STATE_RDY_XFER);

   end Pldmfw_Cmd_Apply_Complete;

   procedure Pldmfw_Cmd_Activate_Fw
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context;
      Size    :        NvU32)
   is
      Resp                       : Pldmfw_Resp_Activate_Fw := DEFAULT_PLDM_RESP_ACTIVATE_FW;
      PLDMFW_SELF_CONTAIN_OFFSET : constant                := 3;

      procedure Send_Buffer is new Pldm_Hook_Send_Buffer
        (Payload_Type => Pldmfw_Resp_Activate_Fw);

      procedure Set_Non_Idempotent_Response is new Pldmfw_Set_Non_Idempotent_Response
        (Payload_Type => Pldmfw_Resp_Activate_Fw);
   begin
      Ret              := Pldmfw_Ret_Success;
      Resp.Pldm_Hdr    :=
        Uc_Array_To_Pldm_Header
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Pldm_Header'Size) - 1)));
      Resp.Pldm_Hdr.Rq := 0;

      if Context.Update_Interface.Src_Eid /= Context.Rx_Interface.Src_Eid
      then
         Resp.CC := PLDM_CC_ERROR;
         Ret     := Pldmfw_Ret_Recv_Update_Cmd_From_Diff_Interface;
         goto Exit_Point;
      end if;

      if Size < Get_Size_Bytes (Pldmfw_Req_Activate_Fw'Size)
      then
         Resp.CC := PLDM_CC_ERR_INVALID_LENGTH;
         Ret     := Pldmfw_Ret_Invalid_Lens;
         goto Exit_Point;
      end if;

      Resp.Self_Contain_Time := 0;

      if Context.Rx_Msg.Data (PLDMFW_SELF_CONTAIN_OFFSET) = 1
      then
         Resp.CC := PLDMFW_CC_SELF_CONTAINED_ACTIVATION_NOT_PERMITTED;
      else
         Resp.CC := PLDM_CC_SUCCESS;
      end if;

      if Context.Verify_Comp_Size.Valid_Size = 0
      then
         Resp.CC := PLDMFW_CC_NOT_IN_UPDATE_MODE;
         Ret     := Pldmfw_Ret_Pending_Img_Not_Permitted;
         goto Exit_Point;
      else

         Context.Activate_Comp_Size := Context.Verify_Comp_Size;

         for i in 0 .. (Context.Activate_Comp_Size.Valid_Size - 1)
         loop

            if NvU32 (i) < Context.Activate_Comp_Size.Comp_Arr'Length

            then

               Pldm_Hook_Handle_Activate
                 (Component_Id => Context.Activate_Comp_Size.Comp_Arr (NvU32 (i)), Ret => Ret);
            end if;

         end loop;

         --  clear verify complete component
         Context.Verify_Comp_Size.Valid_Size := 0;
      end if;

      Set_Non_Idempotent_Response (Context => Context, Hdr => Resp.Pldm_Hdr, Resp => Resp);

      <<Exit_Point>>

      Send_Buffer
        (Data => Resp, Rx_Interface => Context.Rx_Interface,
         Size => Get_Size_Bytes (Resp'Size));

   end Pldmfw_Cmd_Activate_Fw;

   function Dl_Progress
     (Context : Pldm_Context)
      return NvU8
   is
      Ret : NvU64;
   begin
      Ret := NvU64 (Context.Fw_Current_Offset);
      if Context.Fw_Size /= 0
      then
         Ret := Ret * 100;
         Ret := Ret / NvU64 (Context.Fw_Size);
      else
         Ret := NOT_SUPPORT_PERCENTAGE;
      end if;
      return NvU8 ("and" (Ret, 16#0000_0000_0000_00FF#));
   end Dl_Progress;

   procedure Pldmfw_Cmd_Get_Status
     (Ret     : out Pldmfw_Ret;
      Context :     Pldm_Context)
   is
      Resp : Pldmfw_Resp_Get_Status := DEFAULT_PLDM_RESP_GET_STATUS;

      procedure Send_Buffer is new Pldm_Hook_Send_Buffer
        (Payload_Type => Pldmfw_Resp_Get_Status);

   begin
      Ret := Pldmfw_Ret_Success;

      Resp.Pldm_Hdr    :=
        Uc_Array_To_Pldm_Header
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Pldm_Header'Size) - 1)));
      Resp.Pldm_Hdr.Rq := 0;

      Resp.Current_State    := Context.Pldm_State;
      Resp.Previous_State   := Context.Pldm_Previous_State;
      Resp.Reason_Code      := Context.Idle_Reason_Code;
      Resp.Aux_State_Status := 0;
      case Context.Pldm_State is
         when PLDMFW_STATE_IDLE | PLDMFW_STATE_LC | PLDMFW_STATE_RDY_XFER =>
            Resp.Aux_State           := PLDMFW_CMD_GET_STATUS_AUX_NOT_IN_OPERATE;
            Resp.Progress_Percent    := NOT_SUPPORT_PERCENTAGE;
            Resp.Update_Option_Flags := 0;
         when PLDMFW_STATE_APPLY | PLDMFW_STATE_ACTIVATE =>
            Resp.Aux_State        := PLDMFW_CMD_GET_STATUS_AUX_SUCCESS;
            Resp.Progress_Percent := NOT_SUPPORT_PERCENTAGE;
            if Context.Is_Force_Update
            then
               Resp.Update_Option_Flags := 1;
            else
               Resp.Update_Option_Flags := 0;
            end if;
         when PLDMFW_STATE_VERIFY =>
            --  @todo query progress
            Resp.Progress_Percent := 0;
            if Resp.Progress_Percent < 100
            then
               Resp.Aux_State := PLDMFW_CMD_GET_STATUS_AUX_IN_PROGRESS;
            end if;
            if Context.Is_Force_Update
            then
               Resp.Update_Option_Flags := 1;
            else
               Resp.Update_Option_Flags := 0;
            end if;
         when PLDMFW_STATE_DL =>
            Resp.Progress_Percent := Dl_Progress (Context => Context);
            if Resp.Progress_Percent >= 100
            then
               Resp.Aux_State := PLDMFW_CMD_GET_STATUS_AUX_SUCCESS;
            else
               Resp.Aux_State := PLDMFW_CMD_GET_STATUS_AUX_IN_PROGRESS;
            end if;
            if Context.Is_Force_Update
            then
               Resp.Update_Option_Flags := 1;
            else
               Resp.Update_Option_Flags := 0;
            end if;
      end case;

      Send_Buffer
        (Data => Resp, Rx_Interface => Context.Rx_Interface,
         Size => Get_Size_Bytes (Resp'Size));

   end Pldmfw_Cmd_Get_Status;

   procedure Pldmfw_Cmd_Cancel_Update_Component
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context)
   is

      Resp : Pldm_Base_Resp := DEFAULT_PLDM_BASE_RESP;

      procedure Send_Buffer is new Pldm_Hook_Send_Buffer (Payload_Type => Pldm_Base_Resp);

   begin

      Resp.Pldm_Hdr    :=
        Uc_Array_To_Pldm_Header
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Pldm_Header'Size) - 1)));
      Resp.Pldm_Hdr.Rq := 0;

      if Context.Update_Interface.Src_Eid /= Context.Rx_Interface.Src_Eid
      then
         Resp.CC := PLDM_CC_ERROR;
         Ret     := Pldmfw_Ret_Recv_Update_Cmd_From_Diff_Interface;
         goto Exit_Point;
      end if;

      Pldmfw_Change_State (Ret => Ret, Context => Context, New_State => PLDMFW_STATE_RDY_XFER);
      if Ret /= Pldmfw_Ret_Success
      then
         Resp.CC := PLDM_CC_ERROR;
         goto Exit_Point;
      end if;

      Pldm_Hook_Set_Log (Event => Cancel, Info0 => 1);

      <<Exit_Point>>

      Send_Buffer
        (Data => Resp, Rx_Interface => Context.Rx_Interface,
         Size => Get_Size_Bytes (Resp'Size));

   end Pldmfw_Cmd_Cancel_Update_Component;

   procedure Pldmfw_Cmd_Cancel_Update
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context)
   is
      Resp : Pldmfw_Resp_Cancel_Update := DEFAULT_PLDM_RESP_CANCEL_UPDATE;

      procedure Send_Buffer is new Pldm_Hook_Send_Buffer
        (Payload_Type => Pldmfw_Resp_Cancel_Update);

   begin

      Resp.Pldm_Hdr             :=
        Uc_Array_To_Pldm_Header
          (Context.Rx_Msg.Data (0 .. (Get_Size_Bytes (Pldm_Header'Size) - 1)));
      Resp.Pldm_Hdr.Rq          := 0;
      Resp.Nonfunction_Indicate := 0;
      Resp.Nonfunction_BitMap   := 0;

      if Context.Update_Interface.Src_Eid /= Context.Rx_Interface.Src_Eid
      then
         Resp.CC := PLDM_CC_ERROR;
         Ret     := Pldmfw_Ret_Recv_Update_Cmd_From_Diff_Interface;
         goto Exit_Point;
      end if;

      Context.Idle_Reason_Code := PLDMFW_CMD_GET_STATUS_REASON_CANCEL_UPDATE;
      Pldmfw_Change_State (Ret => Ret, Context => Context, New_State => PLDMFW_STATE_IDLE);
      if Ret /= Pldmfw_Ret_Success
      then
         Resp.CC := PLDM_CC_ERROR;
         goto Exit_Point;
      end if;

      Pldm_Hook_Set_Log (Event => Cancel, Info0 => 0);

      <<Exit_Point>>

      Send_Buffer
        (Data => Resp, Rx_Interface => Context.Rx_Interface,
         Size => Get_Size_Bytes (Resp'Size));

   end Pldmfw_Cmd_Cancel_Update;

end Pdk.Pldm.Fwupdate.Cmd;

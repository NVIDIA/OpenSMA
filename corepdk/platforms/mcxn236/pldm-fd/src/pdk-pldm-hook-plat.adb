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

with Ada.Unchecked_Conversion;
with Nv_Types.Shift_Right_Op; use Nv_Types.Shift_Right_Op;
with Pdk.Pldm.Plat; use Pdk.Pldm.Plat;
with Pdk.Pldm.Packet;         use Pdk.Pldm.Packet;
with Nv.Common.Console;
with Pldm_Wrap; use Pldm_Wrap;
with Nv_Types.Get_Size;       use Nv_Types.Get_Size;

package body Pdk.Pldm.Hook.Plat with
  SPARK_Mode => On
is

   --  ///////////////////
   --  /// Hook Up API ///
   --  ///////////////////

   procedure Pldm_Hook_Set_State_Cmd_Debug (State     : Pldmfw_State;
                                            Cmd       : NvU8;
                                            Ret       : Pldmfw_Ret;
                                            Extra_Ret : NvU32 := 0)
   is
      pragma Unreferenced (Extra_Ret);

      type Record_Pldmfw_Ret is
         record
            Ret0   : NvU8;
            Ret1   : NvU8;
         end record
        with
          Size => 16, Object_Size => 16, Alignment => 1;

      Rec : Record_Pldmfw_Ret;

      function Uc_Pldmfw_Ret_To_Rec is new
         Ada.Unchecked_Conversion (Source => Pldmfw_Ret,
                                   Target => Record_Pldmfw_Ret);
   begin

      if Ret /= Pldmfw_Ret_Success
      then

         Rec := Uc_Pldmfw_Ret_To_Rec (Ret);

         Set_Log (Event => LOG_PLDM_ERROR,
                  Info0 => State,
                  Info1 => Cmd,
                  Info2 => Rec.Ret0,
                  Info3 => Rec.Ret1);

      end if;

   end Pldm_Hook_Set_State_Cmd_Debug;

   procedure Pldm_Hook_Debug (Msg  : String;
                              data : NvU32)
   is

      --  subtype String_64_Buffer is String (1 .. 64);
      --  function Uc_Str_To_Array is new
      --     Ada.Unchecked_Conversion (Source => String_64_Buffer,
      --                               Target => Arr_64_Buffer);
   begin

      null;
      --  Print_Msg (Msg  => Uc_Str_To_Array (Msg (Msg'First .. Msg'First + 63)),
      --             Data => data);

   end Pldm_Hook_Debug;

   procedure Pldm_Hook_Send_Buffer (Data          : Payload_Type;
                                    Rx_Interface  : Pldmfw_Interface_Info;
                                    Size          : NvU32;
                                    Is_Request    : Boolean := False)
   is

      Size_To_Send      : NvU32 := Size;
      PLDM_DEBUG        : constant boolean := False;

      subtype Payload_As_Arr_Type is ARR_NvU8_IDX32 (0 .. (Payload_Type'Size / NvU8'Size - 1));
      Buffer        : aliased constant Payload_As_Arr_Type
         with Address => Data'Address, Import;

      Hex_Chars : constant String := "0123456789ABCDEF";
      Byte_Char : String := "00 ";

      procedure To_Hex_String (Str   : in out String;
                               Value : NvU8)
      is
         V         : NvU8 := Value;
      begin
         for I in reverse Str'Range loop
            Str(I) := Hex_Chars(Standard.Integer ((V mod 16) + 1));
            V := V / 16;
         end loop;
      end To_Hex_String;
   begin

      --  range check
      if Size_To_Send > Payload_As_Arr_Type'Size / NvU8'Size
      then
         Size_To_Send := Payload_Type'Size / NvU8'Size;
      end if;

      if Size_To_Send > Arr_Pldm_Tx_Msg_Buffer'Size / NvU8'Size
      then
         Size_To_Send := Arr_Pldm_Tx_Msg_Buffer'Size / NvU8'Size;
      end if;

      --  print buffer
      if PLDM_DEBUG
      then
         Nv.Common.Console.Print ("Pldm:tx\n");
         for I in 0 .. (Size_To_Send - 1) loop
            To_Hex_String (Str   => Byte_Char (1 .. 2),
                           Value => Buffer (I));
            Nv.Common.Console.Print (Byte_Char);
            if (I + 1) mod 16 = 0
            then
               Nv.Common.Console.Print ("\n");
            end if;
         end loop;

         if Size_To_Send mod 16 /= 0
         then
            Nv.Common.Console.Print ("\n");
         end if;
      end if;

      Send_Pldm_Msg_To_Mctp (Buffer         => Buffer,
                             Client         => Rx_Interface.Client,
                             Src_Eid        => Rx_Interface.Src_Eid,
                             Header_Version => Rx_Interface.Header_Version,
                             Message_Tag    => Rx_Interface.Message_Tag,
                             Size           => Size_To_Send,
                             Is_Request     => (if Is_Request then 1 else 0));

   end Pldm_Hook_Send_Buffer;

   procedure Pldm_Hook_Image_Write (Data   : Payload_Type;
                                    Offset : NvU32;
                                    Size   : NvU32;
                                    Ret    : out Pldmfw_Ret)
   is
      Size_To_Write      : NvU32 := Size;
      Err      : NvU16 := 0;
      subtype Payload_As_Arr_Type is ARR_NvU8_IDX32 (0 .. (Get_Size_Bytes(Payload_Type'Size) - 1));
      Buffer : aliased constant Payload_As_Arr_Type
         with Address => Data'Address, Import;

   begin
      Ret := Pldmfw_Ret_Success;

      If Size_To_Write > Get_Size_Bytes(Payload_Type'Size)
      then
         Size_To_Write := Get_Size_Bytes(Payload_Type'Size);
      end if;

      Pldm_Write(Buffer       => Buffer,
                 Offset       => Offset,
                 Size         => Size_To_Write,
                 Err          => Err);

      if Err /= 0
      then
         Ret := Uc_U16_Pldmfw_Ret(Err);
      end if;

   end Pldm_Hook_Image_Write;

   procedure Pldm_Hook_Get_Time_Milli_Seconds (Timestamp  : out NvU32)
   is
   begin

      Timestamp := 0;

      Get_Ticks (Ticks => Timestamp);

      Timestamp := Timestamp * 5;

   end Pldm_Hook_Get_Time_Milli_Seconds;

   procedure Pldm_Hook_Set_Timer (Ret          : out Pldmfw_Ret;
                                  Timestamp_Ns : NvU64)
   is
   begin

      Ret := Pldmfw_Ret_Success;
      Set_Timer (Ns => Timestamp_Ns);

   end Pldm_Hook_Set_Timer;

   procedure Pldm_Hook_Clear_Timer (Ret : out Pldmfw_Ret)
   is
   begin

      Ret := Pldmfw_Ret_Success;
      Clear_Timer;

   end Pldm_Hook_Clear_Timer;

   procedure Pldm_Hook_Get_Component_Info_List (Component_Infos  : out Component_Info_List;
                                                Component_Length : out Component_Count)
   is
      Unused_Build_Mode : NvU8;
      Unused_Fw_State   : NvU8;
   begin

      Component_Length := 0;
      Component_Infos  := [others => DEFAULT_COMPONENT_INFO];

      Get_Fw_Info (Component_Id => Component_Infos(Component_Length).Id,
                   Fw_Size      => Component_Infos(Component_Length).Fw_Size,
                   Fw_Offset    => Component_Infos(Component_Length).Fw_Offset,
                   Ap_Sku_Id    => Component_Infos(Component_Length).Ap_Sku_Id,
                   Build_Mode   => Unused_Build_Mode,
                   Fw_State     => Unused_Fw_State);

      Get_Active_Stamp (Stamp => Component_Infos(Component_Length).Stamp);

      Get_Pending_Stamp (Stamp => Component_Infos(Component_Length).Pending_Stamp);

      Get_Active_Version_String
         (Version_String => Component_Infos(Component_Length).Version_String,
          Length         => Component_Infos(Component_Length).Version_String_Length);

      Get_Pending_Version_String
         (Version_String => Component_Infos(Component_Length).Pending_Version_String,
          Length         => Component_Infos(Component_Length).Pending_Version_String_Length);

      Component_Infos(Component_Length).Active_Method := DEFAULT_PLDMFW_ACTIVE_METHOD;
      Component_Length := Component_Length + 1;

   end Pldm_Hook_Get_Component_Info_List;

   procedure Pldm_Hook_Set_Update_State (Status : NvU8;
                                         Ret    : out Pldmfw_Ret)
   is
      Err      : NvU16 := 0;

   begin
      Ret := Pldmfw_Ret_Success;

      Pldm_Update_Pds(Status  => Status,
                      Err     => Err);

      if Err /= 0
      then
         Ret := Uc_U16_Pldmfw_Ret(Err);
      end if;

   end Pldm_Hook_Set_Update_State;

   procedure Pldm_Hook_Populate_Descriptors (Device_Descriptors        : out Device_Descriptor_List;
                                             Vendor_Descriptors        : out Vendor_Descriptor_List;
                                             Device_Descriptors_Count  : out Descriptor_Count;
                                             Vendor_Descriptors_Count  : out Descriptor_Count)
   is
      Unused_Component_Id : NvU16 := 0;
      Unused_Fw_Size      : NvU32 := 0;
      Unused_Fw_Offset    : NvU32 := 0;
      Ap_Sku_Id           : NvU32 := 0;
      Unused_Build_Mode   : NvU8  := 0;
      Unused_Fw_State     : NvU8  := 0;
      --  pcie
      Vendor_Id           : NvU16 := 0;
      Device_Id           : NvU16 := 0;
      Subsystem_Vendor_Id : NvU16 := 0;
      Subsystem_Device_Id : NvU16 := 0;

      subtype Arr_Idx_4_Buffer is
         ARR_NvU8_IDX32 (0 .. 3)
          with Object_Size => 32;
      function Uc_U32_To_Array is new
         Ada.Unchecked_Conversion (Source => NvU32,
                                   Target => Arr_Idx_4_Buffer);

      subtype Arr_Idx_2_Buffer is
         ARR_NvU8_IDX32 (0 .. 1)
          with Object_Size => 16;
      function Uc_U16_To_Array is new
         Ada.Unchecked_Conversion (Source => NvU16,
                                   Target => Arr_Idx_2_Buffer);
   begin

      --  https://confluence.nvidia.com/display/GFWBC/MCU+SMA+PLDM+T5+Inventory
      Device_Descriptors := [others => DEFAULT_DEVICE_DESCRIPTOR];
      Vendor_Descriptors := [others => DEFAULT_VENDOR_DESCRIPTOR];
      Device_Descriptors_Count := 0;
      Vendor_Descriptors_Count := 0;

      Get_Fw_Info (Component_Id => Unused_Component_Id,
                   Fw_Size      => Unused_Fw_Size,
                   Fw_Offset    => Unused_Fw_Offset,
                   Ap_Sku_Id    => Ap_Sku_Id,
                   Build_Mode   => Unused_Build_Mode,
                   Fw_State     => Unused_Fw_State);

      Get_Pcie_Info (Vendor_Id           => Vendor_Id,
                     Device_Id           => Device_Id,
                     Subsystem_Vendor_Id => Subsystem_Vendor_Id,
                     Subsystem_Device_Id => Subsystem_Device_Id);

      --  iana
      Device_Descriptors(Device_Descriptors_Count).Descriptor_Type := DESCRIPTOR_ID_TYPE_IANA_ENTERPRISE_ID;
      Device_Descriptors(Device_Descriptors_Count).Length          := DESCRIPTOR_ID_LENS_IANA_ENTERPRISE_ID;
      Device_Descriptors(Device_Descriptors_Count).Data (0 .. (DESCRIPTOR_ID_LENS_IANA_ENTERPRISE_ID - 1))
         := Pldm_Enterprise_Id (0 .. (DESCRIPTOR_ID_LENS_IANA_ENTERPRISE_ID - 1));
      Device_Descriptors_Count                                     := Device_Descriptors_Count + 1;

      --  uuid
      Device_Descriptors(Device_Descriptors_Count).Descriptor_Type := DESCRIPTOR_ID_TYPE_UUID;
      Device_Descriptors(Device_Descriptors_Count).Length          := DESCRIPTOR_ID_LENS_UUID;
      Get_Uuid_Info 
         (Uuid => Device_Descriptors(Device_Descriptors_Count).Data (0 .. (DESCRIPTOR_ID_LENS_UUID - 1)));
      Device_Descriptors_Count                                     := Device_Descriptors_Count + 1;

      --  pci vendor id
      Device_Descriptors(Device_Descriptors_Count).Descriptor_Type := DESCRIPTOR_ID_TYPE_PCI_VENDOR_ID;
      Device_Descriptors(Device_Descriptors_Count).Length          := DESCRIPTOR_ID_LENS_PCI_VENDOR_ID;
      Device_Descriptors(Device_Descriptors_Count).Data (0 .. (DESCRIPTOR_ID_LENS_PCI_VENDOR_ID - 1))
         := Uc_U16_To_Array(Vendor_Id);
      Device_Descriptors_Count                                     := Device_Descriptors_Count + 1;

      --  pci device id
      Device_Descriptors(Device_Descriptors_Count).Descriptor_Type := DESCRIPTOR_ID_TYPE_PCI_DEVICE_ID;
      Device_Descriptors(Device_Descriptors_Count).Length          := DESCRIPTOR_ID_LENS_PCI_DEVICE_ID;
      Device_Descriptors(Device_Descriptors_Count).Data (0 .. (DESCRIPTOR_ID_LENS_PCI_DEVICE_ID - 1))
         := Uc_U16_To_Array(Device_Id);
      Device_Descriptors_Count                                     := Device_Descriptors_Count + 1;

      --  pci subsystem vendor id
      Device_Descriptors(Device_Descriptors_Count).Descriptor_Type := DESCRIPTOR_ID_TYPE_PCI_SUBSYSTEM_VENDOR_ID;
      Device_Descriptors(Device_Descriptors_Count).Length          := DESCRIPTOR_ID_LENS_PCI_SUBSYSTEM_VENDOR_ID;
      Device_Descriptors(Device_Descriptors_Count).Data (0 .. (DESCRIPTOR_ID_LENS_PCI_SUBSYSTEM_VENDOR_ID - 1))
         := Uc_U16_To_Array(Subsystem_Vendor_Id);
      Device_Descriptors_Count                                     := Device_Descriptors_Count + 1;

      --  pci subsystem device id
      Device_Descriptors(Device_Descriptors_Count).Descriptor_Type := DESCRIPTOR_ID_TYPE_PCI_SUBSYSTEM_DEVICE_ID;
      Device_Descriptors(Device_Descriptors_Count).Length          := DESCRIPTOR_ID_LENS_PCI_SUBSYSTEM_DEVICE_ID;
      Device_Descriptors(Device_Descriptors_Count).Data (0 .. (DESCRIPTOR_ID_LENS_PCI_SUBSYSTEM_DEVICE_ID - 1))
         := Uc_U16_To_Array(Subsystem_Device_Id);
      Device_Descriptors_Count                                     := Device_Descriptors_Count + 1;

      --  apsku
      Vendor_Descriptors(Vendor_Descriptors_Count).String_Type     := FW_STR_TYPE_ASCII;
      Vendor_Descriptors(Vendor_Descriptors_Count).String_Length   := Pldm_Vendor_Defined_Descriptor_Title_String_Apsku'Length;
      Vendor_Descriptors(Vendor_Descriptors_Count).String (0 .. (Pldm_Vendor_Defined_Descriptor_Title_String_Apsku'Length - 1))
         := Pldm_Vendor_Defined_Descriptor_Title_String_Apsku
            (0 .. (Pldm_Vendor_Defined_Descriptor_Title_String_Apsku'Length - 1));
      Vendor_Descriptors(Vendor_Descriptors_Count).Data_Length     := 4;
      Vendor_Descriptors(Vendor_Descriptors_Count).Data (0 .. 3)   := Uc_U32_To_Array (Ap_Sku_Id);
      Vendor_Descriptors_Count                                     := Vendor_Descriptors_Count + 1;

   end Pldm_Hook_Populate_Descriptors;


   procedure Pldm_Hook_Collect_Metadata (Component_Id    : NvU16;
                                          Current_fw_offset: NvU32;
                                          Transfer_size: NvU32;
                                          Data            : Arr_4k_Buffer;
                                          Is_Recv_All_Metadata : out NvU8;
                                          Metadata : in out ARR_NvU8_IDX16)
   is
      Mcu_Component_Id : NvU16 := 0;
      Unused_Fw_Size   : NvU32 := 0;
      Unused_Fw_Offset : NvU32 := 0;
      Unused_Ap_Sku_Id        : NvU32 := 0;
      Unused_Build_Mode       : NvU8  := 0;
      Unused_Fw_State         : NvU8  := 0;
   begin
      Get_Fw_Info (Component_Id => Mcu_Component_Id,
                   Fw_Size      => Unused_Fw_Size,
                   Fw_Offset    => Unused_Fw_Offset,
                   Ap_Sku_Id    => Unused_Ap_Sku_Id,
                   Build_Mode   => Unused_Build_Mode,
                   Fw_State     => Unused_Fw_State);

      if Component_Id = Mcu_Component_Id
      then

         Parse_Header (Current_fw_offset => Current_fw_offset,
                        Transfer_size     => Transfer_size,
                        Buffer    => Data,
                        Metadata => Metadata,
                        Is_Recv_All_Metadata => Is_Recv_All_Metadata);
      end if;

   end Pldm_Hook_Collect_Metadata;


   procedure Pldm_Hook_Auth_Metadata (Component_Id    : NvU16;
                                       Is_Force_Update : Boolean;
                                       Metadata : ARR_NvU8_IDX16;
                                       Ret             : out Pldmfw_Ret)
   is
      Mcu_Component_Id : NvU16 := 0;
      Unused_Fw_Size   : NvU32 := 0;
      Unused_Fw_Offset : NvU32 := 0;
      Ap_Sku_Id        : NvU32 := 0;
      Build_Mode       : NvU8  := 0;
      Fw_State         : NvU8  := 0;

      Local_Minor    : NvU8  := 0;
      Local_Patch    : NvU16 := 0;
      Local_Build    : NvU16 := 0;
      Local_Ap_Sku_Id : NvU32 := 0;
      Local_Stamp      : NvU32 := 0;

      Inactive_Stamp   : NvU32 := 0;
   begin

      Ret := Pldmfw_Ret_Success;

      Get_Fw_Info (Component_Id => Mcu_Component_Id,
                   Fw_Size      => Unused_Fw_Size,
                   Fw_Offset    => Unused_Fw_Offset,
                   Ap_Sku_Id    => Ap_Sku_Id,
                   Build_Mode   => Build_Mode,
                   Fw_State     => Fw_State);

      if Component_Id = Mcu_Component_Id
      then
         --  get version info from metadata
         Get_FW_Info_From_Metadata
            (Metadata => Metadata,
               Minor                => Local_Minor,
               Patch                => Local_Patch,
               Build                => Local_Build,
               Ap_Sku_Id => Local_Ap_Sku_Id);

         Populate_Stamp (Minor    => Local_Minor,
                           Patch    => Local_Patch,
                           Build    => Local_Build,
                           Stamp    => Local_Stamp);

         Get_Pending_Stamp (Stamp => Inactive_Stamp);

         --  check apsku id at rel build
         --  rel = 0, dev = 1, dbg = 2
         if Build_Mode = 0 and then
            Ap_Sku_Id /= 0
         then
            if Ap_Sku_Id /= Local_Ap_Sku_Id
            then
               Ret := Pldmfw_Ret_Ap_Sku_Id_Mismatch;
               goto Exit_Point;
            end if;
         end if;

         --  staged fw update check
         if Is_Force_Update = False and then
            Fw_State = Uc_Update_State_To_U8 (Stage)
         then
            --  check stamp
            if Local_Stamp = Inactive_Stamp
            then
               Ret := Pldmfw_Ret_Staged_Fw_Identical;
            elsif Local_Stamp < Inactive_Stamp
            then
               Ret := Pldmfw_Ret_Staged_Fw_Lower_Stamp;
            else
               --  higher version case, normal update
               null;
            end if;
         end if;

      end if;

      <<Exit_Point>>
      if Ret = Pldmfw_Ret_Success
      then
         Pldm_Hook_Set_Update_State (Status => UPDATE_INPROGRESS, Ret    => Ret);
      end if;

   end Pldm_Hook_Auth_Metadata;

   procedure Pldm_Hook_Handle_Activate (Component_Id : NvU16;
                                        Ret          : out Pldmfw_Ret)
   is
      Mcu_Component_Id  : NvU16 := 0;
      Unused_Fw_Size    : NvU32 := 0;
      Unused_Fw_Offset  : NvU32 := 0;
      Unused_Ap_Sku_Id  : NvU32 := 0;
      Unused_Build_Mode : NvU8  := 0;
      Unused_Fw_State   : NvU8  := 0;
   begin

      Ret := Pldmfw_Ret_Success;

      Get_Fw_Info (Component_Id => Mcu_Component_Id,
                   Fw_Size      => Unused_Fw_Size,
                   Fw_Offset    => Unused_Fw_Offset,
                   Ap_Sku_Id    => Unused_Ap_Sku_Id,
                   Build_Mode   => Unused_Build_Mode,
                   Fw_State     => Unused_Fw_State);

      Pldm_Hook_Set_Log (Event => Activate,
                         Info0 => NvU8 ("and" (Component_Id, 16#0000_0000_0000_00FF#)),
                         Info1 => NvU8 ("and" (Safe_Shift_Right (Component_Id, 8), 16#0000_0000_0000_00FF#)));

      if Component_Id = Mcu_Component_Id
      then
         Pldm_Hook_Set_Update_State (Status => UPDATE_COMPLETE, Ret    => Ret);
      end if;

   end Pldm_Hook_Handle_Activate;

   procedure Pldm_Hook_Set_Log (Event : Log_Event;
                                Info0 : NvU8 := 0;
                                Info1 : NvU8 := 0;
                                Info2 : NvU8 := 0;
                                Info3 : NvU8 := 0)
   is
      Event_U16 : NvU16;
   begin

      case Event is
         when Change_State =>
            Event_U16 := LOG_PLDM_CHANGE_STATE;
         when Transfer_Complete =>
            Event_U16 := LOG_PLDM_TRANSFER_COMPLETE;
         when Auth =>
            Event_U16 := LOG_PLDM_AUTH;
         when Activate =>
            Event_U16 := LOG_PLDM_ACTIVATE;
         when Timeout =>
            Event_U16 := LOG_PLDM_TIMEOUT;
         when Cancel =>
            Event_U16 := LOG_PLDM_CANCEL;
         when Stage_Update =>
            Event_U16 := LOG_PLDM_STAGE_UPDATE;
      end case;

      Set_Log (Event => Event_U16,
               Info0 => Info0,
               Info1 => Info1,
               Info2 => Info2,
               Info3 => Info3);

   end Pldm_Hook_Set_Log;

   procedure Pldm_Hook_Log_Fw_Update_Report (Fw_Update_Offset : NvU32;
                                             Total_Retry      : NvU32;
                                             Idle_Reason_Code : NvU32;
                                             M0_Time          : NvU32;
                                             M1_Time          : NvU32)
   is
      type U32_Record is record
         data0 : NvU8;
         data1 : NvU8;
         data2 : NvU8;
         data3 : NvU8;
      end record;

      for U32_Record use record
         data0 at 0 range 0 .. 7;
         data1 at 0 range 8 .. 15;
         data2 at 0 range 16 .. 23;
         data3 at 0 range 24 .. 31;
      end record;

      U32_Rec : U32_Record;

      function Uc_U32_To_Record is new
         Ada.Unchecked_Conversion (Source => NvU32,
                                   Target => U32_Record);
   begin

      U32_Rec := Uc_U32_To_Record (Fw_Update_Offset);

      Set_Log (Event => LOG_PLDM_UPDATE_OFFSET,
               Info0 => U32_Rec.data0,
               Info1 => U32_Rec.data1,
               Info2 => U32_Rec.data2,
               Info3 => U32_Rec.data3);

      U32_Rec := Uc_U32_To_Record (Total_Retry);

      Set_Log (Event => LOG_PLDM_TOTAL_RETRY,
               Info0 => U32_Rec.data0,
               Info1 => U32_Rec.data1,
               Info2 => U32_Rec.data2,
               Info3 => U32_Rec.data3);

      U32_Rec := Uc_U32_To_Record (Idle_Reason_Code);

      Set_Log (Event => LOG_PLDM_IDLE_REASON,
               Info0 => U32_Rec.data0,
               Info1 => U32_Rec.data1,
               Info2 => U32_Rec.data2,
               Info3 => U32_Rec.data3);

      U32_Rec := Uc_U32_To_Record (M0_Time);

      Set_Log (Event => LOG_PLDM_M0_TIME,
               Info0 => U32_Rec.data0,
               Info1 => U32_Rec.data1,
               Info2 => U32_Rec.data2,
               Info3 => U32_Rec.data3);

      U32_Rec := Uc_U32_To_Record (M1_Time);

      Set_Log (Event => LOG_PLDM_M1_TIME,
               Info0 => U32_Rec.data0,
               Info1 => U32_Rec.data1,
               Info2 => U32_Rec.data2,
               Info3 => U32_Rec.data3);

   end Pldm_Hook_Log_Fw_Update_Report;

   procedure Pldm_Hook_Handle_Stage_Update (Component_Id : NvU16;
                                            Ret          : out Pldmfw_Ret)
   is
      Mcu_Component_Id  : NvU16 := 0;
      Unused_Fw_Size    : NvU32 := 0;
      Unused_Fw_Offset  : NvU32 := 0;
      Unused_Ap_Sku_Id  : NvU32 := 0;
      Unused_Build_Mode : NvU8  := 0;
      Unused_Fw_State   : NvU8  := 0;
   begin
      Ret := Pldmfw_Ret_Success;

      Get_Fw_Info (Component_Id => Mcu_Component_Id,
                   Fw_Size      => Unused_Fw_Size,
                   Fw_Offset    => Unused_Fw_Offset,
                   Ap_Sku_Id    => Unused_Ap_Sku_Id,
                   Build_Mode   => Unused_Build_Mode,
                   Fw_State     => Unused_Fw_State);

      Pldm_Hook_Set_Log (Event => Stage_Update,
                         Info0 => NvU8 ("and" (Component_Id, 16#0000_0000_0000_00FF#)),
                         Info1 => NvU8 ("and" (Safe_Shift_Right (Component_Id, 8), 16#0000_0000_0000_00FF#)));

      if Component_Id = Mcu_Component_Id
      then
         Pldm_Hook_Set_Update_State (Status => UPDATE_STAGED, Ret    => Ret);
      end if;

   end Pldm_Hook_Handle_Stage_Update;

   procedure Pldm_Hook_Is_Staged (Component_Id : NvU16;
                                  Is_Staged    : out Boolean)
   is
      Mcu_Component_Id  : NvU16 := 0;
      Unused_Fw_Size    : NvU32 := 0;
      Unused_Fw_Offset  : NvU32 := 0;
      Unused_Ap_Sku_Id  : NvU32 := 0;
      Unused_Build_Mode : NvU8  := 0;
      Fw_State          : NvU8  := 0;
   begin

      Is_Staged := False;

      Get_Fw_Info (Component_Id => Mcu_Component_Id,
                   Fw_Size      => Unused_Fw_Size,
                   Fw_Offset    => Unused_Fw_Offset,
                   Ap_Sku_Id    => Unused_Ap_Sku_Id,
                   Build_Mode   => Unused_Build_Mode,
                   Fw_State     => Fw_State);

      if Component_Id = Mcu_Component_Id and then
         Fw_State = Uc_Update_State_To_U8 (Stage)
      then
         Is_Staged := True;
      end if;

   end Pldm_Hook_Is_Staged;

   procedure Pldm_Hook_Request_Authenticate
   is
   begin
      Request_Authentication;

   end Pldm_Hook_Request_Authenticate;

   procedure Pldm_Hook_Get_Limited_Rate_Info (Info : out Limited_Rate_Info) is
   begin
      Info := DEFAULT_LIMITED_RATE_INFO;
   end Pldm_Hook_Get_Limited_Rate_Info;

end Pdk.Pldm.Hook.Plat;


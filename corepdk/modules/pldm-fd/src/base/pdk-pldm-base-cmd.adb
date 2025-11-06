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

--  @summary
--  PLDM base defines and routines
--
--  @description
--  This package contains procedure to perform PLDM - T0 Base operations.
--
with Nv_Types.Get_Size;       use Nv_Types.Get_Size;
with Nv_Types.Shift_Left_Op;  use Nv_Types.Shift_Left_Op;
with Nv_Types.Shift_Right_Op; use Nv_Types.Shift_Right_Op;
with Pdk.Pldm.Hook.Plat;      use Pdk.Pldm.Hook.Plat;

package body Pdk.Pldm.Base.Cmd with
  SPARK_Mode => On
is

   -----------------------
   -- Process_Base_Commands --
   -----------------------

   procedure Process_Base_Commands
     (Pldm_Ctx : aliased in out Pldm_Base_Context;
      Size     :                NvU32)
   is

      Hdr : Pldm_Header;
      Cc  : NvU8 := PLDM_CC_SUCCESS;

   begin

      Hdr :=
        Uc_Array_To_Pldm_Header
          (Pldm_Ctx.Req.Data
             (Pldm_Ctx.Req.Data'First ..
                  Pldm_Ctx.Req.Data'First + Get_Size_Bytes (Arr_Pldm_Header'Size) - 1));

      case Hdr.Command_Code is

         when BASE_COMMAND_SET_TID =>

            if Size < Get_Size_Bytes (Arr_Pldm_Set_Eid_Req'Size)
            then

               Cc := PLDM_CC_ERR_INVALID_LENGTH;

               goto Exit_Point;

            end if;

            On_Set_Tid
              (Pldm_Ctx => Pldm_Ctx,
               Buffer   =>
                 Pldm_Ctx.Req.Data
                   (Pldm_Ctx.Req.Data'First ..
                        Pldm_Ctx.Req.Data'First + Get_Size_Bytes (Arr_Pldm_Set_Eid_Req'Size) -
                        1));

         when BASE_COMMAND_GET_TID =>

            if Size < Get_Size_Bytes (Arr_Pldm_Get_Eid_Req'Size)
            then

               Cc := PLDM_CC_ERR_INVALID_LENGTH;
               goto Exit_Point;

            end if;

            On_Get_Tid
              (Pldm_Ctx => Pldm_Ctx,
               Buffer   =>
                 Pldm_Ctx.Req.Data
                   (Pldm_Ctx.Req.Data'First ..
                        Pldm_Ctx.Req.Data'First + Get_Size_Bytes (Arr_Pldm_Get_Eid_Req'Size) -
                        1));

         when BASE_COMMAND_GET_PLDM_VER =>

            if Size < Get_Size_Bytes (Arr_Pldm_Get_Pldm_Ver_Req'Size)
            then

               Cc := PLDM_CC_ERR_INVALID_LENGTH;
               goto Exit_Point;

            end if;

            Get_Pldm_Version
              (Pldm_Ctx => Pldm_Ctx,
               Buffer   =>
                 Pldm_Ctx.Req.Data
                   (Pldm_Ctx.Req.Data'First ..
                        Pldm_Ctx.Req.Data'First +
                        Get_Size_Bytes (Arr_Pldm_Get_Pldm_Ver_Req'Size) - 1));

         when BASE_COMMAND_GET_PLDM_TYPES =>

            if Size < Get_Size_Bytes (Arr_Pldm_Get_Pldm_Types_Req'Size)
            then

               Cc := PLDM_CC_ERR_INVALID_LENGTH;

               goto Exit_Point;

            end if;

            Get_Pldm_Types
              (Pldm_Ctx => Pldm_Ctx,
               Buffer   =>
                 Pldm_Ctx.Req.Data
                   (Pldm_Ctx.Req.Data'First ..
                        Pldm_Ctx.Req.Data'First +
                        Get_Size_Bytes (Arr_Pldm_Get_Pldm_Types_Req'Size) - 1));

         when BASE_COMMAND_GET_PLDM_CMDS =>

            if Size < Get_Size_Bytes (Arr_Pldm_Get_Pldm_Cmds_Req'Size)
            then

               Cc := PLDM_CC_ERR_INVALID_LENGTH;

               goto Exit_Point;

            end if;

            Get_Pldm_Cmds
              (Pldm_Ctx => Pldm_Ctx,
               Buffer   =>
                 Pldm_Ctx.Req.Data
                   (Pldm_Ctx.Req.Data'First ..
                        Pldm_Ctx.Req.Data'First +
                        Get_Size_Bytes (Arr_Pldm_Get_Pldm_Cmds_Req'Size) - 1));

         when others =>

            Cc := PLDM_CC_ERR_UNSUPPORTED_CMD;

      end case;

      <<Exit_Point>>

      if Cc /= PLDM_CC_SUCCESS
      then

         Respond_Error (Pldm_Ctx => Pldm_Ctx, Req => Hdr, Cc => Cc);

      end if;

   end Process_Base_Commands;

   ----------------------
   -- Fill_Resp_Header --
   ----------------------

   procedure Fill_Resp_Header
     (Req  :     Pldm_Header;
      Resp : out Pldm_Header)
   is
   begin

      Resp    := Req;
      Resp.Rq := 0;

   end Fill_Resp_Header;

   -------------------
   -- Respond_Error --
   -------------------

   procedure Respond_Error
     (Pldm_Ctx : aliased Pldm_Base_Context;
      Req      :         Pldm_Header;
      Cc       :         NvU8)
   is

      Res : Pldm_Error_Res;
      procedure Send_Buffer is new Pldm_Hook_Send_Buffer (Payload_Type => Pldm_Error_Res);

   begin

      Fill_Resp_Header (Req => Req, Resp => Res.Hdr);

      Res.Cc := Cc;

      Send_Buffer
        (Data => Res, Rx_Interface => Pldm_Ctx.Rx_Interface, Size => Get_Size_Bytes (Res'Size));

   end Respond_Error;

   -----------
   -- Crc32 --
   -----------

   procedure Crc32
     (Buffer  :     ARR_NvU8_IDX32;
      Ret_Val : out NvU32)
   is

      Crc : NvU32 := 16#FFFF_FFFF#;
      B   : NvU32;
      Ch  : NvU32;

      CRC32_POLYNOMIAL : constant := 16#EDB8_8320#;

   begin

      for I in Buffer'Range
      loop

         Ch := NvU32 (Buffer (NvU32 (I)));

         for J in 1 .. 8
         loop

            B := (Ch xor Crc) and 1;

            Crc := Safe_Shift_Right (Crc, 1);

            if B /= 0
            then

               Crc := Crc xor CRC32_POLYNOMIAL;

            end if;

            Ch := Safe_Shift_Right (Ch, 1);

         end loop;

      end loop;

      Ret_Val := Crc xor 16#FFFF_FFFF#;

   end Crc32;

   ----------------
   -- On_Set_Tid --
   ----------------

   procedure On_Set_Tid
     (Pldm_Ctx : aliased in out Pldm_Base_Context;
      Buffer   :                Arr_Pldm_Set_Eid_Req)
   is

      Req  : Pldm_Set_Eid_Req;
      Resp : Pldm_Set_Eid_Resp;

      procedure Send_Buffer is new Pldm_Hook_Send_Buffer (Payload_Type => Pldm_Set_Eid_Resp);

   begin

      Req := Uc_Array_To_Pldm_Set_Eid_Req (Buffer);

      Fill_Resp_Header (Req => Req.Hdr, Resp => Resp.Hdr);

      Resp.Cc := PLDM_CC_SUCCESS;

      Pldm_Ctx.Tid := Req.Tid;

      Send_Buffer
        (Data => Resp, Rx_Interface => Pldm_Ctx.Rx_Interface,
         Size => Get_Size_Bytes (Resp'Size));

   end On_Set_Tid;

   ----------------
   -- On_Get_Tid --
   ----------------
   procedure On_Get_Tid
     (Pldm_Ctx : aliased Pldm_Base_Context;
      Buffer   :         Arr_Pldm_Get_Eid_Req)
   is

      Req  : Pldm_Get_Eid_Req;
      Resp : Pldm_Get_Eid_Res;

      procedure Send_Buffer is new Pldm_Hook_Send_Buffer (Payload_Type => Pldm_Get_Eid_Res);

   begin

      Req := Uc_Array_To_Pldm_Get_Eid_Req (Buffer);

      Fill_Resp_Header (Req => Req.Hdr, Resp => Resp.Hdr);

      Resp.Cc  := PLDM_CC_SUCCESS;
      Resp.Tid := Pldm_Ctx.Tid;

      Send_Buffer
        (Data => Resp, Rx_Interface => Pldm_Ctx.Rx_Interface,
         Size => Get_Size_Bytes (Resp'Size));

   end On_Get_Tid;

   ----------------------
   -- Get_Pldm_Version --
   ----------------------

   procedure Get_Pldm_Version
     (Pldm_Ctx : aliased Pldm_Base_Context;
      Buffer   :         Arr_Pldm_Get_Pldm_Ver_Req)
   is
      Req       : Pldm_Get_Pldm_Ver_Req;
      Resp      : Pldm_Get_Pldm_Ver_Res;
      Cap       : Pldm_Capability :=
        (P_Type => 0,
         Ver    => 0,
         Cmds   =>
           [others => 0]);
      Cap_Valid : Boolean         := False;
      Ver_Arr   : Arr_Pldm_Ver;

      procedure Send_Buffer is new Pldm_Hook_Send_Buffer
        (Payload_Type => Pldm_Get_Pldm_Ver_Res);

   begin

      Req := Uc_Array_To_Pldm_Get_Pldm_Ver_Req (Buffer);

      Fill_Resp_Header (Req => Req.Hdr, Resp => Resp.Hdr);

      Resp.Cc     := PLDM_CC_SUCCESS;
      Resp.Handle := 0;
      Resp.Flag   := INVALID;
      Resp.Ver    := 0;
      Resp.Crc    := 0;

      if Req.Flag = GET_FIRST_PART
      then
         Pldm_Ctx_Cap_Loop :
         for I in Pldm_Ctx.Caps'Range
         loop

            if Req.P_Type = Pldm_Ctx.Caps (I).P_Type
            then

               Cap       := Pldm_Ctx.Caps (I);
               Cap_Valid := True;
               exit Pldm_Ctx_Cap_Loop;

            end if;

         end loop Pldm_Ctx_Cap_Loop;

         if not Cap_Valid
         then

            Resp.Cc := PLDM_BASE_CC_INVALID_PLDM_TYPE_IN_REQUEST_DATA;
            goto Exit_Point;

         end if;

         Resp.Handle := 0;
         Resp.Flag   := START_AND_END;
         Resp.Ver    := Cap.Ver;

         Ver_Arr := Uc_Pldm_Ver_To_Array (Resp.Ver);

         Crc32 (Buffer => Ver_Arr, Ret_Val => Resp.Crc);

      else

         Resp.Cc := PLDM_BASE_CC_INVALID_TRANSFER_OPERATION_FLAG;

      end if;

      <<Exit_Point>>

      Send_Buffer
        (Data => Resp, Rx_Interface => Pldm_Ctx.Rx_Interface,
         Size => Get_Size_Bytes (Resp'Size));

   end Get_Pldm_Version;

   --------------------
   -- Get_Pldm_Types --
   --------------------

   procedure Get_Pldm_Types
     (Pldm_Ctx : aliased Pldm_Base_Context;
      Buffer   :         Arr_Pldm_Get_Pldm_Types_Req)
   is
      Req       : Pldm_Get_Pldm_Types_Req;
      Resp      : Pldm_Get_Pldm_Types_Res;
      Val       : NvU8;
      Shift_Val : NvU8;
      Index     : NvU8;

      procedure Send_Buffer is new Pldm_Hook_Send_Buffer
        (Payload_Type => Pldm_Get_Pldm_Types_Res);

   begin

      Req := Uc_Array_To_Pldm_Get_Pldm_Types_Req (Buffer);

      Fill_Resp_Header (Req => Req.Hdr, Resp => Resp.Hdr);

      Resp.Cc      := PLDM_CC_SUCCESS;
      Resp.P_Types := [others => 0];

      for I in Pldm_Ctx.Caps'Range
      loop

         Index     := (Pldm_Ctx.Caps (I).P_Type / 8) and NvU8'Last;
         Shift_Val := (Pldm_Ctx.Caps (I).P_Type mod 8) and NvU8'Last;
         Val       := Resp.P_Types (NvU32 (Index));
         Shift_Val := Safe_Shift_Left (1, Natural (Shift_Val));

         Resp.P_Types (NvU32 (Index)) := Shift_Val or Val;

      end loop;

      Send_Buffer
        (Data => Resp, Rx_Interface => Pldm_Ctx.Rx_Interface,
         Size => Get_Size_Bytes (Resp'Size));

   end Get_Pldm_Types;

   -------------------
   -- Get_Pldm_Cmds --
   -------------------

   procedure Get_Pldm_Cmds
     (Pldm_Ctx : aliased Pldm_Base_Context;
      Buffer   :         Arr_Pldm_Get_Pldm_Cmds_Req)
   is
      Req       : Pldm_Get_Pldm_Cmds_Req;
      Resp      : Pldm_Get_Pldm_Cmds_Res;
      Cap       : Pldm_Capability :=
        (P_Type => 0,
         Ver    => 0,
         Cmds   =>
           [others => 0]);
      Cap_Valid : Boolean         := False;

      procedure Send_Buffer is new Pldm_Hook_Send_Buffer
        (Payload_Type => Pldm_Get_Pldm_Cmds_Res);

   begin

      Req := Uc_Array_To_Pldm_Get_Pldm_Cmds_Req (Buffer);

      Fill_Resp_Header (Req => Req.Hdr, Resp => Resp.Hdr);

      Resp.Cc   := PLDM_CC_SUCCESS;
      Resp.Cmds := [others => 0];

      for I in Pldm_Ctx.Caps'Range
      loop

         if Req.P_Type = Pldm_Ctx.Caps (I).P_Type
         then

            Cap       := Pldm_Ctx.Caps (I);
            Cap_Valid := True;
            exit;

         end if;

      end loop;

      if not Cap_Valid
      then

         Resp.Cc := PLDM_BASE_CC_INVALID_PLDM_TYPE_IN_REQUEST_DATA;
         goto Exit_Point;

      end if;

      if Req.Ver = Cap.Ver
      then

         Resp.Cmds := Cap.Cmds;

      else

         Resp.Cc := PLDM_BASE_CC_INVALID_PLDM_VERSION_IN_REQUEST_DATA;

      end if;

      <<Exit_Point>>

      Send_Buffer
        (Data => Resp, Rx_Interface => Pldm_Ctx.Rx_Interface,
         Size => Get_Size_Bytes (Resp'Size));

   end Get_Pldm_Cmds;

   procedure Init_Pldm_Base_Ctx (Pldm_Ctx : aliased out Pldm_Base_Context) is

      Shift_Val : NvU8;
      Index     : NvU8;
      Val       : NvU8;

   begin

      --  To please flow analysis
      for I in Pldm_Ctx.Caps'Range
      loop

         Pldm_Ctx.Caps (I) :=
           (P_Type => 0,
            Ver    => 0,
            Cmds   =>
              [others => 0]);

      end loop;

      Pldm_Ctx.Tid          := 0;
      Pldm_Ctx.Req          :=
        (Data =>
           [others => 0]);
      Pldm_Ctx.Rx_Interface := DEFAULT_PLDMFW_INTERFACE_INFO;

      --  PLDM type msg control and discovery
      Pldm_Ctx.Caps (0).P_Type := PLDM_TYPE_MSG_CTRL_AND_DISCOVERY;
      Pldm_Ctx.Caps (0).Ver    := PLDM_BASE_CMD_LSB_VER;

      --  Populate the capabilities
      for I in P_Base_Cmd_Arr'Range
      loop

         Index     := (P_Base_Cmd_Arr (I) / 8) and NvU8'Last;
         Shift_Val := (P_Base_Cmd_Arr (I) mod 8) and NvU8'Last;
         Val       := Pldm_Ctx.Caps (0).Cmds (NvU32 (Index));
         Shift_Val := Safe_Shift_Left (1, Natural (Shift_Val));

         Pldm_Ctx.Caps (0).Cmds (NvU32 (Index)) := Shift_Val or Val;

      end loop;

      --  PLDM type firmware update
      Pldm_Ctx.Caps (1).P_Type := PLDM_TYPE_FIRMWARE_UPDATE;
      Pldm_Ctx.Caps (1).Ver    := PLDM_FWUPDATE_CMD_LSB_VER;

      --  Populate the capabilities
      for I in P_Fw_Cmd_Arr'Range
      loop

         Index     := (P_Fw_Cmd_Arr (I) / 8) and NvU8'Last;
         Shift_Val := (P_Fw_Cmd_Arr (I) mod 8) and NvU8'Last;
         Val       := Pldm_Ctx.Caps (1).Cmds (NvU32 (Index));
         Shift_Val := Safe_Shift_Left (1, Natural (Shift_Val));

         Pldm_Ctx.Caps (1).Cmds (NvU32 (Index)) := Shift_Val or Val;

      end loop;

   end Init_Pldm_Base_Ctx;

end Pdk.Pldm.Base.Cmd;

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
with Pdk.Pldm.Device; use Pdk.Pldm.Device;

--  @brief PLDM Firmware Update Command Package
--  @details This package implements the command handlers for PLDM firmware update protocol.
--           It provides types and procedures for handling all firmware update related commands.
--  @see PLDM specification for firmware update protocol details
package Pdk.Pldm.Fwupdate.Cmd with
  SPARK_Mode => On
is

   --  @brief Base response type for PLDM commands
   --  @details Contains header and completion code
   type Pldm_Base_Resp is record
      Pldm_Hdr : Pldm_Header;
      CC       : NvU8;
   end record with
     Size => 32, Object_Size => 32, Alignment => 1;

   --  @brief Default base response with success completion code
   DEFAULT_PLDM_BASE_RESP : constant Pldm_Base_Resp :=
     (DEFAULT_PLDM_HEADER,
      PLDM_CC_SUCCESS);

   --  @brief Fixed-size array type for 4-byte values
   subtype Arr_U8_Idx_4 is ARR_NvU8_IDX32 (0 .. 3) with
       Object_Size => 32;

   --  @brief Fixed-size array type for 8-byte values
   subtype Arr_U8_Idx_8 is ARR_NvU8_IDX32 (0 .. 7) with
       Object_Size => 64;

   --  @brief Safe conversion from array to base response
   function Uc_Array_To_Pldm_Base_Resp is new Ada.Unchecked_Conversion
     (Source => Arr_U8_Idx_4, Target => Pldm_Base_Resp);

   --  @brief Safe conversion from array to 64-bit unsigned integer
   function Uc_Array_To_U64 is new Ada.Unchecked_Conversion
     (Source => Arr_U8_Idx_8, Target => NvU64);

   --  @brief Calculates the lower power of 2 for a given number
   --  @param x Input number
   --  @param Ret Output parameter containing the result
   procedure Lower_Power_Of_2
     (x   :     NvU32;
      Ret : out NvU32);

   --  @brief Safe subtraction for 32-bit unsigned integers
   --  @param a First operand
   --  @param b Second operand
   --  @param c Output parameter containing the result
   --  @note Handles underflow by returning 0
   procedure Minus_U32
     (a :     NvU32;
      b :     NvU32;
      c : out NvU32) with
     Global => null;

   --  @brief Safe subtraction for 8-bit unsigned integers
   --  @param a First operand
   --  @param b Second operand
   --  @param c Output parameter containing the result
   --  @note Handles underflow by returning 0
   procedure Minus_U8
     (a :     NvU8;
      b :     NvU8;
      c : out NvU8) with
     Global => null;

   --  @brief Safe addition for 32-bit unsigned integers
   --  @param a First operand
   --  @param b Second operand
   --  @param c Output parameter containing the result
   --  @note Handles overflow by returning maximum value
   procedure Add_U32
     (a :     NvU32;
      b :     NvU32;
      c : out NvU32) with
     Global => null;

   --  @brief Safe addition for 8-bit unsigned integers
   --  @param a First operand
   --  @param b Second operand
   --  @param c Output parameter containing the result
   --  @note Handles overflow by returning maximum value
   procedure Add_U8
     (a :     NvU8;
      b :     NvU8;
      c : out NvU8) with
     Global => null;

   --  @brief Converts little-endian buffer to 32-bit unsigned integer
   --  @param buffer Input buffer containing little-endian data
   --  @param Ret Output parameter containing the converted value
   procedure Le_U32
     (buffer :     Arr_U8_Idx_4;
      Ret    : out NvU32);

   --  @brief Generates firmware update report
   --  @param Context The PLDM context containing update statistics
   --  @note Logs update progress and timing information
   procedure Pldmfw_Generate_Update_Report (Context : Pldm_Context);

   --  @brief is rate limit reached
   --  @param Context The PLDM context containing update statistics
   --  @param Is_Limited_Rate_Reached Output parameteter if limited rate reached
   --  @note check if rate limit is reached
   procedure Pldmfw_Is_Rate_Limit_Reached
     (Context                 : in out Pldm_Context;
      Is_Limited_Rate_Reached :    out Boolean);

   --  @brief Initializes PLDM context with default values
   --  @param Pldm_Ctx Output parameter containing initialized context
   --  @note Sets up all context fields to their initial values
   procedure Pldmfw_Context_Init (Pldm_Ctx : out Pldm_Context) with
     Export => True, Convention => CPP, External_Name => "ada_pldmfw_context_init";

   --  @brief Increments the Instance ID (IID) counter
   --  @param Context The PLDM context
   --  @param Iid Output parameter containing the new IID
   --  @note Handles IID wraparound at maximum value
   procedure Pldmfw_Increment_Iid
     (Context : in out Pldm_Context;
      Iid     :    out NvU5);

   --  @brief Fills PLDM request header with command information
   --  @param Context The PLDM context
   --  @param Cmd Command code to set in header
   --  @param Hdr Output parameter containing filled header
   --  @note Sets up header fields including IID and command type
   procedure Pldmfw_Fill_Req_Header
     (Context : in out Pldm_Context;
      Cmd     :        NvU8;
      Hdr     :    out Pldm_Header);

   --  @brief Sends error response with specified completion code
   --  @param Context The PLDM context
   --  @param CC Completion code to send
   --  @note Constructs and sends error response packet
   procedure Pldmfw_Response_Error_Code
     (Context : Pldm_Context;
      CC      : NvU8);

   --  @brief Generic procedure for setting non-idempotent response
   --  @param Context The PLDM context
   --  @param Hdr PLDM header for the response
   --  @param Resp Response payload to store
   --  @note Stores response for potential retransmission
   generic
      type Payload_Type is private;
   procedure Pldmfw_Set_Non_Idempotent_Response
     (Context : in out Pldm_Context;
      Hdr     :        Pldm_Header;
      Resp    :        Payload_Type);

   --  @brief Handles response to non-idempotent command
   --  @param Ret Output parameter indicating success/failure
   --  @param Context The PLDM context
   --  @param Iid Instance ID of the command
   --  @param Cmd Command code
   --  @note Manages response retransmission for non-idempotent commands
   procedure Pldmfw_Response_Non_Idempotent_Cmd
     (Ret     : out Pldmfw_Ret;
      Context :     Pldm_Context;
      Iid     :     NvU5;
      Cmd     :     NvU8);

   --  @brief Constant defining firmware type identifier
   FIRMWARE_TYPE : constant NvU16 := 16#0a#;

   --  @brief Handles Query Device ID command
   --  @param Ret Output parameter indicating success/failure
   --  @param Context The PLDM context
   --  @note Returns device identification information
   procedure Pldmfw_Cmd_Query_Device_Id
     (Ret     : out Pldmfw_Ret;
      Context :     Pldm_Context);

   --  @brief Record type for component parameter table
   --  @details Contains information about firmware components
   type Pldmfw_Component_Parameter_Table is record
      Comp_Class                 : NvU16;
      Comp_Id                    : NvU16;
      Comp_Class_Index           : NvU8;
      Active_Comp_Stamp          : NvU32;
      Active_Comp_Ver_Str_Type   : NvU8;
      Active_Comp_Ver_Str_Len    : NvU8;
      Active_Comp_Relase_Date    : Arr_U8_Idx_8;
      Pending_Comp_Stamp         : NvU32;
      Pending_Comp_Ver_Str_Type  : NvU8;
      Pending_Comp_Ver_Str_Len   : NvU8;
      Pending_Comp_Relase_Date   : Arr_U8_Idx_8;
      Comp_Active_Method         : Pldmfw_Active_Method;
      Capabilities_During_Update : NvU32;
   end record with
     Size => 312, Object_Size => 312, Alignment => 1;

   --  @brief Record type for Get Firmware Parameters response
   --  @details Contains firmware update capabilities and component information
   type Pldmfw_Resp_Get_Fw_Parameters is record
      Pldm_Hdr                   : Pldm_Header;
      CC                         : NvU8;
      Capabilities_During_Update : NvU32;
      Component_Count            : NvU16;
      Active_Comp_Str_Type       : NvU8;
      Active_Comp_Str_Len        : NvU8;
      Pending_Comp_Str_Type      : NvU8;
      Pending_Comp_Str_Len       : NvU8;
   end record with
     Size => 112, Object_Size => 112, Alignment => 1;

   --  @brief Handles Get Firmware Parameters command
   --  @param Ret Output parameter indicating success/failure
   --  @param Context The PLDM context
   --  @note Returns firmware update capabilities and component information
   procedure Pldmfw_Cmd_Get_Fw_Parameters
     (Ret     : out Pldmfw_Ret;
      Context :     Pldm_Context);

   --  @brief Record type for Request Update command
   --  @details Contains update request parameters
   type Pldmfw_Req_Request_Update is record
      Pldm_Hdr                     : Pldm_Header;
      Max_Transfer_Size            : NvU32;
      Num_Component                : NvU16;
      Max_Outstanding_Transfer_Req : NvU8;
      Pkg_Data_Length              : NvU16;
      Comp_Version_String_Type     : NvU8;
      Comp_Version_String_Length   : NvU8;
   end record with
     Size => 112, Object_Size => 112, Alignment => 1;

   --  @brief Default Request Update request
   DEFAULT_PLDM_REQ_REQUEST_UPDARTE : constant Pldmfw_Req_Request_Update :=
     (DEFAULT_PLDM_HEADER,
      0,
      0,
      0,
      0,
      0,
      0);

   --  @brief Array type for Request Update request
   subtype Array_Pldmfw_Req_Request_Update is
     ARR_NvU8_IDX32 (0 .. (Pldmfw_Req_Request_Update'Size / NvU8'Size) - 1) with
       Object_Size => 112;

   function Uc_Array_To_Req_Request_Update is new Ada.Unchecked_Conversion
     (Source => Array_Pldmfw_Req_Request_Update, Target => Pldmfw_Req_Request_Update);

   --  @brief Record type for Request Update response
   --  @details Contains update request response information
   type Pldmfw_Resp_Request_Update is record
      Pldm_Hdr                     : Pldm_Header;
      CC                           : NvU8;
      Fw_Device_Metadata_Length    : NvU16;
      Fd_Will_Send_Get_Package_Cmd : NvU8;
   end record with
     Size => 56, Object_Size => 56, Alignment => 1;

   --  @brief Default Request Update response with success completion code
   DEFAULT_PLDM_RESP_REQUEST_UPDATE : constant Pldmfw_Resp_Request_Update :=
     (DEFAULT_PLDM_HEADER,
      PLDM_CC_SUCCESS,
      0,
      0);

   --  @brief Handles Request Update command
   --  @param Ret Output parameter indicating success/failure
   --  @param Context The PLDM context
   --  @param Size Size of the received command data
   --  @note Initiates firmware update process
   procedure Pldmfw_Cmd_Request_Update
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context;
      Size    :        NvU32);

   --  @brief Record type for Pass Component Table command
   --  @details Contains component table information
   type Pldmfw_Req_Pass_Component_Table is record
      Pldm_Hdr           : Pldm_Header;
      Transfer_Flag      : NvU8;
      Class              : NvU16;
      Id                 : NvU16;
      Class_Index        : NvU8;
      Comp_Stamp         : NvU32;
      Version_Str_Type   : NvU8;
      Version_Str_Length : NvU8;
   end record with
     Size => 120, Object_Size => 120, Alignment => 1;

   --  @brief Default Pass Component Table request
   DEFAULT_PLDM_REQ_PASS_COMP_TABLE : constant Pldmfw_Req_Pass_Component_Table :=
     (DEFAULT_PLDM_HEADER,
      0,
      0,
      0,
      0,
      0,
      0,
      0);

   --  @brief Array type for Pass Component Table request
   subtype Array_Pldmfw_Req_Pass_Component_Table is
     ARR_NvU8_IDX32 (0 .. (Pldmfw_Req_Pass_Component_Table'Size / NvU8'Size) - 1);

   --  @brief Safe conversion from array to Pass Component Table request
   function Uc_Array_To_Req_Pass_Component_Table is new Ada.Unchecked_Conversion
     (Source => Array_Pldmfw_Req_Pass_Component_Table,
      Target => Pldmfw_Req_Pass_Component_Table);

   --  @brief Record type for Pass Component Table response
   --  @details Contains component compatibility information
   type Pldmfw_Resp_Pass_Component_Table is record
      Pldm_Hdr            : Pldm_Header;
      CC                  : NvU8;
      Component_Resp      : NvU8;
      Component_Resp_Code : NvU8;
   end record with
     Size => 48, Object_Size => 48, Alignment => 1;

   --  @brief Default Pass Component Table response
   DEFAULT_PLDM_RESP_PASS_COMP_TABLE : constant Pldmfw_Resp_Pass_Component_Table :=
     (DEFAULT_PLDM_HEADER,
      PLDM_CC_SUCCESS,
      PLDMFW_COMP_COMPATIBILITY_RESPONSE_MAY_BE_UPDATABLE,
      PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_NOT_SUPPORT);

   --  @brief Handles Pass Component Table command
   --  @param Ret Output parameter indicating success/failure
   --  @param Context The PLDM context
   --  @param Size Size of the received command data
   --  @note Processes component compatibility information
   procedure Pldmfw_Cmd_Pass_Component_Table
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context;
      Size    :        NvU32);

   --  @brief Record type for Update Component command
   --  @details Contains component update parameters
   type Pldmfw_Req_Update_Component is record
      Pldm_Hdr           : Pldm_Header;
      Class              : NvU16;
      Id                 : NvU16;
      Class_Index        : NvU8;
      Comp_Stamp         : NvU32;
      Comp_Size          : NvU32;
      Update_Options     : NvU32;
      Version_Str_Type   : NvU8;
      Version_Str_Length : NvU8;
   end record with
     Size => 176, Object_Size => 176, Alignment => 1;

   --  @brief Default Update Component request
   DEFAULT_PLDM_REQ_UPDATE_COMP : constant Pldmfw_Req_Update_Component :=
     (DEFAULT_PLDM_HEADER,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0);

   --  @brief Array type for Update Component request
   subtype Array_Pldmfw_Req_Update_Component is
     ARR_NvU8_IDX32 (0 .. (Pldmfw_Req_Update_Component'Size / NvU8'Size) - 1);

   --  @brief Safe conversion from array to Update Component request
   function Uc_Array_To_Req_Update_Component is new Ada.Unchecked_Conversion
     (Source => Array_Pldmfw_Req_Update_Component, Target => Pldmfw_Req_Update_Component);

   --  @brief Record type for Update Component response
   --  @details Contains component update response information
   type Pldmfw_Resp_Update_Component is record
      Pldm_Hdr                             : Pldm_Header;
      CC                                   : NvU8;
      Component_Resp                       : NvU8;
      Component_Resp_Code                  : NvU8;
      Update_Options_Enabled               : NvU32;
      Est_Time_Before_Send_Request_Fw_Data : NvU16;
   end record with
     Size => 96, Object_Size => 96, Alignment => 1;

   --  @brief Default Update Component response
   DEFAULT_PLDM_RESP_UPDATE_COMP : constant Pldmfw_Resp_Update_Component :=
     (DEFAULT_PLDM_HEADER,
      PLDM_CC_SUCCESS,
      PLDMFW_COMP_COMPATIBILITY_RESPONSE_CAN_BE_UPDATE,
      PLDMFW_COMP_COMPATIBILITY_RESPONSE_CODE_CAN_BE_UPDATE,
      0,
      0);

   --  @brief Handles Update Component command
   --  @param Ret Output parameter indicating success/failure
   --  @param Context The PLDM context
   --  @param Size Size of the received command data
   --  @note Initiates component update process
   procedure Pldmfw_Cmd_Update_Component
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context;
      Size    :        NvU32);

   --  @brief Record type for Request Firmware Data command
   --  @details Contains firmware data request parameters
   type Pldmfw_Req_Request_Fw_Data is record
      Pldm_Hdr : Pldm_Header;
      Offset   : NvU32;
      Length   : NvU32;
   end record with
     Size => 88, Object_Size => 88, Alignment => 1;

   --  @brief Default Request Firmware Data command
   DEFAULT_PLDM_REQ_REQUEST_FW_DATA : constant Pldmfw_Req_Request_Fw_Data :=
     (DEFAULT_PLDM_HEADER,
      0,
      0);

   --  @brief Sets IID and offset pair in the request list
   --  @param Context The PLDM context
   --  @param Iid Instance ID to store
   --  @param Offset Offset value to store
   --  @note Maintains list of IID-offset pairs for request tracking
   procedure Set_Iid_Offset_List
     (Context : in out Pldm_Context;
      Iid     :        NvU5;
      Offset  :        NvU32);

   --  @brief Sends Request Firmware Data command
   --  @param Ret Output parameter indicating success/failure
   --  @param Context The PLDM context
   --  @note Initiates firmware data transfer
   procedure Pldmfw_Send_Request_Fw_Data
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context);

   --  @brief Retrieves offset for a given IID
   --  @param Context The PLDM context
   --  @param Iid Instance ID to look up
   --  @return Offset value associated with the IID
   function Iid_Get_Offset
     (Context : Pldm_Context;
      Iid     : NvU5)
      return NvU32;

   --  @brief Handles Request Firmware Data command
   --  @param Ret Output parameter indicating success/failure
   --  @param Context The PLDM context
   --  @param Size Size of the received command data
   --  @note Processes firmware data transfer
   procedure Pldmfw_Cmd_Request_Fw_Data
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context;
      Size    :        NvU32);

   --  @brief Record type for Transfer Complete command
   --  @details Contains transfer completion information
   type Pldmfw_Req_Transfer_Complete is record
      Pldm_Hdr        : Pldm_Header;
      Transfer_Result : NvU8;
   end record with
     Size => 32, Object_Size => 32, Alignment => 1;

   --  @brief Default Transfer Complete command
   DEFAULT_PLDM_REQ_TRANSFER_COMP : constant Pldmfw_Req_Transfer_Complete :=
     (DEFAULT_PLDM_HEADER,
      PLDMFW_CMD_TRANSFER_COMPLETE_RESULT_SUCCESS);

   --  @brief Sends Transfer Complete command
   --  @param Ret Output parameter indicating success/failure
   --  @param Context The PLDM context
   --  @note Indicates completion of firmware transfer
   procedure Pldmfw_Send_Transfer_Complete
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context);

   --  @brief Handles Transfer Complete command
   --  @param Ret Output parameter indicating success/failure
   --  @param Context The PLDM context
   --  @note Processes transfer completion
   procedure Pldmfw_Cmd_Transfer_Complete
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context);

   --  @brief Record type for Verify Complete command
   --  @details Contains verification completion information
   type Pldmfw_Req_Verify_Complete is record
      Pldm_Hdr      : Pldm_Header;
      Verify_Result : NvU8;
   end record with
     Size => 32, Object_Size => 32, Alignment => 1;

   --  @brief Default Verify Complete command
   DEFAULT_PLDM_REQ_VERIFY_COMP : constant Pldmfw_Req_Verify_Complete :=
     (DEFAULT_PLDM_HEADER,
      PLDMFW_CMD_VERIFY_COMPLETE_RESULT_SUCCESS);

   --  @brief Sends Verify Complete command
   --  @param Ret Output parameter indicating success/failure
   --  @param Context The PLDM context
   --  @note Indicates completion of firmware verification
   procedure Pldmfw_Send_Verify_Complete
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context);

   --  @brief Handles Verify Complete command
   --  @param Ret Output parameter indicating success/failure
   --  @param Context The PLDM context
   --  @note Processes verification completion
   procedure Pldmfw_Cmd_Verify_Complete
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context);

   --  @brief Record type for Apply Complete command
   --  @details Contains application completion information
   type Pldmfw_Req_Apply_Complete is record
      Pldm_Hdr        : Pldm_Header;
      Apply_Result    : NvU8;
      Activate_Method : Pldmfw_Active_Method;
   end record with
     Size => 48, Object_Size => 48, Alignment => 1;

   --  @brief Sends Apply Complete command
   --  @param Ret Output parameter indicating success/failure
   --  @param Context The PLDM context
   --  @note Indicates completion of firmware application
   procedure Pldmfw_Send_Apply_Complete
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context);

   --  @brief Handles Apply Complete command
   --  @param Ret Output parameter indicating success/failure
   --  @param Context The PLDM context
   --  @note Processes application completion
   procedure Pldmfw_Cmd_Apply_Complete
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context);

   --  @brief Record type for Activate Firmware command
   --  @details Contains firmware activation parameters
   type Pldmfw_Req_Activate_Fw is record
      Pldm_Hdr     : Pldm_Header;
      Self_Contain : NvU8;
   end record with
     Size => 32, Object_Size => 32, Alignment => 1;

   --  @brief Default Activate Firmware command
   DEFAULT_PLDM_REQ_ACTIVATE_FW : constant Pldmfw_Req_Activate_Fw :=
     (DEFAULT_PLDM_HEADER,
      0);

   --  @brief Record type for Activate Firmware response
   --  @details Contains firmware activation response information
   type Pldmfw_Resp_Activate_Fw is record
      Pldm_Hdr          : Pldm_Header;
      CC                : NvU8;
      Self_Contain_Time : NvU16;
   end record with
     Size => 48, Object_Size => 48, Alignment => 1;

   --  @brief Default Activate Firmware response
   DEFAULT_PLDM_RESP_ACTIVATE_FW : constant Pldmfw_Resp_Activate_Fw :=
     (DEFAULT_PLDM_HEADER,
      PLDM_CC_SUCCESS,
      0);

   --  @brief Handles Activate Firmware command
   --  @param Ret Output parameter indicating success/failure
   --  @param Context The PLDM context
   --  @param Size Size of the received command data
   --  @note Processes firmware activation
   procedure Pldmfw_Cmd_Activate_Fw
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context;
      Size    :        NvU32);

   --  @brief Constant indicating unsupported percentage value
   NOT_SUPPORT_PERCENTAGE : constant := 16#65#;

   --  @brief Record type for Get Status command
   --  @details Contains status request parameters
   type Pldmfw_Req_Get_Status is record
      Pldm_Hdr : Pldm_Header;
   end record with
     Size => 24, Object_Size => 24, Alignment => 1;

   --  @brief Record type for Get Status response
   --  @details Contains current status information
   type Pldmfw_Resp_Get_Status is record
      Pldm_Hdr            : Pldm_Header;
      CC                  : NvU8;
      Current_State       : NvU8;
      Previous_State      : NvU8;
      Aux_State           : NvU8;
      Aux_State_Status    : NvU8;
      Progress_Percent    : NvU8;
      Reason_Code         : NvU8;
      Update_Option_Flags : NvU32;
   end record with
     Size => 112, Object_Size => 112, Alignment => 1;

   --  @brief Default Get Status response
   DEFAULT_PLDM_RESP_GET_STATUS : constant Pldmfw_Resp_Get_Status :=
     (DEFAULT_PLDM_HEADER,
      PLDM_CC_SUCCESS,
      PLDMFW_STATE_IDLE,
      PLDMFW_STATE_IDLE,
      PLDMFW_CMD_GET_STATUS_AUX_IN_PROGRESS,
      0,
      0,
      PLDMFW_CMD_GET_STATUS_REASON_INIT_FW_DEVICE,
      0);

   --  @brief Calculates download progress percentage
   --  @param Context The PLDM context
   --  @return Progress percentage value
   function Dl_Progress
     (Context : Pldm_Context)
      return NvU8;

   --  @brief Handles Get Status command
   --  @param Ret Output parameter indicating success/failure
   --  @param Context The PLDM context
   --  @note Returns current update status
   procedure Pldmfw_Cmd_Get_Status
     (Ret     : out Pldmfw_Ret;
      Context :     Pldm_Context);

   --  @brief Handles Cancel Update Component command
   --  @param Ret Output parameter indicating success/failure
   --  @param Context The PLDM context
   --  @note Cancels current component update
   procedure Pldmfw_Cmd_Cancel_Update_Component
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context);

   --  @brief Record type for Cancel Update response
   --  @details Contains cancellation response information
   type Pldmfw_Resp_Cancel_Update is record
      Pldm_Hdr             : Pldm_Header;
      CC                   : NvU8;
      Nonfunction_Indicate : NvU8;
      Nonfunction_BitMap   : NvU64;
   end record with
     Size => 104, Object_Size => 104, Alignment => 1;

   --  @brief Default Cancel Update response
   DEFAULT_PLDM_RESP_CANCEL_UPDATE : constant Pldmfw_Resp_Cancel_Update :=
     (DEFAULT_PLDM_HEADER,
      PLDM_CC_SUCCESS,
      0,
      0);

   --  @brief Handles Cancel Update command
   --  @param Ret Output parameter indicating success/failure
   --  @param Context The PLDM context
   --  @note Cancels entire update process
   procedure Pldmfw_Cmd_Cancel_Update
     (Ret     :    out Pldmfw_Ret;
      Context : in out Pldm_Context);

end Pdk.Pldm.Fwupdate.Cmd;

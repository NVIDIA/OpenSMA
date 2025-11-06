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

--  @brief PLDM Firmware Update State Machine Package
--  @details This package implements the state machine for PLDM firmware update protocol.
--           It handles state transitions, timeout management, and command routing
--           based on the current state.
--  @see PLDM specification for firmware update protocol details
package Pdk.Pldm.Fwupdate.State with
  SPARK_Mode => On
is

   --  @brief Changes the current state of the PLDM firmware update state machine
   --  @param Ret Output parameter indicating success/failure of state change
   --  @param Context The PLDM context containing current state information
   --  @param New_State The desired new state to transition to
   --  @note This procedure handles state transition validation and cleanup
   procedure Pldmfw_Change_State
     (Ret       :    out Pldmfw_Ret;
      Context   : in out Pldm_Context;
      New_State :        Pldmfw_State);

   --  @brief Handles commands received while in IDLE state
   --  @param Context The PLDM context
   --  @param Size Size of the received command data
   --  @note Only basic queries and update initiation are allowed in this state
   procedure Pldmfw_State_Idle_Handler
     (Context : in out Pldm_Context;
      Size    :        NvU32);

   --  @brief Handles commands received while in LEARN COMPONENT state
   --  @param Context The PLDM context
   --  @param Size Size of the received command data
   --  @note Handles component table passing and update preparation
   procedure Pldmfw_State_Lc_Handler
     (Context : in out Pldm_Context;
      Size    :        NvU32);

   --  @brief Handles commands received while in READY TRANSFER state
   --  @param Context The PLDM context
   --  @param Size Size of the received command data
   --  @note Manages component update initiation and firmware transfer preparation
   procedure Pldmfw_State_Rdy_Xfer_Handler
     (Context : in out Pldm_Context;
      Size    :        NvU32);

   --  @brief Handles commands received while in DOWNLOAD state
   --  @param Context The PLDM context
   --  @param Size Size of the received command data
   --  @note Manages firmware data transfer and progress tracking
   procedure Pldmfw_State_Dl_Handler
     (Context : in out Pldm_Context;
      Size    :        NvU32);

   --  @brief Handles commands received while in VERIFY state
   --  @param Context The PLDM context
   --  @note Manages firmware verification process
   procedure Pldmfw_State_Verify_Handler (Context : in out Pldm_Context);

   --  @brief Handles commands received while in APPLY state
   --  @param Context The PLDM context
   --  @note Manages firmware application process
   procedure Pldmfw_State_Apply_Handler (Context : in out Pldm_Context);

   --  @brief Handles commands received while in ACTIVATE state
   --  @param Context The PLDM context
   --  @note Manages firmware activation process
   procedure Pldmfw_State_Activate_Handler (Context : in out Pldm_Context);

   --  @brief Main state machine runner that routes commands to appropriate handlers
   --  @param Context The PLDM context
   --  @param Size Size of the received command data
   --  @note This is the main entry point for command processing
   procedure Pldmfw_State_Runner
     (Context : in out Pldm_Context;
      Size    :        NvU32) with
     Export => True, Convention => CPP, External_Name => "ada_pldmfw_state_runner";

   --  @brief Handles timeout events for the state machine
   --  @param Context The PLDM context
   --  @note Manages retries and timeout-based state transitions
   procedure Pldmfw_Timeout_Handler (Context : in out Pldm_Context) with
     Export => True, Convention => CPP, External_Name => "ada_pldmfw_timeout_handler";

   --  @brief Handles authenticate result
   --  @param Context The PLDM context
   --  @param Verify_Cc verify complete code
   --  @note Manages authenticate result
   procedure Pldmfw_Authenticate_Handler
     (Context   : in out Pldm_Context;
      Verify_Cc :        NvU8) with
     Export => True, Convention => CPP, External_Name => "ada_pldmfw_authenticate_handler";

end Pdk.Pldm.Fwupdate.State;

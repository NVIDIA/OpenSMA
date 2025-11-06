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
package Pdk.Pldm is

   PLDM_TYPE_MSG_CTRL_AND_DISCOVERY : constant := 0;
   PLDM_TYPE_FIRMWARE_UPDATE        : constant := 5;

   --  pldm CC
   PLDM_CC_SUCCESS               : constant := 16#00#;
   PLDM_CC_ERROR                 : constant := 16#01#;
   PLDM_CC_ERR_INVALID_DATA      : constant := 16#02#;
   PLDM_CC_ERR_INVALID_LENGTH    : constant := 16#03#;
   PLDM_CC_ERR_INVALID_NOT_READY : constant := 16#04#;
   PLDM_CC_ERR_UNSUPPORTED_CMD   : constant := 16#05#;
   PLDM_CC_INVALID_PLDM_TYPE     : constant := 16#20#;

end Pdk.Pldm;

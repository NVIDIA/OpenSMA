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

-- mctp_channel.adb
--
-- This file has the implementation for the interfaces of the platform transport code to the SPDM
-- responder from AdaCore
--

with Interfaces.C;
with System;


package body Mctp_Channel is
    -- procedure to send data to the transport layer
    procedure Send (
        Buffer : RFLX.RFLX_Builtin_Types.Bytes) is
        procedure C_Send_Data( Buf : System.Address;
                               Len : in Interfaces.C.size_t) with
            Import => True,
            Convention => C,
            External_Name => "get_data_from_spdm_responsder_c";
        C_Length : constant Interfaces.C.size_t := Interfaces.C.size_t(Buffer'Length);
    begin
        C_Send_Data(Buffer'Address, C_Length);
    end Send;

    -- procedure receive data from the transport layer
    procedure Receive (
        Buffer : out RFLX.RFLX_Builtin_Types.Bytes;
        Length : out RFLX.RFLX_Builtin_Types.Length) is
        procedure C_Receive_Data( Buf : System.Address;
                                  Len : in out Interfaces.C.size_t) with
            Import => True,
            Convention => C,
            External_Name => "send_data_to_spdm_responsder_c";
        C_Length : Interfaces.C.size_t := Buffer'Length;
    begin
        C_Receive_Data(Buffer'Address, C_Length);
        Length := RFLX.RFLX_Builtin_Types.Length(C_Length);
    end Receive;

    -- trace function
    procedure Trace_Func (
        Var : Integer) is
        procedure C_Trace_Func( Var : in Integer ) with
            Import => True,
            Convention => C,
            External_Name => "trace_func";
        begin
            C_Trace_Func(Var);
        end Trace_Func;

--    type Index is range 1 .. 2;
    -- function to return true if there is data for the responder to consume
    function Has_Data return Index is
        function C_Has_Data return Interfaces.C.int with
            Import => True,
            Convention => C,
            External_Name => "has_data_for_spdm_responsder_c";
        use type Interfaces.C.int;
        Ret_Val : Interfaces.C.int;
    begin
        loop
            Ret_Val := C_Has_Data;
            if Ret_Val = 1 then
                return 1;
            end if;
            if Ret_Val = 2 then
                return 2;
            end if;
        end loop;
        
    end Has_Data;

end Mctp_Channel;

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

-- mctp_channel.ads
--
-- This file contains the interfaces for the platform transport code to interface with the SPDM responder
-- from AdaCore
--

with Rflx.Rflx_Builtin_Types;

generic
package Mctp_Channel with
  spark_mode
is
    -- sends data from responder through the channel
   procedure Send(Buffer: Rflx.Rflx_Builtin_Types.Bytes);

    -- receives data from the channel to the responder
   procedure Receive
     (Buffer: out Rflx.Rflx_Builtin_Types.Bytes; Length: out Rflx.Rflx_Builtin_Types.Length);

    -- trace function
   procedure Trace_Func(Var: Integer);

   type Index is range 1 .. 2;
    -- checks if there is data to receive from the channel
   function Has_Data return Index;

end Mctp_Channel;

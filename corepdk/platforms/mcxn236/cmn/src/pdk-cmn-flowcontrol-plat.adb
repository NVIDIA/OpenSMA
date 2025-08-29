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
with Pdk.Cmn.Types; use Pdk.Cmn.Types;

package body Pdk.Cmn.Flowcontrol.Plat is
   
   procedure Exit_Program(Ex: Integer) is
      procedure System_Abort(Ex: Uint8) with
        import => True, convention => C, external_name => "System_Abort";
   begin
      System_Abort(Uint8 (Ex));
   end exit_program;

end Pdk.Cmn.FlowControl.Plat;

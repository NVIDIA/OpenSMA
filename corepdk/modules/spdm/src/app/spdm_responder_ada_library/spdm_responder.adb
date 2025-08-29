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

-- spdm_responder.adb
--
-- This file has the implementation for the main entry routine into the SPDM responder

with Mctp_Channel;
with Rflx.Spdm_Responder.Session;
with Rflx.Rflx_Types;
with Spdm_C_Responder;

package body Spdm_Responder is
   procedure Main is

      use type Rflx.Rflx_Types.Index;
      use type Rflx.Rflx_Types.Length;

      package Sr renames Rflx.Spdm_Responder.Session;

      type Mainindex is range 1 .. 2;
      type Context_Array is array(Mainindex) of Spdm_C_Responder.Context;

   --   function  Get_Main_Buffer_Address return System.Address;
   --   pragma Import(C,Get_Main_Buffer_Address,"spdm_get_main_buffer_address");
   --   MainBuffer : RFLX.RFLX_Types.Bytes (RFLX.RFLX_Types.Index'First .. RFLX.RFLX_Types.Index'First + 2049) with Address => Get_Main_Buffer_Address;
      Mainbuffer:
        Rflx.Rflx_Types.Bytes
          (Rflx.Rflx_Types.Index'First .. Rflx.Rflx_Types.Index'First + 2_049);

      Maincontexts: Context_Array;

      generic
         with procedure Send(Buffer: Rflx.Rflx_Types.Bytes);
         with procedure Receive
           (Buffer: out Rflx.Rflx_Types.Bytes; Length: out Rflx.Rflx_Types.Length);

      procedure Run_Responder(Context: in out Spdm_C_Responder.Context) with
        pre => Sr.Initialized(Context), post => Sr.Initialized(Context);
    -- The `Run_Responder` procedure has the following tasks:
    --
    --    1. Bring the state machine in a state where data is needed, if that is not already the case.
    --    2. Provide the received data to the state machine.
    --    3. Bring the state machine in a state where all actions are done (e.g., sending a response).
    --       This is the case as soon as the state machine reaches a state where data is needed again.
    --
    -- If the state machine is not active or reaches a final state, not all of these tasks may be
    -- executed.
      procedure Run_Responder(Context: in out Spdm_C_Responder.Context) is
         Data_Available: Boolean := True;
--        Buffer : RFLX.RFLX_Types.Bytes (RFLX.RFLX_Types.Index'First .. RFLX.RFLX_Types.Index'First + 4095) := (others => 0);
         Length        : Rflx.Rflx_Types.Length;
      begin
         pragma assert(Sr.Initialized(Context));

         while Sr.Active(Context) and Data_Available loop
            pragma loop_invariant(Sr.Initialized(Context));

            if Sr.Needs_Data(Context, Sr.C_Transport) then
               declare
                  Bs: constant Rflx.Rflx_Types.Length :=
                    Sr.Write_Buffer_Size(Context, Sr.C_Transport);
               begin
                  Receive(Mainbuffer, Length);
                  if Length = 0 then
                        -- no data available, set the flag to exit loop
                     Data_Available := False;
                  elsif Length > 0 and Length <= Bs then
                        -- data available, write it to the state machine
                     Sr.Write
                       (Context,
                        Sr.C_Transport,
                        Mainbuffer
                          (Mainbuffer'First ..
                               Mainbuffer'First + Rflx.Rflx_Types.Index(Length) - 1));
                  end if;
               end;
            end if;
            if Data_Available then
                -- run the state machine
               Sr.Run(Context);

                -- check if response is available
               if Sr.Has_Data(Context, Sr.C_Transport) then
                  declare
                     Bs: constant Rflx.Rflx_Types.Length :=
                       Sr.Read_Buffer_Size(Context, Sr.C_Transport);
                  begin
                        -- send the response
                     if Mainbuffer'Length >= Bs then
                        Sr.Read
                          (Context,
                           Sr.C_Transport,
                           Mainbuffer
                             (Mainbuffer'First ..
                                  Mainbuffer'First - 2 + Rflx.Rflx_Types.Index(Bs + 1)));
                        Send
                          (Mainbuffer
                             (Mainbuffer'First ..
                                  Mainbuffer'First - 2 + Rflx.Rflx_Types.Index(Bs + 1)));
                     end if;
                  end;
               end if;
            end if;

         end loop;

         pragma assert(Sr.Initialized(Context));
      end Run_Responder;

      package Transport_Channel is new Mctp_Channel;
      procedure Run_Responder_1 is new Run_Responder
        (Transport_Channel.Send, Transport_Channel.Receive);
      procedure Run_Responder_2 is new Run_Responder
        (Transport_Channel.Send, Transport_Channel.Receive);

--    Contexts       : Context_Array;

   begin
        -- initialize the smbus and spi contexts
      for I in Mainindex loop
         Sr.Initialize(Maincontexts(I));
         Spdm_C_Responder.Plat_Initialize(Maincontexts(I));
      end loop;
      loop
         pragma loop_invariant(Sr.Initialized(Maincontexts(1)));
         pragma loop_invariant(Sr.Initialized(Maincontexts(2)));

         while (for all I in Mainindex => Sr.Active(Maincontexts(I))) loop
            pragma loop_invariant(Sr.Initialized(Maincontexts(1)));
            pragma loop_invariant(Sr.Initialized(Maincontexts(2)));

                -- Block until a message is received
            case Transport_Channel.Has_Data is
               when 1 =>
                  Run_Responder_1(Maincontexts(1));
               when 2 =>
                  Run_Responder_2(Maincontexts(2));
            end case;
         end loop;

         pragma assert(Sr.Initialized(Maincontexts(1)));
         pragma assert(Sr.Initialized(Maincontexts(2)));

         for I in Mainindex loop
            if not Sr.Active(Maincontexts(I)) then
               Sr.Finalize(Maincontexts(I));
               Sr.Initialize(Maincontexts(I));
            end if;
         end loop;
      end loop;

   end Main;
end Spdm_Responder;

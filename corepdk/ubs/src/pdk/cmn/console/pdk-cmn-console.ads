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
package Pdk.Cmn.Console is

   -- Ada API

   --- Increasing levels of verbosity.
   type Level is (None, Fatal, Error, Warning, Debug, Info);

   --- Only messages less than or equal to this level will be shown.
   Verbosity: Level := Info;

   --- Print out a fatal message to the console and terminate the program.
   --- @param Msg    Message to be sent to the console.
   procedure Fatal(Msg: in String);

   --- Print out a non-fatal error condition to the console.
   --- @param Msg    Message to be sent to the console.
   procedure Error(Msg: in String);

   --- Print out a warning condition to the console.
   --- @param Msg    Message to be sent to the console.
   procedure Warning(Msg: in String);

   --- Print out a debug message to the console.
   --- @param Msg    Message to be sent to the console.
   procedure Debug(Msg: in String);

   --- Print out a verbose information message to the console.
   --- @param Msg    Message to be sent to the console.
   procedure Info(Msg: in String);

end;

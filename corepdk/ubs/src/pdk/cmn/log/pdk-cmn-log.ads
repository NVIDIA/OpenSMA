------------------------------------------------------------------------------------------------
--                 Copyright (c) 2024, NVIDIA Corporation.  All Rights Reserved.              --
------------------------------------------------------------------------------------------------
--   NVIDIA Corporation and its licensors retain all intellectual property and proprietary    --
--   rights in and to this software and related documentation.  Any use, reproduction,        --
--   disclosure or distribution of this software and related documentation without an         --
--   express license agreement from NVIDIA Corporation is strictly prohibited.                --
------------------------------------------------------------------------------------------------

package Pdk.Cmn.Log is
   -- Ada API

   --- Status mirroring C++ implementation
   type Status is (Ok, Timeout, Truncated, Fatal, Unknown);

   --- Increasing levels of verbosity.
   type Level is (None, Fatal, Error, Warn, Debug, Info);

   --- Only messages less than or equal to this level will be shown.
   Verbosity: Level := Info;

   --- Print out a fatal message to the console and terminate the program.
   --- @param Msg    Message to be sent to the console.
   function Fatal(Msg: in String) return Status;

   --- Print out a non-fatal error condition to the console.
   --- @param Msg    Message to be sent to the console.
   function Error(Msg: in String) return Status;

   --- Print out a warning condition to the console.
   --- @param Msg    Message to be sent to the console.
   function Warn(Msg: in String) return Status;

   --- Print out a debug message to the console.
   --- @param Msg    Message to be sent to the console.
   function Debug(Msg: in String) return Status;

   --- Print out a verbose information message to the console.
   --- @param Msg    Message to be sent to the console.
   function Info(Msg: in String) return Status;

end;

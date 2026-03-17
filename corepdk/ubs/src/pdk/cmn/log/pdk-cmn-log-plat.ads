------------------------------------------------------------------------------------------------
--                 Copyright (c) 2024, NVIDIA Corporation.  All Rights Reserved.              --
------------------------------------------------------------------------------------------------
--   NVIDIA Corporation and its licensors retain all intellectual property and proprietary    --
--   rights in and to this software and related documentation.  Any use, reproduction,        --
--   disclosure or distribution of this software and related documentation without an         --
--   express license agreement from NVIDIA Corporation is strictly prohibited.                --
------------------------------------------------------------------------------------------------

package Pdk.Cmn.Log.Plat is
   function Print(Lvl: Level; Msg: String) return Status;
   function Print_P(Lvl: Level; Msg: String) return Status;

   procedure Print(Msg: String);
   procedure Putchar(C: Character);
   procedure Flush;
end;

------------------------------------------------------------------------------------------------
--                 Copyright (c) 2024, NVIDIA Corporation.  All Rights Reserved.              --
------------------------------------------------------------------------------------------------
--   NVIDIA Corporation and its licensors retain all intellectual property and proprietary    --
--   rights in and to this software and related documentation.  Any use, reproduction,        --
--   disclosure or distribution of this software and related documentation without an         --
--   express license agreement from NVIDIA Corporation is strictly prohibited.                --
------------------------------------------------------------------------------------------------

with Pdk.Cmn.Log.Plat;

package body Pdk.Cmn.Log is
   -- New_Line: constant Character := Character'Val(10);

   function Fatal(Msg: String) return Status is
   begin
      return Plat.Print(Fatal, Msg);
   end;

   function Error(Msg: String) return Status is
   begin
      return Plat.Print(Error, Msg);
   end;

   function Warn(Msg: String) return Status is
   begin
      return Plat.Print(Warn, Msg);
   end;

   function Debug(Msg: in String) return Status is
   begin
      return Plat.Print(Debug, Msg);
   end;

   function Info(Msg: in String) return Status is
   begin
      return Plat.Print(Info, Msg);
   end;

end;

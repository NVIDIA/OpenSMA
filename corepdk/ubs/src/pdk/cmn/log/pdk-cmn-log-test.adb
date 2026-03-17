------------------------------------------------------------------------------------------------
--                 Copyright (c) 2024, NVIDIA Corporation.  All Rights Reserved.              --
------------------------------------------------------------------------------------------------
--   NVIDIA Corporation and its licensors retain all intellectual property and proprietary    --
--   rights in and to this software and related documentation.  Any use, reproduction,        --
--   disclosure or distribution of this software and related documentation without an         --
--   express license agreement from NVIDIA Corporation is strictly prohibited.                --
------------------------------------------------------------------------------------------------
with Ubs.Unittest.Assert; use Ubs.Unittest;

package body Pdk.Cmn.Log.Test is
   procedure Consoleout is
      Result: Status;
   begin
      Result := Debug("ada debug");
      Assert.Is_True(Result = Ok);
      Result := Warn("ada warn");
      Assert.Is_True(Result = Ok);
      Result := Error("ada error");
      Assert.Is_True(Result = Ok);
      Result := Fatal("ada fatal");
      Assert.Is_True(Result = Ok);
   end;
begin
   Consoleout;
end;

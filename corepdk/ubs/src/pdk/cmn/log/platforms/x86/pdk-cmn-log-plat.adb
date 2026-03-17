------------------------------------------------------------------------------------------------
--                 Copyright (c) 2024, NVIDIA Corporation.  All Rights Reserved.              --
------------------------------------------------------------------------------------------------
--   NVIDIA Corporation and its licensors retain all intellectual property and proprietary    --
--   rights in and to this software and related documentation.  Any use, reproduction,        --
--   disclosure or distribution of this software and related documentation without an         --
--   express license agreement from NVIDIA Corporation is strictly prohibited.                --
------------------------------------------------------------------------------------------------

with Interfaces.C;

package body Pdk.Cmn.Log.Plat is

   use Interfaces.C;

   -- imports the C++
   function C_Log(Lvl: Int; Msg: Interfaces.C.Char_Array) return Int with
     import => True, convention => C, external_name => "ada_log";

   function C_Log_P(Lvl: Int; Msg: Interfaces.C.Char_Array) return Int with
     import => True, convention => C, external_name => "ada_log_p";

   procedure C_Log_Putc(Ch: Interfaces.C.Int) with
     import => True, convention => C, external_name => "ada_log_putc";

   procedure C_Log_Print(Msg: Interfaces.C.Char_Array) with
     import => True, convention => C, external_name => "ada_log_puts";

   procedure C_Log_Flush with
     import => True, convention => C, external_name => "ada_log_flush";

   function Print(Lvl: Level; Msg: String) return Status is
   begin
      return Status'Val(Integer(C_Log(Level'Pos(Lvl), Interfaces.C.To_C(Msg))));
   end;

   function Print_P(Lvl: Level; Msg: String) return Status is
   begin
      return Status'Val(Integer(C_Log_P(Level'Pos(Lvl), Interfaces.C.To_C(Msg))));
   end;

   procedure Print(Msg: String) is
   begin
      C_Log_Print(Interfaces.C.To_C(Msg));
   end;

   procedure Putchar(C: Character) is
   begin
      C_Log_Putc(Interfaces.C.Int(Character'Pos(C)));
   end;

   procedure Flush is
   begin
      C_Log_Flush;
   end;

end;

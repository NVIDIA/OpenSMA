/*
** ###################################################################
**     Linker script for separate diag test build
**     This produces a position-independent binary that can be loaded to RAM
**
**     Copyright 2025 NVIDIA CORPORATION & AFFILIATES.
**     SPDX-License-Identifier: Apache-2.0
** ###################################################################
*/

/* Entry Point */
ENTRY(handle_test)

/* This is a RAM-only build - no heap/stack needed */
/* The binary will be loaded to RAM at __diag_ram_start by the main firmware (m_diag region) */

/* Specify the memory areas - this matches m_diag from main linker script */
MEMORY
{
  m_ram_exec (RWX) : ORIGIN = 0x30060000, LENGTH = 0x00004000 /* 16KB for diag tests */
}

/* Define output sections */
SECTIONS
{
  /* All code goes into executable RAM section */
  .text :
  {
    . = ALIGN(4);
    *(.text)                 /* .text sections (code) */
    *(.text*)                /* .text* sections (code) */
    *(.rodata)               /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)              /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } > m_ram_exec

  /* C++ constructors/destructors (if any) */
  .init_array :
  {
    . = ALIGN(4);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    . = ALIGN(4);
  } > m_ram_exec

  .fini_array :
  {
    . = ALIGN(4);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    . = ALIGN(4);
  } > m_ram_exec

  /* Data section */
  .data :
  {
    . = ALIGN(4);
    *(.data)                 /* .data sections */
    *(.data*)                /* .data* sections */
    . = ALIGN(4);
  } > m_ram_exec

  /* BSS section */
  .bss (NOLOAD):
  {
    . = ALIGN(4);
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
  } > m_ram_exec

  /* Remove unwanted sections */
  /DISCARD/ :
  {
    *(.ARM.exidx*)
    *(.ARM.extab*)
    *(.eh_frame)
    *(.comment)
    *(.note*)
  }
}


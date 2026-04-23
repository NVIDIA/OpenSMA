/*
** ###################################################################
**     Processors:          MCXN556SCDF_cm33_core0
**
**     Compiler:            GNU C Compiler
**     Reference manual:    MCXNx4x Reference Manual
**     Version:             rev. 1.0, 2021-08-03
**     Build:               b250703
**
**     Abstract:
**         Linker file for the GNU C Compiler
**
**     Copyright 2016 Freescale Semiconductor, Inc.
**     Copyright 2016-2025 NXP
**     SPDX-License-Identifier: BSD-3-Clause
**
**     http:                 www.nxp.com
**     mail:                 support@nxp.com
**
** ###################################################################
*/

INCLUDE "src/projects/mcxn547helloworld/core0/sys/mcxn556/generated_core1.ld"

/* Entry Point */
ENTRY(Reset_Handler)

HEAP_SIZE  = DEFINED(__heap_size__)  ? __heap_size__  : 0x0400;
STACK_SIZE = DEFINED(__stack_size__) ? __stack_size__ : 0x0C00;

__core1_ram_start__ = CORE1_RAM_START + 0x10000000;
__core1_ram_end__ = CORE1_RAM_END + 0x10000000;

__core1_text_start__ = CORE1_TEXT_START + 0x10000000;
__core1_text_end__ = CORE1_TEXT_END + 0x10000000;

/* Specify the memory areas */
MEMORY
{
  m_interrupts          (RX)  : ORIGIN = 0x10000000, LENGTH = 0x000002B0 // 688
  m_header              (RW)  : ORIGIN = 0x100002B0, LENGTH = 0x00000150 // 336
  m_text                (RX)  : ORIGIN = 0x10000400, LENGTH = 0x00060000
  m_FMC                 (RW)  : ORIGIN = 0x30000000, LENGTH = 0x00002300 // 8960
  m_MailBox             (RW)  : ORIGIN = 0x30002300, LENGTH = 0x00000100 // 256
  m_data                (RW)  : ORIGIN = 0x30002400, LENGTH = 0x0005DC00
  m_diag                (RWX) : ORIGIN = 0x30060000, LENGTH = 0x00004000 // 16KB for diag tests
  m_sramx               (RW)  : ORIGIN = 0x14000000, LENGTH = 0x00018000
}

/* Define output sections */
SECTIONS
{
  /* The startup code goes first into internal flash */
  .interrupts :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))     /* Startup code */
    . = ALIGN(4);
  } > m_interrupts

  .header :
  {
    KEEP(*(.header))
  } > m_header

  .core1_image :
  {
    . = ALIGN(32);
    __core1_image_start__ = .;
    KEEP(*(.core1_image))
    . = ALIGN(32);
    __core1_image_end__ = .;
  } > m_text

  /* The program code and other data goes into internal flash */
  .rodata_text :
  {
    . = ALIGN(4);

    . = ALIGN(32);
    __unprivileged_flash_start__ = .;
     __rodata_start__ = .;
    *(.rodata)               /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)              /* .rodata* sections (constants, strings, etc.) */
     __rodata_end__ = .;
  } > m_text

  /* Ensure rodata crosses 0x8000 AHB flash block boundary */
  Rodata_Block        = (__rodata_end__ + 1) / 0x8000;
  Core1_Block     = (__core1_image_end__ - 1) / 0x8000;
  Rodata_Align_Size = (Rodata_Block > Core1_Block ) ? 0x4 : 0x8000;

  .text :
  {
    . = ALIGN(Rodata_Align_Size);
    *(.text)                 /* .text sections (code) */
    *(.text*)                /* .text* sections (code) */
    *(.glue_7)               /* glue arm to thumb code */
    *(.glue_7t)              /* glue thumb to arm code */
    *(.eh_frame)            /* exception support */
    KEEP (*(.init))
    KEEP (*(.fini))
    *(.data..Lsrc_*)        /* file name/ function name/ function line */
    . = ALIGN(32);
    __unprivileged_flash_end__ = .;
    . =  . + 32;

    __privileged_functions_start__ = .;
    *(privileged_functions)
    *(privileged_functions*)
    . = ALIGN(32);
    __privileged_functions_end__ = .;
    . =  . + 32;

    __syscalls_flash_start__ = .;
    *(freertos_system_calls)
    *(freertos_system_calls*)
    *(system_calls)
    *(system_calls*)
    . = ALIGN(32);
    __syscalls_flash_end__ = .;
    . =  . + 32;
  } > m_text

  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } > m_text

  .ARM :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } > m_text

 .ctors :
  {
    __CTOR_LIST__ = .;
    /* gcc uses crtbegin.o to find the start of
       the constructors, so we make sure it is
       first.  Because this is a wildcard, it
       doesn't matter if the user does not
       actually link against crtbegin.o; the
       linker won't look for a file to match a
       wildcard.  The wildcard also means that it
       doesn't matter which directory crtbegin.o
       is in.  */
    KEEP (*crtbegin.o(.ctors))
    KEEP (*crtbegin?.o(.ctors))
    /* We don't want to include the .ctor section from
       from the crtend.o file until after the sorted ctors.
       The .ctor section from the crtend file contains the
       end of ctors marker and it must be last */
    KEEP (*(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    __CTOR_END__ = .;
  } > m_text

  .dtors :
  {
    __DTOR_LIST__ = .;
    KEEP (*crtbegin.o(.dtors))
    KEEP (*crtbegin?.o(.dtors))
    KEEP (*(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    __DTOR_END__ = .;
  } > m_text

  .preinit_array :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } > m_text

  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } > m_text

  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } > m_text

  __etext = .;    /* define a global symbol at end of code */
  __DATA_ROM = .; /* Symbol is used by startup for data initialization */

  .core1_ram :
  {
    . = ABSOLUTE(__core1_ram_end__);
  } > m_data

  .data : AT(__DATA_ROM)
  {
    . = ALIGN(4);
    __DATA_RAM = .;

    __data_start__ = .;      /* create a global symbol at data start */
    . = ALIGN(32);
    __privileged_sram_start__ = .;
    *(privileged_data*)
    . = ALIGN(32);
    __privileged_sram_end__ = .;
    . =  . + 32;

    *(.ramfunc*)             /* for functions in ram */
    *(.data)                 /* .data sections */
    /* .data* sections */
    EXCLUDE_FILE (*middleware/multicore/mcmgr/src/mcmgr_internal_core_api_mcxnx4x.c.o *middleware/multicore/mcmgr/src/mcmgr.c.o *middleware/mbedtls/library/platform.c.o *middleware/mbedtls/port/pkc/els_pkc_mbedtls.c.o *middleware/mbedtls/port/els/cbc_mac_alt.c.o *middleware/mbedtls/library/memory_buffer_alloc.c.o) *(.data*)
    *(NonCacheable.init)     /* NonCacheable init section */
    *(NonCacheable)          /* NonCacheable section */
    *(CodeQuickAccess)       /* quick access code section */
    *(DataQuickAccess)       /* quick access data section */
    KEEP(*(.jcr*))

    . = ALIGN(32);
    nv_linker_var_shared_ram_start = .;
    *(shared_data*)
    *middleware/multicore/mcmgr/src/mcmgr_internal_core_api_mcxnx4x.c.o(.data*)
    *middleware/multicore/mcmgr/src/mcmgr.c.o(.data*)
    *middleware/mbedtls/library/memory_buffer_alloc.c.o(.data*)
    *middleware/mbedtls/library/platform.c.o(.data*)
    *middleware/mbedtls/port/pkc/els_pkc_mbedtls.c.o(.data*)
    *middleware/mbedtls/port/els/cbc_mac_alt.c.o(.data*)

    . = ALIGN(4);
    __data_end__ = .;        /* define a global symbol at data end */
  } > m_data

  __DATA_END = __DATA_ROM + (__data_end__ - __data_start__);

  __SRAMX_FLASH_START = __DATA_END;

  .core1_image_sram :
  {
    . = ALIGN(32);
    __core1_image_sram_start__ = .;
    . = . + (__core1_image_end__ - __core1_image_start__);
    . = ALIGN(32);
    __core1_image_sram_end__ = .;
  } > m_sramx

  .sramx_code : AT(__SRAMX_FLASH_START)
  {
    . = ALIGN(32);
    __sramx_code_start__ = .;
    *(sramx_code)
    *(.code.$SRAMX_SMARTDMA)
    *(SRAMX_SMARTDMA)
    *(.data.$SRAMX_SMARTDMA)
    . = ALIGN(32);
    __sramx_code_end__ = .;
  } > m_sramx

  __SRAMX_FLASH_END = __DATA_END + (__sramx_code_end__ - __sramx_code_start__);

  text_end = ORIGIN(m_text) + LENGTH(m_text);
  PROVIDE(__text_size = __etext - ORIGIN(m_text));
  
  ASSERT(__SRAMX_FLASH_END <= text_end, "region m_text overflowed with text, data and sramx code")

  /* Uninitialized data section */
  .bss (NOLOAD):
  {
    /* This is used by the startup in order to initialize the .bss section */
    . = ALIGN(4);
    __START_BSS = .;
    __bss_start__ = .;
    *(shared_bss*)
    *middleware/multicore/mcmgr/src/mcmgr_internal_core_api_mcxnx4x.c.o(.bss*)
    *middleware/multicore/mcmgr/src/mcmgr.c.o(.bss*)
    *middleware/usb/device/class/usb_device_hid.c.o(.bss*)
    *middleware/usb/device/usb_device_dci.c.o(.bss*)
    *middleware/usb/device/usb_device_ehci.c.o(.bss*)
    *middleware/mbedtls/library/memory_buffer_alloc.c.o(.bss*)
    *middleware/mbedtls/library/platform.c.o(.bss*)
    *middleware/mbedtls/port/pkc/els_pkc_mbedtls.c.o(.bss*)
    *middleware/mbedtls/port/els/cbc_mac_alt.c.o(.bss*)
    *devices/MCXN556S/drivers/fsl_i3c.c.o(.bss*)
    *devices/MCXN556S/drivers/fsl_i3c_edma.c.o(.bss*)
    *devices/MCXN556S/drivers/fsl_lpspi.c.o(.bss*)
    *(*_ZGVZN*)
    *(*s_debugConsole*)   /* WAR for not changing SDK*/
    . = ALIGN(32);
    nv_linker_var_shared_ram_end = .;

    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(32);
    EXCLUDE_FILE (*nv/diag/task.cpp.o) *(stack*);
    EXCLUDE_FILE (*nv/diag/task.cpp.o) *(task_data*);

    . = ALIGN(0x1000);           /* Align to 4KB */
    __shared_memory_start__ = .;
    *(.c2c_memory_data)
    . = ALIGN(0x1000);           /* Align to 4KB */
    __shared_memory_end__ = .;
    
    __bss_end__ = .;
    __END_BSS = .;
  } > m_data

 /* Diag task stack/data in SRAMX to reduce m_data pressure */
  .bss_2 (NOLOAD):
  {
    . = ALIGN(32);
    __bss_start_2__ = .;
    *nv/diag/task.cpp.o(stack*);
    *nv/diag/task.cpp.o(task_data*);
    . = ALIGN(32);
    __bss_end_2__ = .;
    __END_BSS_2 = .;
  } > m_sramx

#ifdef NV_COVERAGE
  .gcov_info      :
  {
    PROVIDE (__gcov_info_start = .);
    KEEP (*(.gcov_info))
    PROVIDE (__gcov_info_end = .);
  } > m_data
#endif

  /* Diagnostic test function RAM area */
  .diag_ram (NOLOAD) :
  {
    . = ALIGN(4);
    __diag_ram_start = .;
    . = . + LENGTH(m_diag);
  } > m_diag

  .heap :
  {
    . = ALIGN(8);
    __end__ = .;
    PROVIDE(end = .);
    PROVIDE(__heap_start = .);
    __HeapBase = .;
    . += HEAP_SIZE;
    __HeapLimit = .;
    __heap_limit = .; /* Add for _sbrk */
    PROVIDE(__heap_end = .);
  } > m_data

  .stack :
  {
    . = ALIGN(8);
    . += STACK_SIZE;
  } > m_data

  __edata = .;    /* define a global symbol at end of data */
  PROVIDE(__data_size = __edata - ORIGIN(m_data) + LENGTH(m_FMC));

  /* Initializes stack on the end of block */
  __StackTop   = ORIGIN(m_data) + LENGTH(m_data);
  __StackLimit = __StackTop - STACK_SIZE;
  PROVIDE(__stack = __StackTop);

  .ARM.attributes 0 : { *(.ARM.attributes) }

  PROVIDE(__max_fw_size = LENGTH(m_text));
  PROVIDE(__max_ram_size = LENGTH(m_FMC) + LENGTH(m_data));
  PROVIDE(__diag_ram_length = LENGTH(m_diag));

  ASSERT(__StackLimit >= __HeapLimit, "region m_data overflowed with stack and heap")
}


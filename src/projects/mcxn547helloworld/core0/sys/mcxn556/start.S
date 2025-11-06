/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

    .arch armv8-m.main
	.syntax unified
	.text
	.thumb

// -- Macros -----------------------------------------------------------------------------------
.macro FUNC_START name
	.global \name
	.thumb_func
\name:
.endm
		
.macro FUNC_EPILOG
.endm



// -- Stack Init -------------------------------------------------------------------------------
	.align	0
	FUNC_START	_stack_init

	sub	sl, r3, #64 << 10

    bx lr	


// -- _start -----------------------------------------------------------------------------------
	.align 	0
	FUNC_START	_mainCRTStartup
	FUNC_START	_start
	ldr	r3, .Lstack
	cmp	r3, #0
	it	eq
	ldreq	r3, .LC0
	bl _stack_init

    // memset bss section
	movs 	a2, #0      // value
	mov	fp, a2			// null fp
	mov	r7, a2			// null fp for thumb
	ldr	a1, .Lbssstart  // __bss_start__
	ldr	a3, .Lbssend    // __bss_end__
	subs	a3, a3, a1  // len
	bl	memset

    // call hardware_init_hook if found
	ldr	r3, .Lhwinit
	cmp	r3, #0
	beq	.LC24
	blx r3
.LC24:	
    // call software_init_hook if found
	ldr	r3, .Lswinit
	cmp	r3, #0
	beq	.LC25
	blx r3

.LC25:	
	movs	r0, #0	// argc
	movs	r1, #0  // argv
	movs	r4, r0
	movs	r5, r1
	ldr	r0, .Latexit
	cmp	r0, #0
	beq	.Lweak_atexit
	ldr	r0, .Lfini
.Lweak_atexit:
	bl	__libc_init_array
	movs	r0, r4
	movs	r1, r5
	bl	main
	bl	exit		/* Should not return.  */
	
// -- Data ------------------------------------------------------------------------------------
	.align 0
.LC0:
	.word	0x80000			/* Top of RAM on the PIE board.  */
.Lhwinit:	
	.word	hardware_init_hook
.Lswinit:
	.word	software_init_hook
.Lstack:
	.word	__stack
.Lbssstart:
	.word	__bss_start__
.Lbssend:
	.word	__bss_end__
.Latexit:
	.word	atexit
.Lfini:
	.word   __libc_fini_array

	.weak   hardware_init_hook 
	.weak   software_init_hook
	.weak   _stack_init
    .weak   __libc_fini_array
	.weak	atexit
	.weak	__stack

// -- _init/_fini prologues -------------------------------------------------------------------
	.eabi_attribute 25, 1
	.section	".init"
	.align 2
	.global	_init
	.thumb_func
	.type _init,function
_init:
	.thumb
	push	{r3, r4, r5, r6, r7, lr}
	
	.section	".fini"
	.align	2
	.global	_fini
	.thumb_func
	.type _fini,function
_fini:
	.thumb
	push	{r3, r4, r5, r6, r7, lr}
		
	
// -- _init / _fini epilogues -----------------------------------------------------------------
	.section	".init"
	.thumb
	
	pop	{r3, r4, r5, r6, r7}
	pop	{r3}
	mov	lr, r3
	bx	lr
	
	.section	".fini"
	.thumb
	
	pop	{r3, r4, r5, r6, r7}
	pop	{r3}
	mov	lr, r3
	bx	lr

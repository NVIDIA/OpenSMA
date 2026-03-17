/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

// Lightweight debug-console configuration used by low-level headers.
// Keep this header free of project includes.
#ifndef NV_UART_INSTANCE
#if defined(CPU_MCXN547VDF_cm33_core0) || defined(CPU_MCXN556SCDF_cm33_core0)
#define NV_UART_INSTANCE 0x04
#else
// Core1: no UART
#define NV_UART_INSTANCE 0xfe
#endif
#endif

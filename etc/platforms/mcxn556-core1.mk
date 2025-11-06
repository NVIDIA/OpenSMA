# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
# All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.




# -- global defines ----------------------------------------------------------------------------
GD_CPU_MCXN556SCDF            := 1
GD_MCUXPRESSO_SDK             := 1
GD_CPU_MCXN556SCDF_cm33_core1 := 1

# -- tools ------------------------------------------------------------------------------------
# Assign a MCU toolchain path
MCU_ARM_PATH := libexec/toolchain/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi
MCU_GNAT_PATH := libexec/toolchain/gnat_arm_elf_14.2.1_524d4d41/
MCU_ARM_LINKER_PATH := libexec/toolchain/gcc-arm-none-eabi-10.3-2021.10/bin
# need to export PATH here to pass system requirements check
export PATH:=$(PATH):$(MCU_GNAT_PATH)/bin:$(MCU_ARM_PATH)/bin

CHIP        ?= mcxn556
VERSION     ?= 01.0000.0000
AP_SKU_PLAT := 0x02000000
ARM_ARCH    := arm-none-eabi
GNAT_ARCH   := arm-eabi

CC			:= $(ARM_ARCH)-gcc
ASM			:= $(ARM_ARCH)-gcc
CPP			:= $(ARM_ARCH)-g++
CXX			:= $(ARM_ARCH)-g++
LD      	:= $(ARM_ARCH)-ld
# workaround for annoymous string issue in output.map
LINK 		:= $(MCU_ARM_LINKER_PATH)/$(ARM_ARCH)-g++
NM			:= $(ARM_ARCH)-nm
SIZE		:= $(ARM_ARCH)-size
OBJCOPY		:= $(ARM_ARCH)-objcopy
OBJDUMP		:= $(ARM_ARCH)-objdump
STRIP		:= $(ARM_ARCH)-strip
READELF		:= $(ARM_ARCH)-readelf

ADA 	    := $(GNAT_ARCH)-gcc
BIND 	    := $(GNAT_ARCH)-gnatbind

ADA_RTS 	:= light-cortex-m33f
ADA_TARGET  := arm-eabi
ADA_INSTALL := $(MCU_GNAT_PATH)/$(ADA_TARGET)/lib/gnat/$(ADA_RTS)/adalib/gnat
#ADA_LIBS 	:= $(MCU_GNAT_PATH)/$(ADA_TARGET)/lib/gnat/$(ADA_RTS)/adalib/libgnat.a

CC_FLAGS    += -Wno-implicit-function-declaration
ADA_FLAGS   += -gnatn -gnatwa -gnatwae
CXX_FLAGS   += -fno-exceptions -fno-rtti -fno-threadsafe-statics
CPP_FLAGS   += -E -P -x c

# -- flags ------------------------------------------------------------------------------------
#
PATH_ADARTS := $(MCU_GNAT_PATH)/$(ADA_TARGET)/lib/gnat/$(ADA_RTS)/

MCU_COMPILE_FLAGS := -mcpu=cortex-m33+nodsp -mthumb -mapcs -mfloat-abi=soft
MCU_COMPILE_FLAGS += -D__NEWLIB__ -DMCU

# For tidy support c++23 feature
ifneq (,$(filter tidy,$(MAKECMDGOALS)))
	MCU_COMPILE_FLAGS += -D__cpp_concepts=202002L -Wno-builtin-macro-redefined
endif

SYSTEM_INCLUDES +=  \
	$(UBS_PATH_SRC)/overrides \
	$(PATH_SDK)/rtos/freertos/freertos-kernel/portable/GCC/ARM_CM33_NTZ/non_secure

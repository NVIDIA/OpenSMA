# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
# MCXN557 (2 MB flash) is built against the MCXN556S SDK until an MCXN557 SDK has been validated,
# so the SDK CPU defines intentionally stay MCXN556SCDF.
GD_CPU_MCXN556SCDF            := 1
GD_MCUXPRESSO_SDK             := 1
GD_CPU_MCXN556SCDF_cm33_core1 := 1
# Identifies the MCXN557T (2 MB flash) part on top of the MCXN556 SDK defines.
GD_CPU_MCXN557T               := 1

# -- tools ------------------------------------------------------------------------------------
# Assign a MCU toolchain path
MCU_TOOLCHAIN_ROOT ?= /toolchain
MCU_ARM_PATH := $(MCU_TOOLCHAIN_ROOT)/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi
MCU_GNAT_PATH := $(MCU_TOOLCHAIN_ROOT)/gnat_arm_elf_14.2.1_524d4d41/
MCU_ARM_LINKER_PATH := $(MCU_TOOLCHAIN_ROOT)/gcc-arm-none-eabi-10.3-2021.10/bin
# need to export PATH here to pass system requirements check
export PATH:=$(PATH):$(MCU_GNAT_PATH)/bin:$(MCU_ARM_PATH)/bin

CHIP        ?= mcxn557
VERSION     ?= 02.0000.0000
AP_SKU_PLAT := 0x03000000
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

include etc/platforms/linker-memory-usage.mk

ADA 	    := $(GNAT_ARCH)-gcc
BIND 	    := $(GNAT_ARCH)-gnatbind

ADA_RTS 	:= light-cortex-m33f
ADA_TARGET  := arm-eabi
ADA_INSTALL := $(MCU_GNAT_PATH)/$(ADA_TARGET)/lib/gnat/$(ADA_RTS)/adalib/gnat
#ADA_LIBS 	:= $(MCU_GNAT_PATH)/$(ADA_TARGET)/lib/gnat/$(ADA_RTS)/adalib/libgnat.a

CC_FLAGS    += -Wno-implicit-function-declaration -fdump-rtl-expand -fstack-usage
ADA_FLAGS   += -gnatn -gnatwa -gnatwae
# -fdump-ipa-devirt: emit each translation unit's class-hierarchy/virtual-call-target
# info so the stack-analysis tooling (libexec/cicd/get-heaviest-stack-path.py) can
# eventually resolve virtual dispatch instead of treating it as an invisible blind
# spot. C-only, so this only applies to CXX_FLAGS.
CXX_FLAGS   += -fno-exceptions -fno-rtti -fno-threadsafe-statics -fdump-rtl-expand -fstack-usage -fdump-ipa-devirt
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

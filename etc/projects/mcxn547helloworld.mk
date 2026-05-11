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



-include etc/features.mk

DATE				:= date
JQ					:= jq
BOARD 			    ?= default
PLATFORM_BASE       := $(firstword $(subst -, ,$(PLATFORM)))
CORE                := $(lastword $(subst -, ,$(PLATFORM)))
ifeq ($(PLATFORM_BASE), mcxn236)
PATH_SYS            = $(UBS_PATH_SRC)/sys/mcxn236
PATH_SDK 			?= libexec/sdk/SDK_25_03_00_FRDM-MCXN236
PATH_SDK_DEVICE  	:= $(PATH_SDK)/devices/MCXN236
else ifeq ($(PLATFORM_BASE), mcxn547)
PATH_SYS            = $(UBS_PATH_SRC)/sys/mcxn556
PATH_SDK 			?= libexec/sdk/SDK_25_03_00_MCX-N5XX-EVK
PATH_SDK_DEVICE  	:= $(PATH_SDK)/devices/MCXN547
else ifeq ($(PLATFORM_BASE), mcxn556)
PATH_SYS            = $(UBS_PATH_SRC)/sys/mcxn556
PATH_SDK 			?= libexec/sdk/SDK_25_09_00_MCXN556S
PATH_SDK_DEVICE  	:= $(PATH_SDK)/devices/MCXN556S
endif
PATH_SDK_COMPONENTS := $(PATH_SDK)/components
PATH_FREERTOS 		:= $(PATH_SDK)/rtos/freertos/freertos-kernel
PATH_BOARD 	 		:= $(UBS_PATH_SRC)/boards/$(BOARD)
PATH_PROJECTS 		:= $(UBS_PATH_SRC)/projects
PATH_PROJECT   		:= $(PATH_PROJECTS)/$(PROJECT)/$(CORE)
PATH_PROJECT_SYS  	:= $(PATH_PROJECT)/sys/$(PLATFORM_BASE)


SRCS_ALL := $(shell find $(UBS_PATH_SRC)/nv -name *.cpp -not -name '*-test.cpp' -not -name 'handle_test.cpp') 
SRCS_ALL += $(shell find $(PATH_SYS) -name *.cpp -not -name '*-test.cpp')
SRCS_ALL += $(filter-out $(wildcard $(PATH_PROJECT)/*-test.cpp), $(wildcard $(PATH_PROJECT)/*.cpp))
SRCS_ALL += $(filter-out $(wildcard $(PATH_PROJECT_SYS)/*-test.cpp), $(wildcard $(PATH_PROJECT_SYS)/*.cpp))
SRCS_ALL += $(filter-out $(wildcard $(UBS_PATH_SRC)/overrides/*-test.cpp), $(wildcard $(UBS_PATH_SRC)/overrides/*.cpp))
ADAS_ALL := $(shell find $(UBS_PATH_SRC)/nv -name *.adb -or -name *.ads -not -name '*-test.adb' -not -name '*-test.ads')
ADAS_ALL += $(shell find $(PATH_SYS)/sys -name *.adb -or -name *.ads -not -name '*-test.adb' -not -name '*-test.ads')

FREERTOS_SRC    := \
	$(PATH_FREERTOS)/croutine.c \
	$(PATH_FREERTOS)/event_groups.c \
	$(PATH_FREERTOS)/list.c \
	$(PATH_FREERTOS)/queue.c \
	$(PATH_FREERTOS)/stream_buffer.c \
	$(PATH_FREERTOS)/tasks.c \
	$(PATH_FREERTOS)/timers.c \
	$(PATH_FREERTOS)/portable/Common/mpu_wrappers_v2.c \
# ----------------------------------------------------------------------------------------------

# -- helper functions -------------------------------------------------------------------------
nvtr        = $(shell echo "$3" | sed -re 's/[$1]/$2/g')
nvcalc 	    = $(strip $(shell echo "$$(( $1 ))"))
nvrest      = $(wordlist 2,$(words $1),$1)
nvmap       = $(strip $(foreach i,$2,$(call $1,$i)))
__nvpos     = $(if $(findstring $1,$2),$(call $0,$1,$(call nvrest,$2)),$(words $2))
nvpos       = $(if $(findstring $(strip $1),$2),$(call nvcalc,$(words $2) - $(call __nvpos,$(strip $1),$2) - 1),$(error invalid index))

# the main entry point file, can be a .c .cpp or .adb
MAIN 				:= $(PATH_PROJECT)/main.cpp
TARGET_NAME			:= $(PROJECT).elf

# -- global defines ----------------------------------------------------------------------------
AP_SKU_PRJ          := 0x4

GD_NV_DEBUG 		:= 1
GDS_NV_IPC_CONFIG_H := $(PROJECT)/config.h
GDS_NV_REG_CONFIG_H := $(PROJECT)/$(CORE)/regconfig.h
GD_MCU_FW_MAJOR 	:= 19
GD_MCU_FW_MINOR 	:= $(shell echo $(VERSION) | $(UBS_AWK) -F . '{printf "%d", $$1}')
GD_MCU_FW_PATCH 	:= $(shell echo $(VERSION) | $(UBS_AWK) -F . '{printf "%d", $$2}')
GD_MCU_FW_BUILD 	:= $(shell echo $(VERSION) | $(UBS_AWK) -F . '{printf "%d", $$3}')
GD_NV_BUILD_MODE    = $(call nvpos,$(UBS_MODE),rel dev dbg)
GD_MCU_AP_SKU       := $(shell echo $$(( $(AP_SKU_PLAT) | $(AP_SKU_PRJ) )))
GD_MCU_SSDID		:= 0x21ab

FW_VERSION          = $(shell printf "%04d.%02d.%04d.%04d" $(GD_MCU_FW_MAJOR) $(GD_MCU_FW_MINOR) $(GD_MCU_FW_PATCH) $(GD_MCU_FW_BUILD))
COMP_STAMP          = $(shell printf "0x%02x%04x%02x" $(GD_MCU_FW_MINOR) $(GD_MCU_FW_PATCH) $$(($(GD_MCU_FW_BUILD)&0xFF)))
AP_SKU              = $(shell printf "0x%x" $(GD_MCU_AP_SKU))
SSDID               = $(shell printf "0x%x" $(GD_MCU_SSDID))
SIGN_KEYSET         ?= S1

GD_MCU_FW_MONTH = $(shell $(DATE) -u +"%-m")
GD_MCU_FW_DAY   = $(shell $(DATE) -u +"%-d")
GD_MCU_FW_YEAR  = $(shell $(DATE) -u +"%-Y")

# -- includes ----------------------------------------------------------------------------------
INCLUDES += $(PATH_SYS) $(PATH_PROJECTS) $(PATH_PROJECT) $(PATH_PROJECT_SYS)
INCLUDES += $(PATH_SDK)/rtos/freertos/freertos-kernel/include

GLOBAL_DEDINES ?= -DDEBUG -fno-common -ffunction-sections -fdata-sections 

# -- flags -------------------------------------------------------------------------------------
CC_FLAGS    += -ffreestanding -DNV_MCTP_USB_CLASS \
                 -DNV_IPC_CONFIG_H=\"$(GDS_NV_IPC_CONFIG_H)\" -DNV_REG_CONFIG_H=\"$(GDS_NV_REG_CONFIG_H)\" \
				 $(GLOBAL_DEDINES) -D__STARTUP_CLEAR_BSS
CXX_FLAGS   += -ffreestanding -DNV_IPC_CONFIG_H=\"$(GDS_NV_IPC_CONFIG_H)\" -DNV_REG_CONFIG_H=\"$(GDS_NV_REG_CONFIG_H)\" \
			     $(GLOBAL_DEDINES)
ADA_FLAGS   += $(GLOBAL_DEDINES)

CC_FLAGS    += -fno-builtin
CXX_FLAGS 	+= -fno-builtin

# We should fix up -Wmissing-field-initializers warnings and remove
#  -Wno-missing-field-initializers & -Wno-unused-parameter
# flags that provide free security features from GCC. Other notes:
#   -fstack-protector-all breaks IRQs; use stack-protector-strong instead
#   -D_FORTIFY_SOURCE=2 is not supported by this project's libc, we need to find an equivalent
CC_FLAGS += -Wall -Wextra \
    -Wno-missing-field-initializers \
    -Wno-unused-parameter \
    -D_FORTIFY_SOURCE=2 \
    -fstack-clash-protection \
    -fcf-protection=check \
    -fbounds-check \
    -fstack-protector-strong -Wstack-protector --param=ssp-buffer-size=4 \
    -ftrivial-auto-var-init=zero
CXX_FLAGS += -Wall -Wextra \
    -Wno-missing-field-initializers \
    -Wno-unused-parameter \
    -D_FORTIFY_SOURCE=2 \
    -fstack-clash-protection \
    -fcf-protection=check \
    -fbounds-check \
    -fstack-protector-strong -Wstack-protector --param=ssp-buffer-size=4 \
    -ftrivial-auto-var-init=zero

CC_FLAGS_dev  := -g -Os
CC_FLAGS_rel  := -Os
CXX_FLAGS_dev := -g -Os
CXX_FLAGS_rel := -Os
ADA_FLAGS_dev := -g -Os
ADA_FLAGS_rel := -Os

# -- sources -----------------------------------------------------------------------------------
define nvsrcfiles
$(foreach DIR,$1, \
	$(shell [ -d $(PATH_SYS)/sys/$(DIR) ] && find $(PATH_SYS)/sys/$(DIR) \( -name "*.adb" -or -name "*.ads" -or -name "*.cpp" \) -not -name "*-test.adb" -not -name "*-test.ads" -not -name "*-test.cpp" -not -name "handle_test.cpp") \
	$(shell [ -d $(UBS_PATH_SRC)/nv/$(DIR) ] && find $(UBS_PATH_SRC)/nv/$(DIR) \( -name "*.adb" -or -name "*.ads" -or -name "*.cpp" \) -not -name "*-test.adb" -not -name "*-test.ads" -not -name "*-test.cpp" -not -name "handle_test.cpp") \
)
endef
MODULES := common ipc mctp i2c pldm usb flash bootloader gpio i3c logger ctimer spdm uart watchdog perf_mon ipchandler sensor debugtoken telemetry spi fw_parser crypto c2c_mailbox ap_operation fru smartdma lstp ssif diag vruart
SOURCES += $(FREERTOS_SRC) $(FREERTOS_SRC_ARCH) $(UBS_PATH_SRC)/nv/nv.ads $(PATH_PROJECT_SYS)/linker_script.ld
SOURCES += $(call nvsrcfiles,$(MODULES))

include etc/projects/nvheader.mk
SOURCES += $(NVHEADER_SRC)

include etc/projects/$(PROJECT)-$(PLATFORM).mk

include etc/projects/module-spdm.mk
include etc/projects/module-pldm-fd.mk
include etc/projects/plats-pldm-fd.mk
include etc/projects/ubs-cmn.mk
include etc/projects/module-mctp-cpp.mk

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


UBS_SRC_PATH := corepdk/ubs/src
PDK_SRC_PATH := $(UBS_SRC_PATH)/pdk
PDK_SRC_CMN_PATH := $(PDK_SRC_PATH)/cmn
INCLUDES += $(UBS_SRC_PATH) $(PDK_SRC_CMN_PATH)/flowcontrol $(PDK_SRC_CMN_PATH)/log
# Add PDK CMN Source Files excluding platforms and test files
SOURCES += $(shell find $(PDK_SRC_PATH) \
           \( -name "*.adb" -or -name "*.ads" -or -name "*.cpp" \) \
           -not -name "*-test.adb" -not -name "*-test.ads" -not -name "*-test.cpp" \
           -not -path "*/platforms/*" )
# highest level of debug logging: Info = 5
GD_PDK_CMN_LOG_DBG_LEVEL_CONSOLE	?= 5
GD_PDK_CMN_LOG_DBG_LEVEL_PERSISTENT ?= 5
GD_PDK_CMN_LOG_MAX_MSG_LENGTH       ?= 128
# use 12 bits footrprint
GD_PDK_CMN_LOG_SL_CONSOLE    := 1
GD_PDK_CMN_LOG_SL_PERSISTENT := 1

# Provide a lightweight per-project debug-console config header (next to NV_IPC_CONFIG_H)
# so low-level headers (e.g. nv/common/debuglevel.h) can pick up defines like NV_UART_INSTANCE.
#
# Example: if GDS_NV_IPC_CONFIG_H is "pk42w/config.h", this becomes "pk42w/DebugConsoleConfig.h".
GDS_NV_DEBUGCONSOLE_CONFIG_H ?= $(dir $(GDS_NV_IPC_CONFIG_H))DebugConsoleConfig.h
CC_FLAGS  += -DNV_DEBUGCONSOLE_CONFIG_H=\"$(GDS_NV_DEBUGCONSOLE_CONFIG_H)\"
CXX_FLAGS += -DNV_DEBUGCONSOLE_CONFIG_H=\"$(GDS_NV_DEBUGCONSOLE_CONFIG_H)\"

ifeq ($(PLATFORM), x86)
	SOURCES += $(PDK_SRC_CMN_PATH)/flowcontrol/platforms/x86/pdk-cmn-flowcontrol-plat.adb
	SOURCES += $(PDK_SRC_CMN_PATH)/log/platforms/x86/pdk-cmn-log-plat.adb
	SOURCES += $(PDK_SRC_CMN_PATH)/log/platforms/x86/log.cpp
	# Add INCLUDES
	INCLUDES += $(PDK_SRC_CMN_PATH)/log/platforms/x86
    INCLUDES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)
	# Global Define for Log Platform Types Header
	GD_PDK_CMN_LOG_PLATFORM_TYPES_H	:= "$(PDK_SRC_CMN_PATH)/log/platforms/x86/types.h"
else
  GD_PDK_CMN_LOG_GLOBAL		        := "shared_data_loglevel"
  ifeq ($(PLATFORM), mcxn236)
	PDK_CMN_PLATFORM_SPECIFIC_PATH := corepdk/platforms/mcxn236/cmn/src
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.cpp
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-log-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-log-plat.cpp
	GD_PDK_CMN_LOG_PLATFORM_TYPES_H	:= "$(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-log-plattypes.h"
	INCLUDES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)
# TODO: Add mcxn547 platform specific sources
  else ifeq ($(PLATFORM), mcxn547-core0)
	PDK_CMN_PLATFORM_SPECIFIC_PATH := corepdk/platforms/mcxn236/cmn/src
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.cpp
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-log-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-log-plat.cpp
	GD_PDK_CMN_LOG_PLATFORM_TYPES_H	:= "$(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-log-plattypes.h"
	INCLUDES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)
  else ifeq ($(PLATFORM), mcxn547-core1)
	PDK_CMN_PLATFORM_SPECIFIC_PATH := corepdk/platforms/mcxn236/cmn/src
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.cpp
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-log-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-log-plat.cpp
	GD_PDK_CMN_LOG_PLATFORM_TYPES_H	:= "$(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-log-plattypes.h"
	INCLUDES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)
# TODO: Add mcxn556 platform specific sources
  else ifeq ($(PLATFORM), mcxn556-core0)
	PDK_CMN_PLATFORM_SPECIFIC_PATH := corepdk/platforms/mcxn236/cmn/src
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.cpp
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-log-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-log-plat.cpp
	GD_PDK_CMN_LOG_PLATFORM_TYPES_H	:= "$(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-log-plattypes.h"
	INCLUDES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)
  else ifeq ($(PLATFORM), mcxn556-core1)
	PDK_CMN_PLATFORM_SPECIFIC_PATH := corepdk/platforms/mcxn236/cmn/src
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.cpp
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-log-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-log-plat.cpp
	GD_PDK_CMN_LOG_PLATFORM_TYPES_H	:= "$(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-log-plattypes.h"
	INCLUDES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)
  else
  	$(ubs-fatal) "Platform $(PLATFORM) is not supported"
  endif
endif
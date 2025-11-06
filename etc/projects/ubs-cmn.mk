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



PDK_SRC_PATH := corepdk/ubs/src/pdk
PDK_SRC_CMN_PATH := $(PDK_SRC_PATH)/cmn
INCLUDES += $(PDK_SRC_CMN_PATH)/console_cpp $(PDK_SRC_CMN_PATH)/flowcontrol $(PDK_SRC_CMN_PATH)/logger
# Add PDK CMN Source Files excluding platforms and test files
SOURCES += $(shell find $(PDK_SRC_PATH) \
           \( -name "*.adb" -or -name "*.ads" -or -name "*.cpp" \) \
           -not -name "*-test.adb" -not -name "*-test.ads" -not -name "*-test.cpp" \
           -not -path "*/platforms/*" )

ifeq ($(PLATFORM), x86)
	SOURCES += $(PDK_SRC_CMN_PATH)/flowcontrol/platforms/x86/pdk-cmn-flowcontrol-plat.adb
	SOURCES += $(PDK_SRC_CMN_PATH)/console/platforms/x86/pdk-cmn-console-plat.adb
	SOURCES += $(PDK_SRC_CMN_PATH)/console_cpp/platforms/x86/pdk-cmn-console-plat.cpp
	SOURCES += $(PDK_SRC_CMN_PATH)/logger/platforms/x86/pdk-cmn-logger-plat.cpp
	INCLUDES += $(PDK_SRC_CMN_PATH)/logger/platforms/x86
else
  ifeq ($(PLATFORM), mcxn236)
	PDK_CMN_PLATFORM_SPECIFIC_PATH := corepdk/platforms/mcxn236/cmn/src
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.cpp
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-console-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-console-plat.cpp
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-logger-plat.cpp
	INCLUDES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)
# TODO: Add mcxn547 platform specific sources
  else ifeq ($(PLATFORM), mcxn547-core0)
	PDK_CMN_PLATFORM_SPECIFIC_PATH := corepdk/platforms/mcxn236/cmn/src
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.cpp
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-console-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-console-plat.cpp
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-logger-plat.cpp
	INCLUDES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)
  else ifeq ($(PLATFORM), mcxn547-core1)
	PDK_CMN_PLATFORM_SPECIFIC_PATH := corepdk/platforms/mcxn236/cmn/src
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.cpp
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-console-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-console-plat.cpp
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-logger-plat.cpp
	INCLUDES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)
# TODO: Add mcxn556 platform specific sources
  else ifeq ($(PLATFORM), mcxn556-core0)
	PDK_CMN_PLATFORM_SPECIFIC_PATH := corepdk/platforms/mcxn236/cmn/src
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.cpp
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-console-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-console-plat.cpp
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-logger-plat.cpp
	INCLUDES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)
  else ifeq ($(PLATFORM), mcxn556-core1)
	PDK_CMN_PLATFORM_SPECIFIC_PATH := corepdk/platforms/mcxn236/cmn/src
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-flowcontrol-plat.cpp
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-console-plat.adb
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-console-plat.cpp
	SOURCES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)/pdk-cmn-logger-plat.cpp
	INCLUDES += $(PDK_CMN_PLATFORM_SPECIFIC_PATH)
  else
  	$(ubs-fatal) "Platform $(PLATFORM) is not supported"
  endif
endif
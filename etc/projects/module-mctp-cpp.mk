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



MCTP_CPP_MODULE_SRC_PATH := corepdk/modules/mctp-cpp/src
# Add CorePDK:MOD::MCTP-CPP include paths
INCLUDES += $(MCTP_CPP_MODULE_SRC_PATH)
# Add PDK CMN Source Files excluding platforms and test files
SOURCES += $(shell find $(MCTP_CPP_MODULE_SRC_PATH) \
           \( -name "*.adb" -or -name "*.ads" -or -name "*.cpp" \) \
           -not -name "*-test.adb" -not -name "*-test.ads" -not -name "*-test.cpp" -not -name "main.cpp" \
           -not -path "*/platforms/*" )
		   
# Separate platform specific sources depending on the platform
ifeq ($(PLATFORM), x86)
	MCTP_CPP_MODULE_PLATFORM_SPECIFIC_PATH := corepdk/modules/mctp-cpp/src/platforms/x86
else
  ifeq ($(PLATFORM), mcxn236)
	MCTP_CPP_MODULE_PLATFORM_SPECIFIC_PATH := corepdk/platforms/mcxn236/mctp-cpp/src
  # TODO: Add mcxn547 platform specific sources
  else ifeq ($(PLATFORM), mcxn547-core0)
	MCTP_CPP_MODULE_PLATFORM_SPECIFIC_PATH := corepdk/platforms/mcxn236/mctp-cpp/src
  else ifeq ($(PLATFORM), mcxn547-core1)
	MCTP_CPP_MODULE_PLATFORM_SPECIFIC_PATH := corepdk/platforms/mcxn236/mctp-cpp/src
  # TODO: Add mcxn556 platform specific sources
  else ifeq ($(PLATFORM), mcxn556-core0)
	MCTP_CPP_MODULE_PLATFORM_SPECIFIC_PATH := corepdk/platforms/mcxn236/mctp-cpp/src
  else ifeq ($(PLATFORM), mcxn556-core1)
	MCTP_CPP_MODULE_PLATFORM_SPECIFIC_PATH := corepdk/platforms/mcxn236/mctp-cpp/src
  else
  	$(ubs-fatal) "Platform $(PLATFORM) is not supported"
  endif
endif

# Add CorePDK:MOD::MCTP-CPP platform-specific include paths
INCLUDES += $(MCTP_CPP_MODULE_PLATFORM_SPECIFIC_PATH)
# Add CorePDK:MOD::MCTP-CPP platform-specific sources
SOURCES += $(MCTP_CPP_MODULE_PLATFORM_SPECIFIC_PATH)/pdk-mctp-platforms-packet-plat.cpp
SOURCES += $(MCTP_CPP_MODULE_PLATFORM_SPECIFIC_PATH)/pdk-mctp-platforms-router-plat.cpp
SOURCES += $(MCTP_CPP_MODULE_PLATFORM_SPECIFIC_PATH)/pdk-mctp-platforms-control.cpp
SOURCES += $(MCTP_CPP_MODULE_PLATFORM_SPECIFIC_PATH)/pdk-mctp-platforms-control-plat.cpp
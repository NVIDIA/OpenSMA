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


ifndef UBS_CMN_CMN

ubs-module-find     = $(shell [ -d $(1) ] && find $(1) -maxdepth 1 -type f ! -name *-test.*)
ubs-module-wildcard = $(foreach d, $(1) $(1)/platforms/$(UBS_PLATFORM), $(call ubs-module-find, $(d)))

# $(wildcard $(1)/platforms/$(UBS_PLATFORM)/*.*)

UBS_CMN_PDK         := $(call ubs-module-wildcard,$(UBS_PATH_PDK)) 
UBS_CMN_CMN         := $(call ubs-module-wildcard,$(UBS_PATH_CMN)) $(UBS_CMN_PDK)
UBS_CMN_FLOWCONTROL := $(call ubs-module-wildcard,$(UBS_PATH_CMN)/flowcontrol) $(UBS_CMN_CMN)
UBS_CMN_LOG         := $(call ubs-module-wildcard,$(UBS_PATH_CMN)/log) $(UBS_CMN_CMN)

# see src/pdk/cmn/log/debuglevel.h
# 0 = None, 1 = Fatal, 2 = Error, 3 = Warn, 4 = Debug, 5 = Info
GD_PDK_CMN_LOG_DBG_LEVEL_CONSOLE	?= 5
GD_PDK_CMN_LOG_DBG_LEVEL_PERSISTENT ?= 5
GD_PDK_CMN_LOG_MAX_MSG_LENGTH       ?= 64
GD_PDK_CMN_LOG_PLATFORM_TYPES_H		:= "pdk/cmn/log/platforms/$(UBS_PLATFORM)/types.h"
# we need to check paths in global if used in selftests
$(ubs_fixup_global_define_path 'GD_PDK_CMN_LOG_PLATFORM_TYPES_H')

# 0 = std::source_location
GD_PDK_CMN_LOG_SL_CONSOLE    := 0
GD_PDK_CMN_LOG_SL_PERSISTENT := 0

# Unique test strings that should NOT appear in binary when logging is disabled
GD_PDK_CMN_LOG_TEST_STRING_1 := "UNIQUE_LOG_TEST_MARKER_12345_ELIMINATED"
GD_PDK_CMN_LOG_TEST_STRING_2 := "UNIQUE_LOG_ERROR_MARKER_67890_ELIMINATED"

endif



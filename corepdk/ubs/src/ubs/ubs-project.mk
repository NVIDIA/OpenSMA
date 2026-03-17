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


include $(ubs_check_project_file '$(UBS_PROJECT)')

# -- verify the required vars are set ----------------------------------------------------------
$(ubs_info 'checking project requirements [$(UBS_FAINT)') 
# library projects do not have a main
UBS_MAIN := $(if $(filter library,$(UBS_TYPE)),,$(ubs_check_var 'MAIN'))
UBS_SRCS  := $(ubs_check_var 'SOURCES') $(UBS_MAIN)
$(ubs_py_exec print('$(UBS_N)]'))

# add features file if we are using ada
UBS_SRCS  += $(if $(filter %.adb,$(UBS_SRCS)),\
	$(UBS_PATH_GEN)/ubs-features.ads $(UBS_PATH_ROOT)/ubs.ads)

# the final target will be the project name or TARGET if given
UBS_TARGET      := $(or $(TARGET_NAME),$(UBS_PROJECT))
UBS_TARGETS_ALL += $(UBS_TARGET)


# -- update flags ------------------------------------------------------------------------------

#
# joins together the flags for each language:
#   UBS_CC_FLAGS := $(CC_FLAGS) $(CC_FLAGS_rel)
#
define ubs-flag-generate
UBS_$(1)_FLAGS  += $$($(1)_FLAGS) $$($(1)_FLAGS_$$(UBS_MODE))
endef
$(foreach lang,$(UBS_SUPPORTED_LANG),$(eval $(call ubs-flag-generate,$(lang))))
ifeq ($(strip $(UBS_GEN_SU)),1)
UBS_CC_FLAGS  += -fstack-usage
UBS_CXX_FLAGS += -fstack-usage
endif

# update include flags
INCLUDES        += $(UBS)/src src
$(ubs_fixup_includes 'INCLUDES')
$(ubs_fixup_includes 'SYSTEM_INCLUDES')
UBS_EXTRA_FLAGS += $(addprefix -isystem,$(SYSTEM_INCLUDES))
UBS_EXTRA_FLAGS += $(addprefix -I,$(INCLUDES))

UBS_CXX_FLAGS := -std=c++23 -xc++ $(UBS_CXX_FLAGS) $(UBS_EXTRA_FLAGS)
UBS_ADA_FLAGS += -gnat2022
UBS_CPP_FLAGS += $(UBS_EXTRA_FLAGS)
UBS_CC_FLAGS  += $(UBS_EXTRA_FLAGS)

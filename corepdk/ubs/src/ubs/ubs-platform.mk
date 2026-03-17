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


include $(ubs_check_platform_file '$(UBS_PLATFORM)')

# export platform type
GD_UBS_PLATFORM := "$(UBS_PLATFORM)"
UBS_SUPPORTED_LANG := CC CXX ADA BIND CPP LD LINK


# verify target has required variables have been correctly set

define ubs-lang-check
UBS_$(1) := $$(ubs_check_exec '$$($1)')
endef

$(ubs_info 'checking platform requirements [$(UBS_FAINT)') 
$(foreach lang,$(UBS_SUPPORTED_LANG), $(eval $(call ubs-lang-check,$(lang))))
$(ubs_py_exec print('$(UBS_N)]'))


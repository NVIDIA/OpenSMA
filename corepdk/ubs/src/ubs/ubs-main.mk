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


-include user.mk
include $(UBS_PATH_ROOT)/ubs-config.mk
include $(UBS_PATH_ROOT)/ubs-utils.mk
include $(UBS_PATH_ROOT)/ubs-printer.mk
include $(UBS_PATH_ROOT)/ubs-plugin.mk
include $(UBS_PATH_ROOT)/ubs-requirements.mk
include $(UBS_PATH_ROOT)/ubs-clean.mk
include $(UBS_PATH_ROOT)/ubs-help.mk
include $(UBS_PATH_ROOT)/ubs-platform.mk
include $(UBS_PATH_ROOT)/ubs-project.mk
include $(UBS_PATH_ROOT)/ubs-unittest.mk
include $(UBS_PATH_ROOT)/ubs-unitprove.mk
include $(UBS_PATH_ROOT)/ubs-coverage.mk
include $(UBS_PATH_ROOT)/ubs-static-analysis.mk
$(ubs_include_plugins True)
include $(UBS_PATH_ROOT)/ubs-post-config.mk
# rules after here
include $(UBS_PATH_ROOT)/ubs-rules.mk
$(ubs_py_exec import ubs_gprproject)


# -- Config dumping ----------------------------------------------------------------------------

config:
	$(ubs_banner_str 'CONFIG')
	$(ubs-info) 'UBS VERSION : $(UBS_VERSION)\n'
	$(ubs-info) '     DOCKER : $(UBS_DOCKER)\n'
	$(ubs-info) '    PROJECT : $(UBS_PROJECT)\n'
	$(ubs-info) '   PLATFORM : $(UBS_PLATFORM)\n'
	$(ubs-info) '       MODE : $(UBS_MODE)\n'
	$(ubs-info) '       TYPE : $(UBS_TYPE)\n'
	$(ubs-info) '    TARGETS : $(UBS_TARGETS_ALL)\n'
	$(ubs-info) '    LINKER  : $(UBS_LINKER_SCRIPT)\n'
	$(ubs-info) '    C Flags : $(UBS_CC_FLAGS)\n'
	$(ubs-info) '  C++ Flags : $(UBS_CXX_FLAGS)\n'
	$(ubs-info) '  Ada Flags : $(UBS_ADA_FLAGS)\n'
	$(ubs_line_str '_')


global-defines:
	$(ubs_banner_str 'GLOBAL-DEFINES')
	$(ubs_list_str 'global-defines')
	$(ubs_line_str '_')


# rebuild everything if these change
%: $(wildcard $(UBS_PATH_ROOT)/*.mk) $(UBS_PROJECT_FILE) $(UBS_SYSTEM_FILE) #TODO: unsure about UBS_SYSTEM_FILE

.SUFFIXES:                                      # disable all inbuilt rules
.PHONY: $(UBS_COMMANDS)                         # all commands are phony
.SILENT: $(UBS_COMMANDS)                        # all commands are silent

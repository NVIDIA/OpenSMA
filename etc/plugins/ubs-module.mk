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



UBS_COMMANDS_RESULT 	+= module-add
UBS_COMMANDS_NO_RESULT 	+= module-list

UBS_MODULE_ADD_STAGES 	:= .WAIT module-add-banner .WAIT \
							module-add-run-validator \
							module-add-git \
							module-add-run-generator

UBS_MODULE_LIST_STAGES	:= .WAIT module-list-banner .WAIT \
							module-list-run-retriever

UBS_MODULE_CONFIG 		?= etc/projects/
UBS_MODULE_CPP_MACRO	?= $(UBS_PATH_GEN)/pdk-cmn-api-project.h
UBS_MODULE_ADA_MACRO	?= $(UBS_PATH_GEN)/pdk-cmn-api-project.ads

$(ubs_py_exec import os; import sys;sys.path.append(os.path.dirname(sys.path[0])); from pdk.cmn.api_version import api_version)

module-add-banner:
	$(ubs_banner_str 'MODULE-ADD')

module-add-run-validator:
	$(ubs_module_validator '$(MODULE)')

module-add-run-generator: module-add-git
	$(ubs_module_conf_generator '$(UBS_MODULE_CONFIG)')
	$(ubs_module_cpp_macro_generator '$(UBS_MODULE_CPP_MACRO)')
	$(ubs_module_ada_macro_generator '$(UBS_MODULE_ADA_MACRO)')

module-add-git: module-add-run-validator
	echo $(subst mcu/mcxn236.git,pdk/$(UBS_MODULE_TYPE)/$(UBS_MODULE_NAME).git,$(shell git config --get remote.origin.url))
	if git ls-remote $(subst mcu/mcxn236.git,pdk/$(UBS_MODULE_TYPE)/$(UBS_MODULE_NAME).git,$(shell git config --get remote.origin.url)) > /dev/null 2>&1; then \
		git submodule add ../../pdk/$(UBS_MODULE_TYPE)/$(UBS_MODULE_NAME) corepdk/modules/$(UBS_MODULE_NAME) > /dev/null 2>&1; \
		echo module-add info: module added; \
		if cd corepdk/modules/$(UBS_MODULE_NAME); git checkout v$(UBS_MODULE_VER_MAJOR).$(UBS_MODULE_VER_MINOR).$(UBS_MODULE_VER_BUILD) > /dev/null 2>&1; then \
			echo module-add info: module checkouted to the specify version; \
		else \
			cd ..; \
			echo module-add error: module version does not exists; \
			git submodule deinit -f corepdk/modules/$(UBS_MODULE_NAME) > /dev/null 2>&1; \
			rm -rf .git/modules/$(UBS_MODULE_NAME)> /dev/null 2>&1; \
			git rm -rf corepdk/modules/$(UBS_MODULE_NAME)> /dev/null 2>&1; \
			echo module-add info: module deleted; \
			exit 1; \
		fi \
	else \
		echo module-add error: module does not exist; \
		exit 1;\
	fi


module-add: $(UBS_MODULE_ADD_STAGES)

module-list-banner:
	$(ubs_banner_str 'MODULE-LIST')

module-list-run-retriever:
	$(ubs_module_retriever '$(UBS_MODULE_CONFIG)')

module-list: $(UBS_MODULE_LIST_STAGES)

.PHONY: $(UBS_MODULE_ADD_STAGES) $(UBS_MODULE_LIST_STAGES)
.SILENT: $(UBS_MODULE_ADD_STAGES) $(UBS_MODULE_LIST_STAGES)
.NOTPARALLEL: $(UBS_MODULE_ADD_STAGES) $(UBS_MODULE_LIST_STAGES)
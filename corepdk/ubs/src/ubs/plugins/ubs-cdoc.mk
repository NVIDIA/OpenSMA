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


UBS_COMMANDS_RESULT  	+= cdoc

# UBS_CDOC_CHECK_ONLY=0 -- will actually generate code documentation
# UBS_CDOC_CHECK_ONLY=1 -- will validate code documentation and fail with error output
# UBS_CDOC_CHECK_ONLY=2 -- will validate code documentation and warn with warn output
UBS_CDOC_CHECK_ONLY     ?= 0

UBS_CDOC_DOTFILE_DIRS	:= $(UBS_PATH_REPORTS)/code-documentation/dotfiles # collect call/caller/dependency grapgh
UBS_CDOC_CXX_REPORTS	:= $(UBS_PATH_REPORTS)/code-documentation/cxx
UBS_CDOC_ADA_REPORTS	:= $(UBS_PATH_REPORTS)/code-documentation/gnatdoc
UBS_CDOC_ADA_FLAGS      := -P$(UBS_GPR_PROJECT) -O$(UBS_CDOC_ADA_REPORTS) --backend=html --generate=public --style=gnat --warnings

UBS_CDOC_RE_CXX 		:= r'[\w/-]+\.[ch](pp)?$$'
UBS_CDOC_RE_ADA 		:= r'[\w/-]+\.ad[bs]$$'
UBS_CDOC_SRCS_CXX 		:= $(ubs_find '$(UBS_PATH_SRC)', $(UBS_CDOC_RE_CXX))
UBS_CDOC_SRCS_ADA 		:= $(ubs_find '$(UBS_PATH_SRC)', $(UBS_CDOC_RE_ADA))

# bypass UBS_UNITTEST_CPP_COUNT if GD_UBS_UNITTEST_CPP_COUNT is not defined during executing doxygen
UBS_CDOC_UNITTEST_CPP_COUNT := $(or $(GD_UBS_UNITTEST_CPP_COUNT),$(ubs_find_unittests '$(UBS_CDOC_SRCS_CXX)'))
UBS_CDOC_DOXYGEN_CONFIG     := $(if $(wildcard etc/doxygen.cfg),etc/doxygen.cfg,$(UBS)/etc/doxygen.cfg)

UBS_CDOC_LOG := while read line; do \
case $(UBS_CDOC_CHECK_ONLY) in \
	0) true;; \
	1) $(UBS_PRINT) "$(ubs-error-fmt) $$line\n"; false;; \
	2) $(UBS_PRINT) "$(ubs-warn-fmt) $$line\n";; \
	*) false ;; \
esac; \
done

ubs-cdoc-banner:
	$(ubs_banner_str 'CODE-DOCUMENTATION')
	$(call ubs-create-folder, $(UBS_PATH_REPORTS))
	@echo "UBS_PATH_REPORTS=$(UBS_PATH_REPORTS)" > $(UBS_PATH_BUILD)/report-path-vars.env

ubs-cdoc-cxx: 
	$(ubs-info) "CXX Code Documentation Generation\n"
	$(call ubs-create-folder,$(UBS_CDOC_CXX_REPORTS))
	$(call ubs-create-folder,$(UBS_CDOC_DOTFILE_DIRS))
	@$(UBS_SED) -e "s|^\(OUTPUT_DIRECTORY[[:space:]]*=\).*|\1 ${UBS_CDOC_CXX_REPORTS}|" \
		 -e "s|^\(HTML_STYLESHEET[[:space:]]*=\).*|\1 ${UBS_PATH_SHARE}/css/doxygen.css|" \
		 -e "s|^\(PROJECT_LOGO[[:space:]]*=\)[[:space:]]*$ |\1 ${UBS_PATH_SHARE}/css/logo-ubs.png|" \
		 $(UBS_CDOC_DOXYGEN_CONFIG) > ${UBS_CDOC_CXX_REPORTS}/doxygen.cfg
	$(UBS_DOXYGEN) ${UBS_CDOC_CXX_REPORTS}/doxygen.cfg 2>&1 | $(UBS_CDOC_LOG)
	python3 $(UBS_PATH_ROOT)/missing-cdoc-detector.py $(UBS_PATH_SRC) | $(UBS_CDOC_LOG)
	$(UBS_FIND) $(UBS_CDOC_CXX_REPORTS) -name '*.dot' -exec mv {} $(UBS_CDOC_DOTFILE_DIRS) \; 

ubs-cdoc-ada:
	$(ubs-info) "Ada Code Documentation Generation\n"
	$(call ubs-create-folder,$(UBS_CDOC_ADA_REPORTS))
	{ $(UBS_GNATDOC) $(UBS_CDOC_ADA_FLAGS) 2>&1; $(UBS_CP) $(UBS_PATH_SHARE)/css/gnatdoc.css $(UBS_CDOC_ADA_REPORTS); } | $(UBS_CDOC_LOG)

UBS_CDOC_STAGES := ubs-cdoc-banner .WAIT $(if $(UBS_CDOC_SRCS_CXX),ubs-cdoc-cxx,) $(if $(UBS_CDOC_SRCS_ADA),ubs-cdoc-ada,)

cdoc: $(UBS_CDOC_STAGES)

static-analysis: cdoc			 # add to static-analysis

.PHONY: cdoc $(UBS_CDOC_STAGES)
.SILENT: cdoc $(UBS_CDOC_STAGES)
.NOTPARALLEL: cdoc $(UBS_CDOC_STAGES)

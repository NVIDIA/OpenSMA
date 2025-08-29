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



UBS_COMMANDS_RESULT += format	# request plugin displays result

# UBS_FORMAT_CHECK_ONLY=0 -- will actually format the files
# UBS_FORMAT_CHECK_ONLY=1 -- will validate formatting and fail on first error
# UBS_FORMAT_CHECK_ONLY=2 -- will validate formatting and warn only
UBS_FORMAT_CHECK_ONLY  ?= 1
# UBS_FORMAT_IGNORE_TEST_FILE=1 -- will ignore xx-test.xx files
# UBS_FORMAT_IGNORE_TEST_FILE=other -- do nothing
UBS_FORMAT_IGNORE_TEST_FILE ?= 0
# UBS_FORMAT_IGNORE_DIR=aa,bb,cc/dd.cpp -- will ignore the certain files or folders, use ',' to split
UBS_FORMAT_IGNORE_DIR  ?= 

UBS_CLANG_FORMAT_FLAGS += --Werror
UBS_GNAT_PP_FLAGS      += --pipe
UBS_FORMAT_DELTA_FLAGS := --config etc/deltarc --width=$(UBS_WIDTH)

CORE_PDK_PLATFORM_SRC := corepdk/platforms
# temp solution to format the corepdk/modules during transition to git submodules
CORE_PDK_MODULE_SRC   := corepdk/modules

# python regex for source files *.cpp *.c *.h *.hpp *.ads *.adb
UBS_FORMAT_COMMA  := ,
UBS_FORMAT_LINE   := |
UBS_FORMAT_IGNORE_PATTERN := src/selftests/format/
UBS_FORMAT_IGNORE_PATTERN := $(UBS_FORMAT_IGNORE_PATTERN)$(if $(UBS_FORMAT_IGNORE_DIR),$(UBS_FORMAT_LINE)$(subst $(UBS_FORMAT_COMMA),$(UBS_FORMAT_LINE),$(UBS_FORMAT_IGNORE_DIR)),)
UBS_FORMAT_IGNORE_PATTERN := $(UBS_FORMAT_IGNORE_PATTERN)$(if $(filter 1, $(UBS_FORMAT_IGNORE_TEST_FILE)),$(UBS_FORMAT_LINE)[\w/-]+-test\.,)
UBS_FORMAT_IGNORE_PATTERN := $(strip $(UBS_FORMAT_IGNORE_PATTERN))
UBS_FORMAT_IGNORE := ^(?!.*($(subst $(UBS_FORMAT_LINE),|,$(strip $(UBS_FORMAT_IGNORE_PATTERN))))).*
UBS_FORMAT_RE_CXX := r'$(UBS_FORMAT_IGNORE)[\w/-]+\.[ch](pp)?$$'
UBS_FORMAT_RE_ADA := r'$(UBS_FORMAT_IGNORE)[\w/-]+\.ad[bs]$$'

UBS_FORMAT_SRCS_CXX := $(ubs_find '$(UBS_PATH_SRC)', $(UBS_FORMAT_RE_CXX))
UBS_FORMAT_SRCS_ADA := $(ubs_find '$(UBS_PATH_SRC)', $(UBS_FORMAT_RE_ADA))
UBS_FORMAT_SRCS_CXX += $(ubs_find '$(CORE_PDK_PLATFORM_SRC)', $(UBS_FORMAT_RE_CXX))
UBS_FORMAT_SRCS_ADA += $(ubs_find '$(CORE_PDK_PLATFORM_SRC)', $(UBS_FORMAT_RE_ADA))
UBS_FORMAT_SRCS_CXX += $(ubs_find '$(CORE_PDK_MODULE_SRC)', $(UBS_FORMAT_RE_CXX))
UBS_FORMAT_SRCS_ADA += $(ubs_find '$(CORE_PDK_MODULE_SRC)', $(UBS_FORMAT_RE_ADA))

UBS_FORMAT_OBJS_CXX := $(addprefix $(UBS_PATH_OBJ)/,$(UBS_FORMAT_SRCS_CXX:=.fmt))
UBS_FORMAT_OBJS_ADA := $(addprefix $(UBS_PATH_OBJ)/,$(UBS_FORMAT_SRCS_ADA:=.fmt))

UBS_FORMAT_COPY_CMD := $(if $(UBS_SELFTESTS),,$(UBS_CP) $$@ $$<) # HACK

define ubs-format-rule
$(UBS_PATH_OBJ)/%.$(1).fmt: %.$(1)
	$$(ubs-build) "FORMAT" "$$@"
	$$(call ubs-create-target-folder)
	@$(if $(filter ad%,$(1)),\
		$$(UBS_GNAT_PP) $$(UBS_GNAT_PP_FLAGS),\
		$$(UBS_CLANG_FORMAT) $$(UBS_CLANG_FORMAT_FLAGS)) $$< > $$@
	@$(UBS_DELTA) $$(UBS_FORMAT_DELTA_FLAGS) $$(shell [ -L "$$<" ] && readlink -f "$$<" || echo "$$<") $$@ || \
    case "$$(UBS_FORMAT_CHECK_ONLY)" in \
	  0) $(UBS_PRINT) "$(ubs-info-fmt)  $$< formatted\n"; $(UBS_FORMAT_COPY_CMD) ;; \
      1) $(UBS_PRINT) "$(ubs-error-fmt) $$<\n"; $(UBS_RM) $$@; false ;; \
      2) $(UBS_PRINT) "$(ubs-warn-fmt)  $$<\n"; $(UBS_RM) $$@;; \
      *) false ;; \
    esac
.NOTPARALLEL: $(UBS_PATH_OBJ)/%.$(1).fmt
endef

# -- format rules ------------------------------------------------------------------------------
#$(foreach ext,c cpp h hpp adb ads,$(eval $(call ubs-format-rule,$(ext))))
$(foreach ext,c cpp h hpp,$(eval $(call ubs-format-rule,$(ext))))

ubs-format-banner:
	$(ubs_banner_str 'FORMAT')
ubs-format-cxx: $(UBS_FORMAT_OBJS_CXX)
#ubs-format-ada: $(UBS_FORMAT_OBJS_ADA)

UBS_FORMAT_STAGES := ubs-format-banner ubs-format-cxx #ubs-format-ada

format: $(UBS_FORMAT_STAGES)

static-analysis: format			# add to static-analysis

.PHONY: format $(UBS_FORMAT_STAGES)
.SILENT: format $(UBS_FORMAT_STAGES)
.NOTPARALLEL: format $(UBS_FORMAT_STAGES)

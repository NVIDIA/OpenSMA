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



# -- base folder structure ---------------------------------------------------------------------
export SHELL        := /usr/bin/bash
UBS_VERSION         := 0.9.1
UBS                 ?= $(PWD)
UBS_PATH_LIBEXEC    := $(UBS)/libexec
UBS_PATH_SHARE      ?= $(UBS)/share
UBS_PATH_ROOT       ?= $(UBS)/src/ubs
UBS_PATH_PDK        ?= $(UBS)/src/pdk
UBS_PATH_CMN        ?= $(UBS_PATH_PDK)/cmn
UBS_PATH_BUILD      ?= build
UBS_PATH_SRC        ?= src
UBS_PATH_DOC        ?= $(UBS)/doc
UBS_PATH_PROJECTS   := $(UBS)/etc/projects etc/projects corepdk_etc/mk/projects #etc/projects is project-level, corepdk_etc/mk/projects is module-level
UBS_PATH_PLATFORMS  := $(UBS)/etc/platforms etc/platforms corepdk_etc/mk/platforms #etc/platforms is project-level, corepdk_etc/mk/platforms is module-level	


UBS_PLATFORM  ?= $(or $(PLATFORM),x86)
UBS_PROJECT   ?= $(or $(PROJECT),default)
UBS_MODE      ?= $(or $(MODE),dev)
UBS_TYPE      ?= $(or $(TYPE),project) # module, project, library
UBS_LIB_KIND  ?= $(or $(LIB_KIND),static) # static, dynamic
UBS_GEN_SU    ?= $(or $(GEN_SU),0) # -fstack-usage to generate .su files for c/cpp

UBS_PATH_MAN	  := $(UBS_PATH_DOC)/man
UBS_PATH_THEME    := $(UBS_PATH_SHARE)/utf8
UBS_PATH_OUT      := $(UBS_PATH_BUILD)/$(or $(UBS_SELFTEST),$(UBS_PROJECT))-$(UBS_PLATFORM)-$(UBS_MODE)
UBS_PATH_OBJ      := $(UBS_PATH_OUT)/obj
UBS_PATH_GEN      := $(UBS_PATH_OUT)/gen
UBS_PATH_REPORTS  := $(UBS_PATH_OUT)/reports
UBS_PATH_TOOLKITS ?= /opt/ubs
UBS_PATH_LIBS_GEN  := $(UBS_PATH_OUT)/libs

UBS_GNAT_VERSION  := 25.0w
UBS_GPR_PROJECT	  := $(or $(GPR_PROJECT),default.gpr)
UBS_DOCKER		  ?= local

# these are x64 only
UBS_PATH_GNAT     := $(UBS_PATH_TOOLKITS)/gnat-$(UBS_GNAT_VERSION)/bin
UBS_PATH_GNAT_SAS := $(UBS_PATH_TOOLKITS)/gnatsas-$(UBS_GNAT_VERSION)/bin
UBS_PATH_GNAT_DAS := $(UBS_PATH_TOOLKITS)/gnatdas-$(UBS_GNAT_VERSION)/bin
UBS_PATH_SPARK    := $(UBS_PATH_TOOLKITS)/spark-$(UBS_GNAT_VERSION)/bin
UBS_PATH_GNAT_DOC := $(UBS_PATH_TOOLKITS)/gnatdoc-25.0/bin

# commands that want the pass/fail banners
UBS_COMMANDS_RESULT     := build unittest coverage prove examine
UBS_COMMANDS_RESULT     += static-analysis coverity tidy sas
UBS_COMMANDS_NO_RESULT  := usage config global-defines

# help commands
UBS_COMMANDS_HELP       := help help-build

# all supported commands
UBS_COMMANDS = $(UBS_COMMANDS_HELP) $(UBS_COMMANDS_RESULT) $(UBS_COMMANDS_NO_RESULT)

# coverage exclusion tags (regex patterns)
UBS_COVERAGE_MARK_LINE  ?= (//|--) \[ubs-no-coverage\]
UBS_COVERAGE_MARK_BEGIN ?= (//|--) \[ubs-no-coverage-begin\]
UBS_COVERAGE_MARK_END   ?= (//|--) \[ubs-no-coverage-end\]



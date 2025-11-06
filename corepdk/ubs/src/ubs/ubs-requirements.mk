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



$(ubs_banner 'HEALTH-CHECK')
$(ubs_info 'checking ubs requirements [$(UBS_FAINT)') 

# these must all be available in the PATH
UBS_TPUT        := $(ubs_check_exec 'tput')
UBS_PRINT       := $(ubs_check_exec 'printf')
UBS_CAT         := $(ubs_check_exec 'cat')
UBS_AWK         := $(ubs_check_exec 'gawk')
UBS_SED         := $(ubs_check_exec 'sed')
UBS_ASCIIDOC    := $(ubs_check_exec 'asciidoc')
UBS_ASCIIDOCTOR := $(ubs_check_exec 'asciidoctor')
UBS_GROFF       := $(ubs_check_exec 'groff')
UBS_GROTTY      := $(ubs_check_exec 'grotty')
UBS_RM          := $(ubs_check_exec 'rm')
UBS_CP          := $(ubs_check_exec 'cp')
UBS_MKDIR       := $(ubs_check_exec 'mkdir') -p
UBS_TOUCH		:= $(ubs_check_exec 'touch')
UBS_FIND		:= $(ubs_check_exec 'find')
UBS_DELTA		:= $(ubs_check_exec 'delta')

UBS_CLANG_TIDY	 := $(ubs_check_exec 'clang-tidy-18')
UBS_CLANG_FORMAT := $(ubs_check_exec 'clang-format-18')
UBS_GCOV		 := $(ubs_check_exec '/usr/bin/gcov-13')
UBS_LCOV		 := $(ubs_check_exec 'lcov')
UBS_GENHTML		 := $(ubs_check_exec 'genhtml')
UBS_DOXYGEN		:= $(ubs_check_exec 'doxygen')
ifneq ($(run_local),1)
UBS_GNAT_SAS	 := $(ubs_check_exec '$(UBS_PATH_GNAT_SAS)/gnatsas')
UBS_GNAT_PP     := $(ubs_check_exec '$(UBS_PATH_GNAT)/gnatpp')
UBS_GNAT_PROVE  := $(ubs_check_exec '$(UBS_PATH_SPARK)/gnatprove')
UBS_GNATDOC	 := $(ubs_check_exec '$(UBS_PATH_GNAT_DOC)/gnatdoc')
UBS_GRCOV                := $(ubs_check_exec '$(UBS_PATH_LIBEXEC)/grcov')
endif
$(ubs_py_exec print('$(UBS_N)]'))


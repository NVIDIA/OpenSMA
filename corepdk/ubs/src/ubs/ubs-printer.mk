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




# -- UBS Universal Build System -- Console Ouptut Library -------------------------------------


# -- Formatting defines -----------------------------------------------------------------------
define UBS_NEWLINE =


endef

UBS_PRINT       := printf
UBS_TERM        ?= xterm
UBS_WIDTH       := 120
UBS_NOOP        :=# leave empty
UBS_SPACE       := $(NOOP) $(NOOP)
UBS_COMMA       := ,
UBS_W           :=$(shell tput -T$(UBS_TERM) setaf 7)
UBS_R           :=$(shell tput -T$(UBS_TERM) setaf 1)
UBS_G           :=$(shell tput -T$(UBS_TERM) setaf 2)
UBS_Y           :=$(shell tput -T$(UBS_TERM) setaf 3)
UBS_BOLD        :=$(shell tput -T$(UBS_TERM) bold)
UBS_FAINT       :=$(shell tput -T$(UBS_TERM) dim)
UBS_ITALIC      :=$(shell tput -T$(UBS_TERM) sitm)
UBS_UNDERLINE   :=$(shell tput -T$(UBS_TERM) smul)
UBS_REVERSE     :=$(shell tput -T$(UBS_TERM) rev)
UBS_CNORM       :=$(shell tput -T$(UBS_TERM) cnorm)
UBS_CIVIS       :=$(shell tput -T$(UBS_TERM) civis)
UBS_CSC         :=$(shell tput -T$(UBS_TERM) sc)
UBS_CRC         :=$(shell tput -T$(UBS_TERM) rc)
UBS_CEOL        :=$(shell tput -T$(UBS_TERM) el)
UBS_N           :=$(shell tput -T$(UBS_TERM) sgr0)
UBS_RB          :=$(UBS_REVERSE)$(UBS_BOLD)
UBS_ROW_COL     := IFS=';' read -u0 -sdR -p $$'\e[6n' ROW COL
UBS_LOGO        :=$(UBS_RB)Ｕ$(UBS_N) $(UBS_RB)Ｂ$(UBS_N) $(UBS_RB)Ｓ$(UBS_N)
UBS_MARGIN      :=  │

# -- ubs console printing macros --------------------------------------------------------------

# print out a informational message from within a rule
# usage: $(ubsinfo) "I am an informational messsage\n"
ubs-info-fmt  := $(UBS_G)INFO    $(UBS_N)$(UBS_MARGIN)#
ubs-info      := @$(UBS_PRINT) "$(ubs-info-fmt) "; $(UBS_PRINT)

# print out a warning message
ubs-warn-fmt := $(UBS_Y)WARN    $(UBS_N)$(UBS_MARGIN)#
ubs-warn     := @$(UBS_PRINT) "$(ubs-warn-fmt) "; $(UBS_PRINT)

# print out an error message
ubs-error-fmt := $(UBS_R)ERROR   $(UBS_N)$(UBS_MARGIN)#
ubs-fatal-fmt := $(UBS_N)$(UBS_R)FATAL   $(UBS_N)$(UBS_MARGIN)#
ubs-error     := @$(UBS_PRINT) "$(ubs-error-fmt) "; $(UBS_PRINT)

# print out a standard build messsage
# usage: $(ubs-build) "C++" "filename.cpp"
ubs-build-fmt := $(UBS_W)%-8s$(UBS_N)$(UBS_MARGIN) $(UBS_FAINT)%-32s$(UBS_N)
ubs-build     := @$(UBS_PRINT) "$(ubs-build-fmt)\n"

# print out a fatal message and exit immediately. 
ubs-fatal     = $(error $(UBS_NEWLINE)$(UBS_R)$(ubs-fatal-fmt) $(1)$(UBS_N)$(UBS_NEWLINE))

# print from current position ..............done
ubs-done = @$(UBS_ROW_COL); echo $$COL | $(UBS_AWK) '        \
BEGIN { printf "$(UBS_FAINT)"  }                            \
      { for (i=$$1; i <= $(UBS_WIDTH)-4; i++) printf "." }  \
END   { printf "$(UBS_N)done\n" }'


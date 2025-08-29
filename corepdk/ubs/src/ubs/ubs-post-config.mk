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



# Everything has been defined at this point

$(ubs_fixup_sources '') # fix up relative paths, remove dups

# -- Generate OBJS -----------------------------------------------------------------------------
UBS_SRCS_ASM           += $(filter %.S,$(UBS_SRCS))
UBS_SRCS_CC            += $(filter %.c,$(UBS_SRCS))
UBS_SRCS_CXX           += $(filter %.cpp,$(UBS_SRCS))
UBS_SRCS_ADB           += $(filter %.adb,$(UBS_SRCS)) 
UBS_SRCS_ADS           += $(filter %.ads,$(UBS_SRCS)) 
UBS_SRCS_LD            := $(or $(filter %.ld,$(UBS_SRCS)),auto)
 
# remove any .ads files if there is a corresponding .adb 
# because we don't need to compile them
UBS_SRCS_ADS_ALL := $(UBS_SRCS_ADS) 
UBS_SRCS_ADB_TMP := $(sort $(notdir $(UBS_SRCS_ADB:.adb=.o)))
UBS_SRCS_ADS     := $(foreach f,$(UBS_SRCS_ADS_ALL),\
	                $(if $(findstring $(notdir $(f:.ads=.o)),$(UBS_SRCS_ADB_TMP)),,$f)) 

UBS_SRCS_ADA_BINDER := $(UBS_PATH_OBJ)/b__$(UBS_PROJECT).adb

UBS_TMP := $(UBS_SRCS_ASM:=.o) $(UBS_SRCS_CC:=.o) $(UBS_SRCS_CXX:=.o)
UBS_TMP += $(UBS_SRCS_ADB:.adb=.o) $(UBS_SRCS_ADS:.ads=.o)
UBS_OBJECTS += $(addprefix $(UBS_PATH_OBJ)/,$(sort $(UBS_TMP)))
# add ada binder only if we have ada files and we are not building a library
UBS_OBJECTS += $(if $(strip $(UBS_SRCS_ADS)$(UBS_SRCS_ADB)),$(if $(UBS_MAIN),$(UBS_SRCS_ADA_BINDER:.adb=.o),))

# -- Read the required input LIBS --------------------------------------------------------------
UBS_LIBS_ADS  := $(foreach lib,$(UBS_LIBS_PATH),$(ubs_find '$(lib)', r'[\w/-]+\.ads$$'))
UBS_LIBS_ALIS := $(foreach lib,$(UBS_LIBS_PATH),$(ubs_find '$(lib)', r'[\w/-]+\.ali$$'))
UBS_LIBS_AS   := $(foreach lib,$(UBS_LIBS_PATH),$(ubs_find '$(lib)', r'[\w/-]+\.a$$'))

# -- Generate ALIS DEPS INCLUDES ---------------------------------------------------------------
UBS_ALIS := $(sort $(UBS_SRCS_ADB:.adb=.ali) $(UBS_SRCS_ADS:.ads=.ali))
UBS_ALIS := $(addprefix $(UBS_PATH_OBJ)/,$(UBS_ALIS)) $(UBS_LIBS_ALIS)

UBS_DEPS  = $(UBS_OBJECTS:=.d)
UBS_SRCS_ADA := $(UBS_SRCS_ADB) $(UBS_SRCS_ADS_ALL) $(UBS_LIBS_ADS)
UBS_ADA_INCLUDES += $(sort $(dir $(UBS_SRCS_ADA)))

# -- Generate LIBS output files ----------------------------------------------------------------
UBS_LIBS_GEN  := $(sort $(filter-out %/ubs-features.ads %/ubs.ads, $(UBS_SRCS_ADA:.adb=.ads) $(UBS_LIBS_ADS)))
UBS_LIBS_GEN  += $(sort $(filter-out %/ubs-features.ali %/ubs.ali, $(UBS_ALIS) $(UBS_LIBS_ALIS)))
UBS_LIBS_GEN  += $(UBS_LIBS_AS)
# -- Linker script -----------------------------------------------------------------------------
UBS_LINKER_SCRIPT_AUTOGEN := $(UBS_PATH_OBJ)/autogen.pp.ld

ifeq ($(strip $(UBS_SRCS_LD)),auto)
UBS_LINKER_SCRIPT := $(UBS_LINKER_SCRIPT_AUTOGEN)
UBS_SRCS          += $(UBS_LINKER_SCRIPT_AUTOGEN)
# autogenerate the linker script
$(ubs_py_exec import ubs_linker_autogen)
else
UBS_LINKER_SCRIPT := $(addprefix $(UBS_PATH_OBJ)/,$(UBS_SRCS_LD:.ld=.pp.ld))
endif

# -- static analysis targets ------------------------------------------------------------------
UBS_TIDY_ALL = $(addprefix $(UBS_PATH_OBJ)/,$(UBS_SRCS_CXX:=.tidy))
# disable sas if no ada sources
UBS_SAS_STAGES := $(if $(strip $(UBS_SRCS_ADB)$(UBS_SRCS_ADS)),$(UBS_SAS_STAGES),)

# -- target -----------------------------------------------------------------------------------
UBS_TARGET := $(addprefix $(UBS_PATH_OUT)/,$(UBS_TARGET))

# TODO: move these ??
UBS_LIBS      += $(if $(UBS_SRCS_ADB),$(ADA_LIBS))
UBS_ADA_FLAGS += $(addprefix -I,$(UBS_ADA_INCLUDES))
ifneq ($(suffix $(UBS_MAIN)),.adb)
UBS_BIND_FLAGS += -n
else
UBS_ALIS       := $(UBS_PATH_OBJ)/$(UBS_MAIN:.adb=.ali) \
				  $(filter-out $(UBS_PATH_OBJ)/$(UBS_MAIN:.adb=.ali),$(UBS_ALIS))
endif

# -- Global Defines ---------------------------------------------------------------------------
UBS_ADA_GLOBAL_DEFINES      := $(UBS_PATH_GEN)/ubs.def
UBS_ADA_GLOBAL_DEFINES_LIST := $(UBS_PATH_GEN)/ubs-global-defines.def
UBS_CPP_GLOBAL_DEFINES_LIST := $(UBS_PATH_GEN)/ubs-global-defines.h
$(ubs_py_exec import ubs_global_defines)

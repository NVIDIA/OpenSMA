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



# -- optional includes -------------------------------------------------------------------------
include $(UBS_PATH_ROOT)/ubs-cmn.mk

# -- required ----------------------------------------------------------------------------------

# a list of all the files that are to be compiled
SOURCES := $(UBS_CMN_CONSOLE) $(UBS_CMN_CONSOLE_CPP)

# the main entry point file, can be a .c .cpp or .adb
MAIN    := $(UBS_PATH_SRC)/main.adb


# -- optional ----------------------------------------------------------------------------------
# these are optional but will be empty by default


# INCLUDES := 
# SYSTEM_INCLUDES := 

# applied to .c file compilations
CC_FLAGS     	:= -Wall
CC_FLAGS_dev 	:= -g -Og
CC_FLAGS_rel 	:= -Os -fomit-frame-pointer

# applied to .cpp file compilations
CXX_FLAGS     	:= -std=c++23 -Wall
CXX_FLAGS_dev 	:= -g -Og
CXX_FLAGS_rel 	:= -Os -fomit-frame-pointer

# applied to .ads/.adb file compilations
ADA_FLAGS     	:= -gnat2022 -gnatwa
ADA_FLAGS_dev 	:= -g -Og
ADA_FLAGS_rel 	:= -Os
# -gnatec for global config pragmas
# -gnatU  for local config pragmas

# applied to ada binder
BIND_FLAGS 	  	:=
BIND_FLAGS_dev	:=
BIND_FLAGS_rel 	:= 

# applied to preprocessing of linker scripts
CPP_FLAGS     	:= 
CPP_FLAGS_dev 	:= 
CPP_FLAGS_rel 	:= 

# applied to linker
LINK_FLAGS 		:=
LINK_FLAGS_dev 	:=
LINK_FLAGS_rel	:=

# -- global defines ---------------------------------------------------------------------------
# Any variable prefixed with GD_ will be visible in all C C++ Ada files
# - They are evaluated as python strings, so the following with be the equilvalent of 
#   -DEXAMPLE_NUMBER=4080
GD_EXAMPLE_NUMBER := 16 * 0xff

# -DEXAMPLE_STRING="build 16 * 0xff"
GD_EXAMPLE_STRING := "build $(GD_EXAMPLE_NUMBER)"

# use $$() to re-eval any make variables 
# -DEXAMPLE_STRING2="build 4080"
GD_EXAMPLE_STRING2 := "build $$(GD_EXAMPLE_NUMBER)"

# to add custom flags to a specific file 
# options are flags='' dev='' rel=''
$(ubs_custom_flags 'src/file.c', rel='-O3')

# or write it manually with make file syntax
$(UBS_PATH_OBJ)/src/file.o: src/file.c
	$(UBS_CC) -O3 $(UBS_DEP_FLAGS) -c $< -o $@

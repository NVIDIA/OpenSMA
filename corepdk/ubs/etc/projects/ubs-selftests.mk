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


# interal generic selftesting unittest project
# each one of the folders in $(UBS_SELF_TEST_ROOT) contains a unique project type
# that is validated

# all of the selftest projects live in here
UBS_SELFTEST_ROOT ?= $(UBS)/src/selftests

# we can override the selftest using UBS_SELFTEST
UBS_SELFTEST      ?= c-only
ROOT := $(UBS_SELFTEST_ROOT)/$(UBS_SELFTEST)

# -- these are required project----------------------------------------------------------------
SOURCES  	:= $(shell find $(ROOT) -type f -not -regex '.+-test\.cpp')
MAIN    	:= $(wildcard $(ROOT)/main.*)


# -- the following are all optional -----------------------------------------------------------

CC_FLAGS     	:= -Wall		
CC_FLAGS_dev 	:= -g
CC_FLAGS_rel 	:= -O2 -fomit-frame-pointer

CXX_FLAGS     	:= -std=c++23 -Wall
CXX_FLAGS_dev 	:= -g
CXX_FLAGS_rel 	:= -O2 -fomit-frame-pointer

ADA_FLAGS     	:= -gnat2022
ADA_FLAGS_dev 	:= -g
ADA_FLAGS_rel 	:= -Os
# -gnatec global config pragmas
# -gnatU  local config pragmas

BIND_FLAGS 	  	:=
BIND_FLAGS_dev	:=
BIND_FLAGS_rel 	:= 

CPP_FLAGS     	:= 
CPP_FLAGS_dev 	:= 
CPP_FLAGS_rel 	:= 

LINK_FLAGS 		:=
LINK_FLAGS_dev 	:=
LINK_FLAGS_rel	:=

# -- global defines ---------------------------------------------------------------------------
GD_UBS_SELFTESTS := 1
GD_UBS_SELFTESTS_STR := "selftests := $$(GD_UBS_SELFTESTS)"

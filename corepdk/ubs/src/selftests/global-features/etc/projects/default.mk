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


include $(UBS_PATH_ROOT)/ubs-cmn.mk

SOURCES := $(UBS_CMN_CONSOLE) $(UBS_CMN_CONSOLE_CPP) $(wildcard src/*.cpp src/*.adb src/*.S src/*.ld)
MAIN    := src/main.cpp

CXX_FLAGS += -DUBS_GLOBAL_FORMAT_TEST=$(TEST)
ADA_FLAGS += -gnateDUBS_GLOBAL_FORMAT_TEST=$(TEST)

FEATURE_Dummy0 := 0
FEATURE_Dummy1 := 0
FEATURE_ASM_SUPPORT := 1
FEATURE_LINKER_SUPPORT := 1

ifeq ($(TEST),0)
FEATURE_RegTable := 1
endif

ifeq ($(TEST),1)
FEATURE_RegTable := 0
endif

ifeq ($(TEST),2)
FEATURE_RegTable := 2
endif

ifeq ($(TEST),3)
FEATURE_RegTable := False
endif

ifeq ($(TEST),4)
FEATURE_RegTable := True
endif

ifeq ($(TEST),5)
FEATURE_RegTable := asdf	# this should fail
endif

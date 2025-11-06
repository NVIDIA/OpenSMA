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



CC 		:= gcc-13
CXX 	:= g++-13
CPP     := cpp-13
LD      := ld
LINK 	:= g++-13
OBJDUMP	:= objdump
READELF	:= readelf
ADA 	:= $(UBS_PATH_GNAT)/gcc
BIND 	:= $(UBS_PATH_GNAT)/gnatbind

ADA_INSTALL := $(shell $(ADA) -print-search-dirs | $(UBS_AWK) -F: '/install:/{print $$2}')
ADA_INSTALL := $(realpath $(ADA_INSTALL))
ADA_RTS     := native
ADA_LIBS 	:= $(ADA_INSTALL)/rts-$(ADA_RTS)/adalib/libgnat.a
ADA_TARGET  := x86_64-linux

# requirement for gnatsas
export PATH:=$(PATH):$(UBS_PATH_GNAT)

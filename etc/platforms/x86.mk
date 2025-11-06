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




GNAT_PRO_ROOT := $(UBS_PATH_TOOLKITS)/gnat-$(UBS_GNAT_VERSION)/bin

CC 		:= gcc-13
CXX 	:= g++-13
CPP     := cpp-13
LD      := ld
LINK 	:= g++-13
OBJDUMP	:= objdump
READELF	:= readelf

ADA 	:= $(GNAT_PRO_ROOT)/gcc
BIND 	:= $(GNAT_PRO_ROOT)/gnatbind

ADA_INSTALL := $(shell $(ADA) -print-search-dirs | $(UBS_AWK) -F: '/install:/{print $$2}')
ADA_INSTALL := $(realpath $(ADA_INSTALL))
ADA_RTS     := native
ADA_LIBS 	:= $(ADA_INSTALL)/rts-$(ADA_RTS)/adalib/libgnat.a
ADA_TARGET  := x86_64-linux

ADA_FLAGS += -gnatn -gnatwa -gnatwae
CPP_FLAGS += -E -P -x c

# -- CRYPTO flags ------------------------------------------------------------------------------
CC_FLAGS += -DMBEDTLS_MEMORY_BUFFER_ALLOC_C -DMBEDTLS_PLATFORM_MEMORY \
			-DMBEDTLS_PLATFORM_EXIT_ALT -DMBEDTLS_PLATFORM_NO_STD_FUNCTIONS

# For tidy support c++23 feature
ifneq (,$(filter tidy,$(MAKECMDGOALS)))
	CC_FLAGS += -D__cpp_concepts=202002L -Wno-builtin-macro-redefined
endif

SYSTEM_INCLUDES += \
	$(UBS_PATH_SRC)/overrides \
	$(PATH_SYS)/freertos

SYSTEM_INCLUDES +=  \
	$(PATH_SDK)/middleware/mbedtls/include/ \
	$(PATH_SDK)/middleware/mbedtls/include/mbedtls/
# ---------------------------------------------------------------------------------------------
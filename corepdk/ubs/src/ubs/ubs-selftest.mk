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


SHELL := bash

export UBS_SELFTESTS := 1

UBS_CONFIG_PATH	:= $(PWD)/src/ubs/ubs-config.mk 
include $(UBS_CONFIG_PATH)
include $(UBS_PATH_ROOT)/ubs-printer.mk

UBS_PATH_SELFTESTS := src/selftests

ALL_SELFTESTS_PROJECTS 	:= $(wildcard $(UBS_PATH_SELFTESTS)/projects/*)
ALL_COMMANDS			:= $(filter-out prove examine, $(UBS_COMMANDS))
ALL_SELFTESTS_PROJ_CMDS	:= $(foreach t, $(ALL_SELFTESTS_PROJECTS), $(foreach cmd, $(ALL_COMMANDS), $(shell basename $(t))-$(cmd)))
ALL_SELFTESTS 			:= generic-library-test library-test global-features format-test cdoc-test cmn-api-version 

selftest: $(ALL_SELFTESTS_PROJ_CMDS) $(ALL_SELFTESTS)

define UT
$(1)-$(2):
	@./ubs distclean > /dev/null
	./ubs $(2) PROJECT=ubs-selftests UBS_SELFTEST=$(1)
	@./ubs distclean > /dev/null
endef

$(foreach test,$(ALL_SELFTESTS),\
	$(foreach cmd,$(UBS_COMMANDS),\
		$(eval $(call UT,$(test),$(cmd)))))

define ubs-selftest-clean
	@cd $(1) && ./ubs distclean > /dev/null
endef 

define UT2
$(2)-$(3):
	$(call ubs-selftest-clean, $(1))
	cd $(1) && ./ubs $(3)
	$(call ubs-selftest-clean, $(1))
endef

$(foreach test,$(ALL_SELFTESTS_PROJECTS),\
	$(foreach cmd,$(UBS_COMMANDS),\
		$(eval $(call UT2,$(test),$(shell basename $(test)),$(cmd)))))

cmn-api-version:
	$(ubs-info) "Testing pdk::cmn::api_version\n"
	cd $(PWD) && $(PWD)/src/pdk/cmn/api_version/unittests.py

global-features:
	$(ubs-info) "Testing Global Features\n"
	@cd $(UBS_PATH_SELFTESTS)/$@ && ./ubs distclean > /dev/null
	cd $(UBS_PATH_SELFTESTS)/$@ && ./ubs unittest TEST=0
	cd $(UBS_PATH_SELFTESTS)/$@ && ./ubs unittest TEST=1
	cd $(UBS_PATH_SELFTESTS)/$@ && ./ubs unittest TEST=2
	cd $(UBS_PATH_SELFTESTS)/$@ && ./ubs unittest TEST=3
	cd $(UBS_PATH_SELFTESTS)/$@ && ./ubs unittest TEST=4
	cd $(UBS_PATH_SELFTESTS)/$@ && (./ubs unittest TEST=5 && false || true)
	$(ubs-info) "negative test: expected failure\n"
	@cd $(UBS_PATH_SELFTESTS)/$@ && ./ubs distclean > /dev/null

define UT3
	$(ubs-info) "Testing ubs::$(1)\n"
	$(call ubs-selftest-clean, $(3))
	cd $(3) && ./ubs $(1) $(2)=1; [ $$? -ne 0 ]
	$(ubs-info) "negative test: expected error\n"

	$(call ubs-selftest-clean, $(3))
	cd $(3) && ./ubs $(1) $(2)=2
	$(ubs-info) "negative test: expected pass with warnings\n"

	$(call ubs-selftest-clean, $(3))
	cd $(3) && ./ubs $(1) $(2)=0;
	$(call ubs-selftest-clean, $(3))
endef

FORMAT_SELFTESTS_PROJECT := $(UBS_PATH_SELFTESTS)/format
format-test:
	$(call UT3, format, UBS_FORMAT_CHECK_ONLY, $(FORMAT_SELFTESTS_PROJECT))

CDOC_SELFTESTS_PROJECT := $(UBS_PATH_SELFTESTS)/cdoc
cdoc-test:
	$(call UT3, cdoc, UBS_CDOC_CHECK_ONLY, $(CDOC_SELFTESTS_PROJECT))

LIB_SELFTESTS_PROJECT := $(UBS_PATH_SELFTESTS)/normal_library
library-test:
	$(call ubs-selftest-clean, $(LIB_SELFTESTS_PROJECT))
	$(ubs-info) "Testing ubs::library\n"
	cd $(LIB_SELFTESTS_PROJECT) && ./ubs build UBS_TYPE=library PROJECT=math_lib
	$(ubs-info) "Testing ubs::non library project includes library\n"
	cd $(LIB_SELFTESTS_PROJECT) && ./ubs build UBS_TYPE=project PROJECT=mainfunc
	$(call ubs-selftest-clean, $(LIB_SELFTESTS_PROJECT))

GENERIC_LIB_SELFTESTS_PROJECT := $(UBS_PATH_SELFTESTS)/generic_library
generic-library-test:
	$(ubs-info) "Testing ubs::generic_library\n"
	$(call ubs-selftest-clean, $(GENERIC_LIB_SELFTESTS_PROJECT))
	cd $(GENERIC_LIB_SELFTESTS_PROJECT) && ./ubs build UBS_TYPE=library PROJECT=parent
	cd $(GENERIC_LIB_SELFTESTS_PROJECT) && ./ubs build UBS_TYPE=library PROJECT=concrete
	cd $(GENERIC_LIB_SELFTESTS_PROJECT) && ./ubs build UBS_TYPE=project PROJECT=mainfunc
	cd $(GENERIC_LIB_SELFTESTS_PROJECT) && ./ubs build UBS_TYPE=project PROJECT=mainconcrete
	$(call ubs-selftest-clean, $(GENERIC_LIB_SELFTESTS_PROJECT))

PHONY: $(ALL_SELFTESTS)
.NOTPARALLEL:

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


ifeq ($(MAKECMDGOALS),coverage)

# TODO: remove this when implemented
ifneq ($(UBS_PLATFORM),x86)
$(ubs-fatal) "coverage not yet supported on hardware!!")
endif

# -- sources ----------------------------------------------------------------------------------
# add the coverage on exit hook
UBS_SRCS += $(UBS_PATH_ROOT)/coverage/ubs-coverage.cpp 

# -- global defines ---------------------------------------------------------------------------
GD_UBS_COVERAGE := 1

# -- flags ------------------------------------------------------------------------------------
# modify build flags to include coverage information
UBS_COV_FLAGS += -Og --coverage -ftest-coverage -fprofile-arcs -fprofile-info-section
UBS_CC_FLAGS  += $(UBS_COV_FLAGS)
UBS_CXX_FLAGS += $(UBS_COV_FLAGS)
UBS_ADA_FLAGS += $(UBS_COV_FLAGS) -gnateS # generate Source Coverage Obligation 
# -fprofile-arcs 	.gcda with arc transition, value profile counts + summary.
# -ftest-coverage 	.gcno block graphs and assign source line numbers to blocks.
# -fprofile-dir
# -fprofile-info-section disable global constructors/destructors
UBS_LINK_FLAGS += --static --coverage -fprofile-info-section
UBS_LCOV_RC    := $(ubs_choose_file 'etc/lcovrc')

# -- locals -----------------------------------------------------------------------------------
UBS_COVERAGE_LCOV_INFO := $(UBS_PATH_REPORTS)/lcov.info
UBS_COVERAGE_DB        := $(UBS_PATH_REPORTS)/cobertura.xml
UBS_GRCOV_FLAGS := $(UBS_PATH_OBJ) --source-dir src --branch    \
		--ignore 'build/*' --ignore '/usr/include/*'            \
		--ignore 'app/spdm_responder_ada_library/*'				\
		--ignore 'corepdk/modules/*'				            \
		--ignore 'libexec/sdk/*'                                \
		--output-config-file $(UBS_LCOV_RC)    					\
		--excl-line='$(strip $(UBS_COVERAGE_MARK_LINE))' 		\
		--excl-start='$(strip $(UBS_COVERAGE_MARK_BEGIN))'   	\
		--excl-stop='$(strip $(UBS_COVERAGE_MARK_END))'
		
# -- rules ------------------------------------------------------------------------------------
$(UBS_COVERAGE_DB):
	$(ubs-info) "Generating cobertura database for Gitlab"
	$(call ubs-create-target-folder)
	$(UBS_GRCOV) $(UBS_GRCOV_FLAGS) --output-types cobertura > $@
	$(ubs-done)

$(UBS_COVERAGE_LCOV_INFO) : $(UBS_COVERAGE_DB)
	$(ubs-info) "Generating lcov database"
	$(call ubs-create-target-folder)
	$(UBS_GRCOV) $(UBS_GRCOV_FLAGS) --output-types lcov > $@
	$(ubs-done)

ubs-coverage-html: $(UBS_COVERAGE_LCOV_INFO)
	$(ubs-info) "Generating coverage html report"
	$(UBS_GENHTML) $< --config-file $(UBS_LCOV_RC) --quiet --quiet \
		--output-directory $(UBS_PATH_REPORTS)/coverage \
		--substitute "s#^((?!share/|corepdk_etc/|libexec/|src/|/|corepdk/).+)#$$PWD/src/\$$1#" \
		--legend --flat --sort \
		--ignore-errors unused \
		--show-details --show-navigation  --show-proportion \
		--css-file $(UBS_PATH_SHARE)/gcov/gcov.css \
		--header-title "CorePDK Coverage Report - [$(UBS_PROJECT)]" \
		--title "CorePDK - $(UBS_PROJECT)" 
	$(ubs-done)

# this is just to hush a warning in x86(posix) builds
ubs-coverage-patch:
	$(ubs-info) "Patching gcno versions -> 'B33*'"
	@for cov_obj in $(UBS_OBJECTS:.o=.gcno); do \
		printf "00000004: 2a 33 33 42" | xxd -r - $$cov_obj; \
	done;
	$(ubs-done)

ubs-coverage-summary:
	$(ubs-info) "Text summary of coverage\n"
	$(ubs_line_str '-')
	$(UBS_GRCOV) $(UBS_GRCOV_FLAGS) --output-types markdown
	$(ubs_line_str '-')

ubs-coverage-banner:
	$(ubs_banner_str 'COVERAGE')

# -- build sequencing -------------------------------------------------------------------------
UBS_COVERAGE_STAGES := ubs-coverage-banner ubs-coverage-patch \
					   ubs-coverage-html ubs-coverage-summary 

coverage: unittest $(UBS_COVERAGE_STAGES)

.SILENT: $(UBS_COVERAGE_STAGES) $(UBS_COVERAGE_DB) $(UBS_COVERAGE_LCOV_INFO)
.PHONY: $(UBS_COVERAGE_STAGES)
.NOTPARALLEL: coverage
endif

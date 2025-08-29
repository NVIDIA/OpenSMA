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


ifneq ($(filter static-analysis coverity tidy sas format cdoc, $(MAKECMDGOALS)),)

# -- Coverity ----------------------------------------------------------------------------------

UBS_CODE_CLIMATE_CONVERT := $(UBS_PATH_ROOT)/ubs-code-climate.py
UBS_PATH_COVERITY	     := $(UBS_PATH_TOOLKITS)/coverity-2023.9.0
UBS_COVERITY_CAPTURE     := $(UBS_PATH_COVERITY)/bin/cov-capture
UBS_COVERITY_CONFIGURE   := $(UBS_PATH_COVERITY)/bin/cov-configure
UBS_COVERITY_ANALYZE     := $(UBS_PATH_COVERITY)/bin/cov-analyze
UBS_COVERITY_FORMAT      := $(UBS_PATH_COVERITY)/bin/cov-format-errors
UBS_COVERITY_EMIT        := $(UBS_PATH_COVERITY)/bin/cov-manage-emit
UBS_COVERITY_OUT         := $(UBS_PATH_OBJ)/cov
UBS_COVERITY_CONFIG      := $(UBS_COVERITY_OUT)/config
UBS_COVERITY_CFG         := $(UBS_COVERITY_CONFIG)/config.xml
UBS_COVERITY_REPORT      := $(UBS_PATH_REPORTS)/coverity
UBS_COMPILE_WRAPPER      := $(UBS_PATH_COVERITY)/bin/cov-build \
						        --no-banner --verbose 0 \
	                            --dir $(UBS_COVERITY_OUT) \
	                            --config $(UBS_COVERITY_CFG) \
	                            --emit-complementary-info 

UBS_COVERTIY_CERT_C       := $(UBS_PATH_COVERITY)/config/coding-standards/cert-c/cert-c-L1-L2.config
UBS_COVERTIY_CERT_CPP     := $(UBS_PATH_COVERITY)/config/coding-standards/cert-cpp/cert-cpp-L2-L1.config
UBS_COVERITY_FORMAT_FLAGS := --dir $(UBS_COVERITY_OUT) \
							 --title "UBS: Coverity Report For '$(UBS_PROJECT)'" \
							 --sort impact,file
UBS_COVERITY_REPORT_JSON  := $(UBS_COVERITY_REPORT)/result.json
UBS_COVERITY_REPORT_CODE  := $(UBS_COVERITY_REPORT)/codequality.json

UBS_COVERITY_STAGES := $(UBS_COVERITY_CFG) ubs-cov-build ubs-cov-analyze ubs-cov-html 
UBS_COVERITY_STAGES += $(UBS_COVERITY_REPORT_JSON) $(UBS_COVERITY_REPORT_CODE)
UBS_COVERITY_FILTER := $(UBS_AWK) '{printf "$(ubs-build-fmt)\n", "COV", $$0}'

# -- global defines ---------------------------------------------------------------------------
GD_UBS_STATIC_ANALYSIS := 1

# -- rules ------------------------------------------------------------------------------------
$(UBS_COVERITY_CFG):
	$(ubs-info) "coverity configuration"
	$(UBS_COVERITY_CONFIGURE) --comptype gcc \
	    --compiler $(notdir $(UBS_CXX)) \
	    --config $(UBS_COVERITY_CFG) --template >/dev/null
	$(ubs-done)

ubs-cov-build:: UBS_COMPILE_PRE := $(UBS_COMPILE_WRAPPER)
ubs-cov-build:: $(UBS_COVERITY_CFG) 
	$(ubs-info) "enabling coverity wrapper and restarting build\n"
ubs-cov-build:: build
	$(ubs-info) "coverity enabled build complete\n"
	# Remove SDK files in coverity DB
	-$(UBS_COVERITY_EMIT) --dir $(UBS_COVERITY_OUT) --tu-pattern "file('libexec/sdk/.*')" delete
	$(ubs-info) "Removed SDK files from coverity DB\n"
	# Remove corepdk modules files in coverity DB
	-$(UBS_COVERITY_EMIT) --dir $(UBS_COVERITY_OUT) --tu-pattern "file('corepdk/modules/.*')" delete
	$(ubs-info) "Removed corepdk/modules files from coverity DB\n"

ubs-cov-analyze: | ubs-cov-build
	$(ubs_banner_str 'COVERITY-ANALYSIS')
	$(ubs-info) "cert-c cert-cpp coverity analysis\n"
	$(ubs_line_str '-')
	$(UBS_COVERITY_ANALYZE) --dir $(UBS_COVERITY_OUT) \
		--no-banner --verbose 0 --ticker-mode none \
		--jobs auto \
        --aggressiveness-level high \
	    --all \
        --enable-fnptr \
        --security \
        --coding-standard-config $(UBS_COVERTIY_CERT_C) \
        --coding-standard-config $(UBS_COVERTIY_CERT_CPP) \
		| $(UBS_COVERITY_FILTER)

ubs-cov-html: | ubs-cov-analyze
	$(ubs_banner_str 'COVERITY-REPORTS')
	$(ubs-info) "generating coverity HTML report\n"
	$(ubs_line_str '-')
	[ -d $(UBS_COVERITY_REPORT) ] && $(UBS_RM) -rf $(UBS_COVERITY_REPORT) || true
	$(UBS_COVERITY_FORMAT) $(UBS_COVERITY_FORMAT_FLAGS) \
		--html-output $(UBS_COVERITY_REPORT) \
		| $(UBS_COVERITY_FILTER)

$(UBS_COVERITY_REPORT_JSON): | ubs-cov-html
	$(ubs-info) "creating coverty JSON database"
	$(UBS_COVERITY_FORMAT) $(UBS_COVERITY_FORMAT_FLAGS) --json-output-v10 $@
	$(ubs-done)

$(UBS_COVERITY_REPORT_CODE): $(UBS_COVERITY_REPORT_JSON)
	$(ubs-info) "converting to CodeClimate database for GitLab CICD integration"
	$(UBS_CODE_CLIMATE_CONVERT) $< $@ #--build-prefix=$(UBS_PATH_OBJ)
	$(ubs-done)

coverity:: 
	@$(ubs_banner_str 'COVERITY')

# -- Clang-Tidy -------------------------------------------------------------------------------
UBS_CLANG_TIDY_CFG   := $(ubs_choose_file 'etc/clang-tidy')
UBS_CLANG_TIDY_FLAGS ?= --quiet --config-file=$(UBS_CLANG_TIDY_CFG)

# the actual rule is in ubs-rules.mk
tidy::
	$(ubs_banner_str 'CLANG-TIDY')


# -- GNATsas ----------------------------------------------------------------------------------
UBS_SAS             := set -o pipefail; $(UBS_GNAT_SAS)
UBS_SAS_STAGES      := ubs-sas-analysis ubs-sas-report-text ubs-sas-report-code
UBS_SAS_REPORT	    := $(UBS_PATH_REPORTS)/sas
UBS_SAS_REPORT_CODE	:= $(UBS_SAS_REPORT)/codequality.json
UBS_GNAT_SAS_FLAGS  ?= -P$(UBS_GPR_PROJECT)
UBS_SAS_FILTER      := 2>&1 | $(UBS_AWK) '\
/high:/ {printf "$(ubs-error-fmt) %s\n",$$0; next}  \
/[a-zA-z0-9\-_]+\.ad[bs]:[0-9]+:[0-9]+: (high|medium|low|error):/ {printf "$(ubs-error-fmt) %s\n",$$0; next}  \
/[a-zA-z0-9\-_]+\.ad[bs]:[0-9]+:[0-9]+:.+ warning:/ {printf "$(ubs-warn-fmt) %s\n",$$0; next}  \
/[a-zA-z0-9\-_]+\.ad[bs]:[0-9]+:[0-9]+:.+:/ {printf "$(ubs-info-fmt) %s\n",$$0; next}  \
{printf "$(ubs-build-fmt)\n", "GNATSAS", $$0}'

ubs-sas-analysis:
	$(ubs-info) "gnatsas static analysis\n"
	$(UBS_SAS) analyze $(UBS_GNAT_SAS_FLAGS) $(UBS_SAS_FILTER)

ubs-sas-report-text:
	$(ubs-info) "generating text report\n"
	$(UBS_SAS) report text $(UBS_GNAT_SAS_FLAGS) $(UBS_SAS_FILTER)

ubs-sas-report-code:
	$(ubs-info) "generating CodeClimate report $(UBS_SAS_REPORT_CODE)"
	$(call ubs-create-folder,$(UBS_SAS_REPORT))
	$(UBS_SAS) report code-climate --quiet --root src/ $(UBS_GNAT_SAS_FLAGS) --out $(UBS_SAS_REPORT_CODE)
	$(ubs-done)

# actual rule in ubs-rules.mk
sas::
	$(ubs_banner_str 'GNAT-SAS')

UBS_STATIC_ANALYSIS_STAGES := tidy coverity sas

static-analysis: $(UBS_STATIC_ANALYSIS_STAGES)

.PHONY: ubs-cov-build ubs-cov-analyze ubs-cov-html
.SILENT: $(UBS_COVERITY_STAGES) $(UBS_SAS_STAGES)
.NOTPARALLEL: coverity tidy sas static-analysis

endif

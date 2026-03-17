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



include etc/plugins/sign-request.mk

UBS_COMMANDS_RESULT += sign-download-mbi sign-download-sb

sign-download-mbi:
	if [ ! -f "$(SIGN_MBI_REQ)" ]; then \
		echo "No longer to sign-request automatically, please use mcu_build.sh instead."; \
		exit 1; \
	fi

	$(ubs-build) "SIGN MBI" "$(UBS_TARGET_SIGNED)"
	# Generate $PROJECT_signed.bin
	REQ_ID=`jq '.requestId' $(SIGN_MBI_REQ)` && \
	echo "Donwloading signed firmware from request: $$REQ_ID" && \
	$(NVSEC) 3s download-signed-file --device_code $(SIGN_SSA) --request_id $$REQ_ID --download_path $(UBS_TARGET_SIGNED)

	# Generate PLDM fwpkg for mcu binary fw
	$(ubs-build) "FWPKG" "$(UBS_TARGET_FWPKG)"

	${PLDMPKG} $(PLDMPKG_CFG) $(UBS_TARGET_SIGNED) --apsku "${AP_SKU}" --ssdid "${SSDID}" --pkgver "${CHIP}PkgMcufw-${FW_VERSION}" --compver "${FW_VERSION}" --compstamp "${COMP_STAMP}" --out $(UBS_TARGET_FWPKG)

sign-download-sb:
	if [ ! -f "$(SIGN_SB_REQ)" ]; then \
		echo "No longer to sign-request automatically, please use mcu_build.sh instead."; \
		exit 1; \
	fi

	$(ubs-build) "SIGN SB" "$(UBS_TARGET_SB)"
	# Generate $PROJECT_signed.sb file
	REQ_ID=`jq '.requestId' $(SIGN_SB_REQ)` && \
	echo "Donwloading signed firmware from request: $$REQ_ID" && \
	$(NVSEC) 3s download-signed-file --device_code $(SIGN_SSA) --request_id $$REQ_ID --download_path $(UBS_TARGET_SB)

	# Generate PLDM fwpkg for mcu recovery fw according to each module
	$(ubs-build) "SB_FWPKG" "$(UBS_TARGET_SB_FWPKG)"
	echo "project config: $(PLDMPKG_RECOVERY_CFG)"; \

	@if [ -f "$(PLDMPKG_RECOVERY_CFG)" ]; then \
		echo "Using project config: $(PLDMPKG_RECOVERY_CFG)"; \
		${PLDMPKG} $(PLDMPKG_RECOVERY_CFG) $(UBS_TARGET_SB) --apsku "${AP_SKU}" --ssdid "${SSDID}" --pkgver "${CHIP}PkgMcufw-${FW_VERSION}" --compver "${FW_VERSION}" --compstamp "${COMP_STAMP}" --out $(UBS_TARGET_SB_FWPKG); \
	else \
		echo "Not found config file, will not generate PLDM fwpkg for mcu recovery fw"; \
	fi

.NOTPARALLEL: sign-download-mbi sign-download-sb
.PHONY: sign-download-mbi sign-download-sb
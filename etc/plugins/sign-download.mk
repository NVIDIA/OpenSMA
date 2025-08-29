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

UBS_COMMANDS_RESULT += sign-download-mbi sign-download-sb3

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

	${PLDMPKG} $(PLDMPKG_CFG) $(UBS_TARGET_SIGNED) --apsku "${AP_SKU}" --ssdid "${SSDID}" --pkgver "${CHIP}PkgMcufw-${FW_VERSION}" --compver "${FW_VERSION}" --out $(UBS_TARGET_FWPKG)

sign-download-sb3:
	if [ ! -f "$(SIGN_SB3_REQ)" ]; then \
		echo "No longer to sign-request automatically, please use mcu_build.sh instead."; \
		exit 1; \
	fi

	$(ubs-build) "SIGN SB3" "$(UBS_TARGET_SB3)"
	# Generate $PROJECT_signed.sb3
	REQ_ID=`jq '.requestId' $(SIGN_SB3_REQ)` && \
	echo "Donwloading signed firmware from request: $$REQ_ID" && \
	$(NVSEC) 3s download-signed-file --device_code $(SIGN_SSA) --request_id $$REQ_ID --download_path $(UBS_TARGET_SB3)

	# Generate PLDM fwpkg for mcu recovery fw according to each module
	$(ubs-build) "SB3_FWPKG" "$(UBS_TARGET_SB3_FWPKG)"
	echo "project config: $(PLDMPKG_RECOVERY_CFG)"; \

	@if [ -f "$(PLDMPKG_RECOVERY_CFG)" ]; then \
		echo "Using project config: $(PLDMPKG_RECOVERY_CFG)"; \
		${PLDMPKG} $(PLDMPKG_RECOVERY_CFG) $(UBS_TARGET_SB3) --apsku "${AP_SKU}" --ssdid "${SSDID}" --pkgver "${CHIP}PkgMcufw-${FW_VERSION}" --compver "${FW_VERSION}" --out $(UBS_TARGET_SB3_FWPKG); \
	else \
		echo "Not found config file, will not generate PLDM fwpkg for mcu recovery fw"; \
	fi

.NOTPARALLEL: sign-download-mbi sign-download-sb3
.PHONY: sign-download-mbi sign-download-sb3
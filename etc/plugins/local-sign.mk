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

SIGN_EXEC_PATH = libexec/tools/signing-service/src/scripts
SIGN_EXEC = signing_script_service.py
CHIP_CAP = $(shell echo $(CHIP) | tr '[:lower:]' '[:upper:]')
JOB_TYPE ?= SMA_$(CHIP_CAP)_PROD_S0
UBS_COMMANDS_RESULT += local-sign

local-sign:
	if [ ! -f "$(UBS_TARGET)" ]; then \
		./ubs build PROJECT=$(PROJECT) PLATFORM=$(PLATFORM) BOARD=$(BOARD) MODE=$(MODE); \
	fi

	# Generate binary file
	$(ubs-build) "OBJCOPY" "$(UBS_TARGET:.elf=.bin)"
	$(OBJCOPY) -Obinary $(UBS_TARGET) $(UBS_TARGET:.elf=.bin)

	$(ubs-info) "Local signing.\n"

	if ! (cd $(SIGN_EXEC_PATH) && python3 $(SIGN_EXEC) --env Dummy --inputFile $(abspath $(UBS_TARGET_BIN)) --resultFile $(abspath $(UBS_TARGET_SIGNED)) --jobType $(JOB_TYPE) --parameters $(abspath $(SIGN_MBI_CFG)) && cd -); then \
		echo "Error: Local signing bin failed"; \
		exit 1; \
	fi

	cp $(UBS_TARGET_SIGNED) $(dir $(UBS_TARGET_SIGNED))signed_mbi.bin
	if ! (cd $(SIGN_EXEC_PATH) && python3 $(SIGN_EXEC) --env Dummy --inputFile $(dir $(abspath ${UBS_TARGET_SIGNED}))signed_mbi.bin --resultFile $(abspath $(UBS_TARGET_SB3)) --jobType $(JOB_TYPE) --parameters $(abspath $(SIGN_SB3_CFG)) && cd -); then \
		echo "Error: Local signing sb3 failed"; \
		exit 1; \
	fi
	
	$(ubs-info) "Local signing complete.\n"

	# Generate PLDM fwpkg for mcu binary fw
	$(ubs-build) "FWPKG" "$(UBS_TARGET_FWPKG)"

	${PLDMPKG} $(PLDMPKG_CFG) $(UBS_TARGET_SIGNED) --apsku "${AP_SKU}" --ssdid "${SSDID}" --pkgver "${CHIP}PkgMcufw-${FW_VERSION}" --compver "${FW_VERSION}" --out $(UBS_TARGET_FWPKG)

.NOTPARALLEL: local-sign
.PHONY: local-sign
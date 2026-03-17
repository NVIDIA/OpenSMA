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
ifeq ($(CHIP), mcxn556)
SIGN_EXEC_PATH = libexec/tools/sma-mcxn556-signing-service/src/scripts
else ifeq ($(CHIP), mcxn236)
SIGN_EXEC_PATH = libexec/tools/sma-mcxn236-signing-service/src/scripts
else ifeq ($(CHIP), mcxn547)
SIGN_EXEC_PATH = libexec/tools/sma-mcxn236-signing-service/src/scripts
endif
SIGN_EXEC = signing_script_service.py
CHIP_CAP = $(shell echo $(CHIP) | tr '[:lower:]' '[:upper:]')
KEY_TYPE ?= DEBUG
JOB_TYPE ?= SMA_$(CHIP_CAP)_$(KEY_TYPE)_S0
UBS_COMMANDS_RESULT += local-sign

local-sign:
ifeq ($(STREAMBOOT), 1)
	#TODO: Should zip both SRAMX and SRAM binaries and sign them together. At present, we are signing SRAMX only and load SRAM binary via `write-memory` command.
	# For STREAMBOOT builds, ensure SRAMX_ELF exists and use it
	if [ ! -f "$(SRAMX_ELF)" ]; then \
		./ubs multi-bin PROJECT=$(PROJECT) PLATFORM=$(PLATFORM) BOARD=$(BOARD) MODE=$(MODE) STREAMBOOT=$(STREAMBOOT); \
	fi
	# Generate binary file from SRAMX_ELF
	$(ubs-build) "OBJCOPY" "$(UBS_STREAMBOOT_TARGET_BIN)"
	$(OBJCOPY) -Obinary $(SRAMX_ELF) $(UBS_STREAMBOOT_TARGET_BIN)
else
	if [ ! -f "$(UBS_TARGET)" ]; then \
		./ubs build PROJECT=$(PROJECT) PLATFORM=$(PLATFORM) BOARD=$(BOARD) MODE=$(MODE) STREAMBOOT=$(STREAMBOOT); \
	fi
	# Generate binary file from UBS_TARGET
	$(ubs-build) "OBJCOPY" "$(UBS_TARGET_BIN)"
	$(OBJCOPY) -Obinary $(UBS_TARGET) $(UBS_TARGET_BIN)
endif

	$(ubs-info) "Local signing.\n"

ifeq ($(STREAMBOOT), 1)
	# Use STREAMBOOT-specific files for signing
	if ! (cd $(SIGN_EXEC_PATH) && python3 $(SIGN_EXEC) --env Dummy --inputFile $(abspath $(UBS_STREAMBOOT_TARGET_BIN)) --resultFile $(abspath $(UBS_STREAMBOOT_TARGET_SIGNED)) --jobType $(JOB_TYPE) --parameters $(abspath $(SIGN_MBI_CFG)) && cd -); then \
		echo "Error: Local signing bin failed"; \
		exit 1; \
	fi

	cp $(UBS_STREAMBOOT_TARGET_SIGNED) $(dir $(UBS_STREAMBOOT_TARGET_SIGNED))signed_mbi.bin
	if ! (cd $(SIGN_EXEC_PATH) && python3 $(SIGN_EXEC) --env Dummy --inputFile $(dir $(abspath ${UBS_STREAMBOOT_TARGET_SIGNED}))signed_mbi.bin --resultFile $(abspath $(UBS_STREAMBOOT_TARGET_SB3)) --jobType $(JOB_TYPE) --parameters $(abspath $(SIGN_SB_CFG)) && cd -); then \
		echo "Error: Local signing sb failed"; \
		exit 1; \
	fi

	$(ubs-info) "Local signing complete.\n"

	# Generate PLDM fwpkg for mcu binary fw
	$(ubs-build) "FWPKG" "$(UBS_STREAMBOOT_TARGET_FWPKG)"
	${PLDMPKG} $(PLDMPKG_CFG) $(UBS_STREAMBOOT_TARGET_SIGNED) --apsku "${AP_SKU}" --ssdid "${SSDID}" --pkgver "${CHIP}PkgMcufw-${FW_VERSION}" --compver "${FW_VERSION}" --compstamp "${COMP_STAMP}" --out $(UBS_STREAMBOOT_TARGET_FWPKG)
else
	# Use regular files for signing
	if ! (cd $(SIGN_EXEC_PATH) && python3 $(SIGN_EXEC) --env Dummy --inputFile $(abspath $(UBS_TARGET_BIN)) --resultFile $(abspath $(UBS_TARGET_SIGNED)) --jobType $(JOB_TYPE) --parameters $(abspath $(SIGN_MBI_CFG)) && cd -); then \
		echo "Error: Local signing bin failed"; \
		exit 1; \
	fi

	cp $(UBS_TARGET_SIGNED) $(dir $(UBS_TARGET_SIGNED))signed_mbi.bin
	if ! (cd $(SIGN_EXEC_PATH) && python3 $(SIGN_EXEC) --env Dummy --inputFile $(dir $(abspath ${UBS_TARGET_SIGNED}))signed_mbi.bin --resultFile $(abspath $(UBS_TARGET_SB)) --jobType $(JOB_TYPE) --parameters $(abspath $(SIGN_SB_CFG)) && cd -); then \
		echo "Error: Local signing sb failed"; \
		exit 1; \
	fi

	$(ubs-info) "Local signing complete.\n"

	# Generate PLDM fwpkg for mcu binary fw
	$(ubs-build) "FWPKG" "$(UBS_TARGET_FWPKG)"
	${PLDMPKG} $(PLDMPKG_CFG) $(UBS_TARGET_SIGNED) --apsku "${AP_SKU}" --ssdid "${SSDID}" --pkgver "${CHIP}PkgMcufw-${FW_VERSION}" --compver "${FW_VERSION}" --compstamp "${COMP_STAMP}" --out $(UBS_TARGET_FWPKG)
endif

.NOTPARALLEL: local-sign
.PHONY: local-sign

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



NVSEC                := /usr/local/bin/nvsec
PROJECT_PLATFORM     := etc/platforms
PLDMPKG_CFG          := $(PROJECT_PLATFORM)/pldmfw-cfg-mcu-fw.json
PLDMPKG_RECOVERY_CFG := $(PROJECT_PLATFORM)/pldmfw-cfg-mcu-recovery-fw-$(PROJECT).json
PLDMPKG              := libexec/tools/pldmfwpkg-gen.py
ENABLE_PQC_SIGN      ?= false

UBS_TARGET_BIN	      = $(UBS_TARGET:.elf=.bin)
UBS_TARGET_SIGNED     = $(UBS_TARGET:.elf=_signed.bin)
ifeq ($(CHIP), mcxn547)
UBS_TARGET_SB	      = $(UBS_TARGET:.elf=_signed.sb3)
UBS_TARGET_SB_FWPKG  = $(UBS_TARGET:.elf=_signed_sb3.fwpkg)
else ifeq ($(CHIP), mcxn236)
UBS_TARGET_SB	      = $(UBS_TARGET:.elf=_signed.sb3)
UBS_TARGET_SB_FWPKG  = $(UBS_TARGET:.elf=_signed_sb3.fwpkg)
else ifeq ($(CHIP), mcxn556)
UBS_TARGET_SB	      = $(UBS_TARGET:.elf=_signed.sb4)
UBS_TARGET_SB_FWPKG  = $(UBS_TARGET:.elf=_signed_sb4.fwpkg)
endif
UBS_TARGET_FWPKG      = $(UBS_TARGET:.elf=_signed.fwpkg)

# STREAMBOOT-specific variables (defined conditionally to avoid dependency issues)
# Only define these when STREAMBOOT=1, and they will be used by local-sign.mk
ifeq ($(STREAMBOOT), 1)
UBS_STREAMBOOT_TARGET_BIN      = $(UBS_TARGET:.elf=_sramx.bin)
UBS_STREAMBOOT_TARGET_SIGNED   = $(UBS_TARGET:.elf=_sramx_signed.bin)
UBS_STREAMBOOT_TARGET_SB3      = $(UBS_TARGET:.elf=_sramx_signed.sb3)
UBS_STREAMBOOT_TARGET_SB3_FWPKG = $(UBS_TARGET:.elf=_sramx_signed_sb3.fwpkg)
UBS_STREAMBOOT_TARGET_FWPKG    = $(UBS_TARGET:.elf=_sramx_signed.fwpkg)
endif

SIGN_TYPE      ?= debug
ifeq ($(CHIP), mcxn556)
SIGN_JOB       = $(addprefix SMA_MCXN556$(if $(filter true,$(ENABLE_PQC_SIGN)),_PQC,)_,$(shell echo $(SIGN_TYPE) | tr [:lower:] [:upper:]))
else
SIGN_JOB       = $(addprefix SMA_MCXN236_,$(shell echo $(SIGN_TYPE) | tr [:lower:] [:upper:])_$(SIGN_KEYSET))
endif

SIGN_DESC	   = "MCU signing for dev testing"
SIGN_MBI_REQ   = $(UBS_PATH_OUT)/request_id_mbi.json
SIGN_SB_REQ   = $(UBS_PATH_OUT)/request_id_sb.json
ifeq ($(STREAMBOOT), 1)
	SIGN_MBI_CFG = $(PATH_PROJECT)/sign-parameters_mbi_$(CHIP)_streamboot.json
	SIGN_SB_CFG = $(PATH_PROJECT)/sign-parameters_sb_$(CHIP)_streamboot.json
else
	SIGN_MBI_CFG = $(PATH_PROJECT)/sign-parameters_mbi_$(CHIP).json
	SIGN_SB_CFG = $(PATH_PROJECT)/sign-parameters_sb_$(CHIP).json
endif
SIGN_FLAG	   = 3s submit --job_type $(SIGN_JOB) --description $(SIGN_DESC) --device_code
SIGN_REQ_ID	   = $(SIGN_MBI_REQ) $(SIGN_SB_REQ)
SIGN_SSA_SCOPE = $(addprefix SIGNING_,$(SIGN_JOB))
SIGN_SSA       = $(if $(SSA_CLIENT_ID),--client_id $(SSA_CLIENT_ID) --client_secret $(SSA_CLIENT_SECRET) --scope $(SIGN_SSA_SCOPE),)

UBS_COMMANDS_RESULT += sign-request-mbi sign-request-sb

sign-request-mbi:
	if [ ! -f "$(UBS_TARGET)" ]; then \
		if [ $(STREAMBOOT) -eq 1 ]; then \
			./ubs build PROJECT=$(PROJECT) PLATFORM=$(PLATFORM) BOARD=$(BOARD) MODE=$(MODE) STREAMBOOT=1; \
		else \
			./ubs build PROJECT=$(PROJECT) PLATFORM=$(PLATFORM) BOARD=$(BOARD) MODE=$(MODE); \
		fi; \
	fi

ifeq ($(STREAMBOOT), 1)
	# Generate binary file from SRAMX_ELF
	$(ubs-build) "OBJCOPY" "$(UBS_STREAMBOOT_TARGET_BIN)"
	$(OBJCOPY) -Obinary $(SRAMX_ELF) $(UBS_STREAMBOOT_TARGET_BIN)
	$(eval SIGN_INPUT_FILE := $(UBS_STREAMBOOT_TARGET_BIN))
else
	# Generate binary file from UBS_TARGET
	$(ubs-build) "OBJCOPY" "$(UBS_TARGET_BIN)"
	$(OBJCOPY) -Obinary $(UBS_TARGET) $(UBS_TARGET_BIN)
	$(eval SIGN_INPUT_FILE := $(UBS_TARGET_BIN))
endif

	$(ubs-build) "SIGN_JOB" "$(SIGN_JOB)"
	$(ubs-build) "REQ MBI" "$(SIGN_MBI_REQ)"
	# Generate request_id_mbi.json
	$(NVSEC) $(SIGN_FLAG) $(SIGN_SSA) --request_id_file $(SIGN_MBI_REQ) --input_file $(SIGN_INPUT_FILE) --parameters_file $(SIGN_MBI_CFG)

sign-request-sb:
	if [ ! -f "$(UBS_TARGET_SIGNED)" ]; then \
		echo "No longer to sign-request automatically, please use mcu_build.sh instead."; \
		exit 1; \
	fi

	$(ubs-build) "SIGN_JOB" "$(SIGN_JOB)"
	$(ubs-build) "REQ SB" "$(SIGN_SB_REQ)"
	# Generate request_id_sb.json
	cp $(UBS_TARGET_SIGNED) $(dir $(UBS_TARGET_SIGNED))signed_mbi.bin
	$(NVSEC) $(SIGN_FLAG) $(SIGN_SSA) --request_id_file $(SIGN_SB_REQ) --input_file $(dir $(UBS_TARGET_SIGNED))signed_mbi.bin --parameters_file $(SIGN_SB_CFG)

.NOTPARALLEL: sign-request-mbi sign-request-sb
.PHONY: sign-request-mbi sign-request-sb
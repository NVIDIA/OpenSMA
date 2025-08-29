
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

UBS_TARGET_BIN	      = $(UBS_TARGET:.elf=.bin)
UBS_TARGET_SIGNED     = $(UBS_TARGET:.elf=_signed.bin)
UBS_TARGET_SB3	      = $(UBS_TARGET:.elf=_signed.sb3)
UBS_TARGET_SB3_FWPKG  = $(UBS_TARGET:.elf=_signed_sb3.fwpkg)
UBS_TARGET_FWPKG      = $(UBS_TARGET:.elf=_signed.fwpkg)

SIGN_TYPE      ?= debug
SIGN_JOB       = $(addprefix SMA_MCXN236_,$(shell echo $(SIGN_TYPE) | tr [:lower:] [:upper:])_$(SIGN_KEYSET))
SIGN_DESC	   = "MCU signing for dev testing"
SIGN_MBI_REQ   = $(UBS_PATH_OUT)/request_id_mbi.json
SIGN_SB3_REQ   = $(UBS_PATH_OUT)/request_id_sb3.json
ifeq ($(GD_MCXN547_BUILD), 1)
	SIGN_MBI_CFG = $(PATH_PROJECT)/sign-parameters_mbi_mcxn547.json
	SIGN_SB3_CFG = $(PATH_PROJECT)/sign-parameters_sb3_mcxn547.json
else
	SIGN_MBI_CFG = $(PATH_PROJECT)/sign-parameters_mbi.json
	SIGN_SB3_CFG = $(PATH_PROJECT)/sign-parameters_sb3.json
endif
SIGN_FLAG	   = 3s submit --job_type $(SIGN_JOB) --description $(SIGN_DESC) --device_code
SIGN_REQ_ID	   = $(SIGN_MBI_REQ) $(SIGN_SB3_REQ)
SIGN_SSA_SCOPE = $(addprefix SIGNING_,$(SIGN_JOB))
SIGN_SSA       = $(if $(SSA_CLIENT_ID),--client_id $(SSA_CLIENT_ID) --client_secret $(SSA_CLIENT_SECRET) --scope $(SIGN_SSA_SCOPE),)

UBS_COMMANDS_RESULT += sign-request-mbi sign-request-sb3

sign-request-mbi:
	if [ ! -f "$(UBS_TARGET)" ]; then \
		./ubs build PROJECT=$(PROJECT) PLATFORM=$(PLATFORM) BOARD=$(BOARD) MODE=$(MODE); \
	fi

	# Generate binary file
	$(ubs-build) "OBJCOPY" "$(UBS_TARGET:.elf=.bin)"
	$(OBJCOPY) -Obinary $(UBS_TARGET) $(UBS_TARGET:.elf=.bin)
	
	$(ubs-build) "REQ MBI" "$(SIGN_MBI_REQ)"
	# Generate request_id_mbi.json
	$(NVSEC) $(SIGN_FLAG) $(SIGN_SSA) --request_id_file $(SIGN_MBI_REQ) --input_file $(UBS_TARGET_BIN) --parameters_file $(SIGN_MBI_CFG)

sign-request-sb3:
	if [ ! -f "$(UBS_TARGET_SIGNED)" ]; then \
		echo "No longer to sign-request automatically, please use mcu_build.sh instead."; \
		exit 1; \
	fi

	$(ubs-build) "REQ SB3" "$(SIGN_SB3_REQ)"
	# Generate request_id_sb3.json
	cp ${UBS_TARGET_SIGNED} signed_mbi.bin
	$(NVSEC) $(SIGN_FLAG) $(SIGN_SSA) --request_id_file $(SIGN_SB3_REQ) --input_file signed_mbi.bin --parameters_file $(SIGN_SB3_CFG)

.NOTPARALLEL: sign-request-mbi sign-request-sb3
.PHONY: sign-request-mbi sign-request-sb3
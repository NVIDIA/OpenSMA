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

REMOTE      ?= 0
PI_PATH     ?= /tmp
SSH_HOST    := pi@mcu-556-tw-$(shell printf "%02d" $(REMOTE))
CMD_FLASH   := blhost -t 5000 -u 0x0955,0xcf1d receive-sb-file $(PI_PATH)/$(notdir $(UBS_TARGET_SB))
SSH         := sshpass -p labuser ssh -o StrictHostKeyChecking=no
RSYNC       := sshpass -p labuser rsync -e "ssh -o StrictHostKeyChecking=no"

UBS_COMMANDS_RESULT += flash

flash:
	if [ ! -f "$(UBS_TARGET_SB)" ]; then \
		./ubs sign-download PROJECT=$(PROJECT) PLATFORM=$(PLATFORM) BOARD=$(BOARD) MODE=$(MODE); \
	fi

	$(ubs-info) "Remote syncing binaries to RPI.\n"
	$(RSYNC) --archive --human-readable --progress $(UBS_TARGET_SB) $(SSH_HOST):$(PI_PATH)
	$(ubs-info) "Upload complete.\n"

	$(ubs-info) "Flashing remote $(SSH_HOST)\n"
	$(SSH) $(SSH_HOST) mcu_isp_enter
	$(SSH) $(SSH_HOST) "$(CMD_FLASH)"
	$(SSH) $(SSH_HOST) "mcu_reset && sleep 1"

	$(ubs-info) "List usb device\n"
	$(SSH) $(SSH_HOST) lsusb

	$(ubs-info) "flash complete.\n"

.PHONY: flash
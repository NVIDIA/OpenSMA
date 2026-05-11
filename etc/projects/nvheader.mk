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



NVHEADER_TMPL := etc/platforms/nvheader-cfg.json
NVHEADER_CFG  := $(addprefix $(UBS_PATH_OUT)/,$(notdir $(NVHEADER_TMPL)))
NVHEADER_SRC  := $(UBS_PATH_OUT)/nvheader.c
NVHEADER      := libexec/tools/nvheader-gen.py

$(NVHEADER_CFG): $(NVHEADER_TMPL)
	@$(JQ) '.data.FwVersion = "$(FW_VERSION)" | .data.ApSkuId = "$(AP_SKU)" | .data.PciSubsystemDeviceId = "$(SSDID)" | .data.BuildMode = $(GD_NV_BUILD_MODE)' $^ > $@

$(NVHEADER_SRC): $(NVHEADER_CFG)
	$(ubs-build) "HEADER" "$@"
	@$(NVHEADER) $^ $@
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

# -- Global Variable Section Placement (PDK.25 / UBS-SA.1) -------------------------------------
# This plugin defines the default section placement for global variables in pdk::cmn modules.
# Projects can override these values in their configuration file to place globals in specific
# memory regions (e.g., retention RAM, DMA-accessible memory, MPU-protected regions).
#
# See: doc/adr/0009-global-section-placement.adoc

UBS_COMMANDS_RESULT += sa-global-variables

# Base default - all module-specific defaults derive from this
GD_PDK_CMN_GLOBAL         ?= '".data"'
# Module-specific defaults
GD_PDK_CMN_LOG_GLOBAL     ?= $(GD_PDK_CMN_GLOBAL)
GD_PDK_CMN_CONSOLE_GLOBAL ?= $(GD_PDK_CMN_GLOBAL)

sa-global-variables:
	$(ubs_banner_str 'SA-GLOBAL-VARIABLES')
	$(ubs-info) "Checking global variable section placement...\n"
	$(ubs-info) "Base default: $(GD_PDK_CMN_GLOBAL)\n"
	$(ubs-info) "Log default: $(GD_PDK_CMN_LOG_GLOBAL)\n"
	$(ubs-info) "Console default: $(GD_PDK_CMN_CONSOLE_GLOBAL)\n"
	$(ubs-warn) "Not implemented yet\n"
	$(ubs-line_str '_')
	
.PHONY: sa-global-variables
.SILENT: sa-global-variables
.NOTPARALLEL: sa-global-variables
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



SPDM_MODULE_PLATFORM_SPECIFIC_PATH := corepdk/platforms/mcxn236/spdm/
# Link the .h file in SPDM_MODULE_PLATFORM_SPECIFIC_PATH first
INCLUDES += $(SPDM_MODULE_PLATFORM_SPECIFIC_PATH)

SPDM_MODULE_ADA_RESPONSDER_PATH := corepdk/modules/spdm/src/app/spdm_responder_ada_library
# Add CorePDK Platform Specific Source Files
SOURCES += $(shell find $(SPDM_MODULE_PLATFORM_SPECIFIC_PATH) \
			\( -name "*.cpp" -or -name "*.c" \) \
           -not -name "*-test.cpp" -not -name "*-test.c")

SOURCES += $(shell find $(SPDM_MODULE_ADA_RESPONSDER_PATH) \
           \( -name "*.adb" -or -name "*.ads" \) \
           -not -name "*-test.adb" -not -name "*-test.ads" )

SPDM_MODULE_PATH := corepdk/modules/spdm/src/app

SOURCES += $(shell find $(SPDM_MODULE_PATH) \
			\( -name "*.cpp" -or -name "*.c" \) \
           -not -name "*-test.cpp" -not -name "*-test.c")
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



PLDM_PLATFORM_SPECIFIC_PATH := corepdk/platforms/mcxn236/pldm-fd/src/
# INCLUDES += corepdk/platforms/mcxn236/
INCLUDES        += ./

# Add CorePDK Platform Specific Source Files
SOURCES += $(shell find $(PLDM_PLATFORM_SPECIFIC_PATH) \
           \( -name "*.cpp" -or -name "*.c" \) -not -name "*-test.adb" -not -name "*-test.ads" -not -name "*-test.cpp")

SOURCES += $(shell find $(PLDM_PLATFORM_SPECIFIC_PATH) \
           \( -name "*.adb" -or -name "*.ads" \) -not -name "*-test.adb" -not -name "*-test.ads" -not -name "*-test.cpp")
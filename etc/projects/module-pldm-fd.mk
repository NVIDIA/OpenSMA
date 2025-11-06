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



# INCLUDES += corepdk/modules

PLDM_MODULE_PATH := corepdk/modules/pldm-fd/src
UBS_SRC         := corepdk/ubs/src
UBS_SRC_PDK     := $(UBS_SRC)/pdk

SOURCES += $(PLDM_MODULE_PATH)/pdk-pldm.ads

# SOURCES += $(shell find $(UBS_SRC_PDK) \( -name "*.adb" -or -name "*.ads" \) -not -name "*-test.adb" -not -name "*-test.ads")

SOURCES += $(UBS_SRC_PDK)/pdk.ads

SOURCES += $(shell find $(PLDM_MODULE_PATH)/app \
           \( -name "*.adb" -or -name "*.ads" \) -not -name "*-test.adb" -not -name "*-test.ads" -not -name "*-test.cpp")

SOURCES += $(shell find $(PLDM_MODULE_PATH)/base \
           \( -name "*.adb" -or -name "*.ads" \) -not -name "*-test.adb" -not -name "*-test.ads" -not -name "*-test.cpp")

SOURCES += $(shell find $(PLDM_MODULE_PATH)/device \
           \( -name "*.adb" -or -name "*.ads" \) -not -name "*-test.adb" -not -name "*-test.ads" -not -name "*-test.cpp")

SOURCES += $(shell find $(PLDM_MODULE_PATH)/fwupdate \
           \( -name "*.adb" -or -name "*.ads" \) -not -name "*-test.adb" -not -name "*-test.ads" -not -name "*-test.cpp")

SOURCES += $(shell find $(PLDM_MODULE_PATH)/types \
           \( -name "*.adb" -or -name "*.ads" \) -not -name "*-test.adb" -not -name "*-test.ads" -not -name "*-test.cpp")

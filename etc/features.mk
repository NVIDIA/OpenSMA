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




# TODO: add macro

FEATURE_reg_table   ?= 0
UNITTESTS_reg_table := $(wildcard $(UBS_PATH_SRC)/nv/reg_table/*-test.cpp)
SOURCES_reg_table   := $(wildcard $(UBS_PATH_SRC)/nv/reg_table/*.cpp)
SOURCES_reg_table   := $(filter-out $(UNITTESTS_reg_table),$(SOURCES_reg_table))

FEATURE_reg_ds   ?= 0
UNITTESTS_reg_ds := $(wildcard $(UBS_PATH_SRC)/nv/reg_ds/*-test.cpp)
SOURCES_reg_ds   := $(wildcard $(UBS_PATH_SRC)/nv/reg_ds/*.cpp)
SOURCES_reg_ds   := $(filter-out $(UNITTESTS_reg_ds),$(SOURCES_reg_ds))

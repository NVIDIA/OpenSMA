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


UBS_PLUGIN ?= lib/ubs.so

ifdef UBS_PRINT

load $(UBS_PLUGIN)

else

bootstrap: $(UBS_PLUGIN)

$(UBS_PLUGIN): $(UBS_PATH_ROOT)/ubs.cpp
	@echo "bootstrapping UBS plugin..."
	@[ -d lib ] || mkdir -p lib
	@g++-13 -std=c++23 -fPIC -D__USE_MISC -shared $(shell python3-config --cflags) \
	    -o $@ $< $(shell python3-config --ldflags --embed) -g -O0

.PHONY: bootstrap
.SILENT: $(UBS_PLUGIN) boostrap

endif



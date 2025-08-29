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


UBS_COMMANDS_NO_RESULT += ls files

ls:
	$(ubs_banner_str 'xLISTING')
	$(ubs-info) "Available Projects:\n"
	$(ubs_list_str 'projects')
	$(ubs_line_str '.f')
	$(ubs-info) "Available Platforms:\n"
	$(ubs_list_str 'platforms')
	$(ubs_line_str '_')

files:
	$(ubs_banner_str 'xFILE-LIST')
	$(ubs-info) "Source files:\n"
	$(ubs_list_str 'sources')
	$(ubs_line_str '_')


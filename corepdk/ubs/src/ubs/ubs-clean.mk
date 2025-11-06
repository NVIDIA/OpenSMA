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



UBS_COMMANDS_NO_RESULT += clean distclean

clean:
	$(ubs_banner_str 'CLEAN')
	$(ubs-info) "removing intermediate files for '$(UBS_PROJECT)'"
	$(UBS_RM) -rf $(UBS_PATH_OBJ) 
	$(UBS_RM) -rf $(UBS_PATH_LIBS_GEN)
	$(ubs-done)
	$(ubs_line_str '_')


distclean:
	$(ubs_banner_str 'DISTCLEAN')
	$(ubs-info) "removing all build files for all projects"
	$(UBS_RM) -rf $(UBS_PATH_BUILD)
	$(UBS_RM) compile_flags.txt $(UBS_GPR_PROJECT)
	$(ubs-done)
	$(ubs_line_str '_')


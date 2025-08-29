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



# UBS_COMMANDS_HELP is defined in ubs-config.h
# help commands map from help-$1 to doc/man/ubs-$1.adoc


$(UBS_COMMANDS_HELP):
	$(ubs_banner_str '$@'.upper())
	$(UBS_ASCIIDOCTOR) -b manpage --out-file - $(UBS_PATH_MAN)/ubs-$(subst help-,,$@).adoc \
	    | GROFF_SGR=1 $(UBS_GROFF) -mandoc -Z -Tutf8 -rLL=$(UBS_WIDTH)n -rLT=$(UBS_WIDTH)n -rU1 - \
	    | $(UBS_GROTTY) -i -r 
	$(ubs_line_str '_')



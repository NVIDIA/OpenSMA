# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
# All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# Purpose: capture and format the final memory-usage table produced during
# LINK. The wrapper forwards the original LINK command and arguments unchanged,
# keeps the normal linker output visible, and does not alter the linked image.
#
# Example table being captured:
#   Memory region	Used Size  		Region Size  	%age Used
#   m_text:      	311824 B		383 KB     		79.51%
#   m_data:      	199704 B		375 KB     		52.01%
#
# Example flow:
#   normal LINK -> firmware image + visible linker output
#               -> capture final memory-usage table -> structured storage
#
# Include this fragment after a platform defines the real LINK command.
# ubs-platform.mk resolves LINK to UBS_LINK after the platform file returns;
# this position therefore preserves the real command in UBS_REAL_LINK while
# making UBS invoke the wrapper.
ifndef LINKER_MEMORY_USAGE_INCLUDED
LINKER_MEMORY_USAGE_INCLUDED := 1
  export UBS_REAL_LINK := $(LINK)
  LINK := $(CURDIR)/libexec/cicd/ubs_link_tee.sh
endif

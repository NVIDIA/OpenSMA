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

# Multi-Binary Generation Plugin
# Creates separate binary files for different memory ranges

# Define memory ranges based on linker script - 2 contiguous main memory regions
# SRAMX: 0x04000000 region (Main SRAM for fast code execution)
SRAMX_BASE           := 0x04000000
SRAMX_END            := 0x04017FFF
SRAMX_BIN            = $(UBS_TARGET:.elf=_sramx.bin)
SRAMX_ELF            = $(UBS_TARGET:.elf=_sramx.elf)

# SRAM: 0x20000000 region (Secondary SRAM for the rest of the code storage)
SRAM_BASE            := 0x20000000
SRAM_END             := 0x2005FFFF
SRAM_BIN             = $(UBS_TARGET:.elf=_sram.bin)
MEMORY_MAP_FILE      = $(UBS_TARGET:.elf=_memory_map.txt)

UBS_COMMANDS_RESULT += multi-bin

multi-bin:
	# Ensure the main ELF exists
	if [ ! -f "$(UBS_TARGET)" ]; then \
		./ubs build PROJECT=$(PROJECT) PLATFORM=$(PLATFORM) BOARD=$(BOARD) MODE=$(MODE) STREAMBOOT=$(STREAMBOOT); \
	fi

	# Create temporary files for section lists with LMA information (only LOAD sections)
	@$(OBJDUMP) -h $(UBS_TARGET) | grep -A1 "^[ ]*[0-9]" | grep -B1 "LOAD" | awk '/^[ ]*[0-9]+/ && $$2 ~ /^\./ && $$5 != "00000000" { printf "%s %s %s\n", $$2, $$5, $$3 }' > $(UBS_PATH_OUT)/sections_temp.txt

	# Generate SRAMX files (0x04000000 region - Main SRAM for fast code execution)
	$(ubs-build) "MULTI-BIN" "SRAMX: $(SRAMX_BIN) + $(SRAMX_ELF)"
	@echo "Extracting sections for SRAMX ($(SRAMX_BASE)-$(SRAMX_END))..."
	@SECTIONS=$$(awk 'BEGIN{base=strtonum("$(SRAMX_BASE)"); end=strtonum("$(SRAMX_END)")} {addr=strtonum("0x" $$2); if(addr >= base && addr <= end) print "--only-section=" $$1}' $(UBS_PATH_OUT)/sections_temp.txt | tr '\n' ' '); \
	if [ -n "$$SECTIONS" ]; then \
		echo "  Sections: $$SECTIONS"; \
		echo "  Creating ELF: $(SRAMX_ELF)"; \
		$(OBJCOPY) $$SECTIONS $(UBS_TARGET) $(SRAMX_ELF) 2>/dev/null; \
		echo "  Creating binary: $(SRAMX_BIN)"; \
		$(OBJCOPY) -O binary $$SECTIONS --gap-fill=0x00 $(UBS_TARGET) $(SRAMX_BIN); \
	else \
		echo "  No sections found in this range"; \
		touch $(SRAMX_BIN) $(SRAMX_ELF); \
	fi

	# Generate SRAM binary (0x20000000 region - Secondary SRAM for the rest of the code storage)
	$(ubs-build) "MULTI-BIN" "SRAM: $(SRAM_BIN)"
	@echo "Extracting sections for SRAM ($(SRAM_BASE)-$(SRAM_END))..."
	@SECTIONS=$$(awk 'BEGIN{base=strtonum("$(SRAM_BASE)"); end=strtonum("$(SRAM_END)")} {addr=strtonum("0x" $$2); if(addr >= base && addr <= end) print "--only-section=" $$1}' $(UBS_PATH_OUT)/sections_temp.txt | tr '\n' ' '); \
	if [ -n "$$SECTIONS" ]; then \
		echo "  Sections: $$SECTIONS"; \
		$(OBJCOPY) -O binary $$SECTIONS --gap-fill=0x00 $(UBS_TARGET) $(SRAM_BIN); \
	else \
		echo "  No sections found in this range"; \
		touch $(SRAM_BIN); \
	fi

	# Generate comprehensive info file with memory ranges and flash commands
	$(ubs-build) "INFO" "$(MEMORY_MAP_FILE)"
	@echo "Multi-Binary Memory Map for $(PROJECT)" > $(MEMORY_MAP_FILE)
	@echo "=======================================" >> $(MEMORY_MAP_FILE)
	@echo "Generated: `date`" >> $(MEMORY_MAP_FILE)
	@echo "" >> $(MEMORY_MAP_FILE)

	@echo "SRAMX ($(SRAMX_BASE) - $(SRAMX_END)): Fast SRAMX (m_interrupts + m_header + m_text)" >> $(MEMORY_MAP_FILE)
	@echo "  Binary: $(SRAMX_BIN)" >> $(MEMORY_MAP_FILE)
	@echo "  Size: `stat -c%s $(SRAMX_BIN) 2>/dev/null || echo 0` bytes" >> $(MEMORY_MAP_FILE)
	@awk 'BEGIN{base=strtonum("$(SRAMX_BASE)"); end=strtonum("$(SRAMX_END)")} {addr=strtonum("0x" $$2); if(addr >= base && addr <= end) printf "  Section %s: 0x%s (size: 0x%s)\n", $$1, $$2, $$3}' $(UBS_PATH_OUT)/sections_temp.txt >> $(MEMORY_MAP_FILE) || true
	@echo "  Load command: blhost -u 0x0955:0xcf1f receive-sb-file p2020_sramx_signed.sb3" >> $(MEMORY_MAP_FILE)
	@echo "" >> $(MEMORY_MAP_FILE)

	@echo "SRAM ($(SRAM_BASE) - $(SRAM_END)): SRAM (m_FMC + m_data + m_text2)" >> $(MEMORY_MAP_FILE)
	@echo "  Binary: $(SRAM_BIN)" >> $(MEMORY_MAP_FILE)
	@echo "  Size: `stat -c%s $(SRAM_BIN) 2>/dev/null || echo 0` bytes" >> $(MEMORY_MAP_FILE)
	@awk 'BEGIN{base=strtonum("$(SRAM_BASE)"); end=strtonum("$(SRAM_END)")} {addr=strtonum("0x" $$2); if(addr >= base && addr <= end) printf "  Section %s: 0x%s (size: 0x%s)\n", $$1, $$2, $$3}' $(UBS_PATH_OUT)/sections_temp.txt >> $(MEMORY_MAP_FILE) || true
	@echo "  Load command: blhost -u 0x0955:0xcf1f write-memory 0x20033000 $(SRAM_BIN)" >> $(MEMORY_MAP_FILE)
	@echo "" >> $(MEMORY_MAP_FILE)

	@echo "Summary:" >> $(MEMORY_MAP_FILE)
	@echo "========" >> $(MEMORY_MAP_FILE)
	@echo "Total binaries: 2" >> $(MEMORY_MAP_FILE)
	@echo "Total size: `expr \`stat -c%s $(SRAMX_BIN) 2>/dev/null || echo 0\` + \`stat -c%s $(SRAM_BIN) 2>/dev/null || echo 0\`` bytes" >> $(MEMORY_MAP_FILE)

	$(ubs-info) "Generated multi-binary files:\n"
	$(ubs-info) "  SRAMX ($(SRAMX_BASE)): $(SRAMX_BIN) (`stat -c%s $(SRAMX_BIN) 2>/dev/null || echo 0` bytes) + $(SRAMX_ELF) - SRAMX\n"
	$(ubs-info) "  SRAM ($(SRAM_BASE)): $(SRAM_BIN) (`stat -c%s $(SRAM_BIN) 2>/dev/null || echo 0` bytes) - non-SRAMX\n"
	$(ubs-info) "  Memory map: $(MEMORY_MAP_FILE)\n"

	# Clean up temp file
	@rm -f $(UBS_PATH_OUT)/sections_temp.txt

.NOTPARALLEL: multi-bin
.PHONY: multi-bin
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


################################################################################
# Optional: import symbols from an existing ELF (plugin use-case)
# Projects can set UBS_BASE_ELF to the original firmware ELF; linker will
# resolve undefineds using that symbol table without altering memory layout.
################################################################################
ifneq ($(UBS_BASE_ELF),)
UBS_BASE_ELF_FLAG := --just-symbols=$(UBS_BASE_ELF)
# GCC/G++ need -Wl, to forward to ld; if UBS_LINK is ld already, pass raw.
ifeq (,$(findstring ld,$(notdir $(UBS_LINK))))
UBS_LINK_FLAGS += -Wl,$(UBS_BASE_ELF_FLAG)
else
UBS_LINK_FLAGS += $(UBS_BASE_ELF_FLAG)
endif
endif


build: config .WAIT build-banner .WAIT $(UBS_TARGETS_ALL) .WAIT $(if $(filter library,$(UBS_TYPE)), lib_gen,)

build-banner:
	$(ubs_banner_str 'BUILD')


$(UBS_TARGET): $(UBS_OBJECTS) $(UBS_LINKER_SCRIPT) $(UBS_LIBS_AS)
	$(ubs_banner_str 'LINK')
	@$(if $(filter library,$(UBS_TYPE)), \
		$(ubs-info) "Skipping link as UBS_TYPE=library\n", \
		$(ubs-build) "LINK" "$@"; $(UBS_LINK) $(UBS_LINK_FLAGS) $(UBS_OBJECTS) $(UBS_LIBS_AS) -T $(UBS_LINKER_SCRIPT) -o $@ $(UBS_LIBS))
	@$(if $(filter library,$(UBS_TYPE)), , \
		$(ubs-build) "OBJDUMP" "$(@:.elf=.objdump)"; \
		$(OBJDUMP) --disassemble --source --demangle=auto $@ > $(@:.elf=.objdump) || true)
	@$(if $(filter library,$(UBS_TYPE)), ,\
		$(ubs-build) "READELF" "$(@:.elf=.readelf)"; \
		$(READELF) --all --wide --demangle=auto --lint $@ > $(@:.elf=.readelf) || true)
	$(ubs_line_str '_')

# usage: $(call ubs-compile-tag-flags compiler,tag,flags)
define ubs-compile-tag-flags
$(call ubs-create-target-folder)
$(ubs-build) "$(2)" "$<"
@$(UBS_COMPILE_PRE) $(UBS_$(1)) $(3) $(UBS_DEP_FLAGS) -c $< -o $@
endef

# usage: $(call ubs-compile-flags compiler,flags) 
ubs-compile-flags = $(call ubs-compile-tag-flags,$(1),$(1),$(2))

# usage: $(call ubs-compile compiler) 
ubs-compile       = $(call ubs-compile-tag-flags,$(1),$(1),$(UBS_$(1)_FLAGS))

define ubs-adadep
@echo "$@ : \\" > $@.d
@for f in `$(UBS_SED) -rne 's#D (\S+).+#\1#p' $(@:.o=.ali)`; do \
    for d in $(UBS_ADA_INCLUDES); do                            \
        [ -f $$d/$$f ] && echo "  $$d$$f \\" >> $@.d || true;   \
    done;                                                       \
done;
endef


# c/c++ unittest filter

$(UBS_PATH_OBJ)/%$(UBS_UNITTEST_SUFFIX).cpp.o : %$(UBS_UNITTEST_SUFFIX).cpp
	$(call ubs-compile-tag-flags,CXX,UNIT,$(UBS_CXX_FLAGS))

# add dependency rules for any unittest ads files
$(foreach f,$(filter %.adb,$(UBS_UNITTEST_TEST_FILES)),\
	$(eval $(f): $(UBS_PATH_GEN)/$(f:.adb=.ads)))

# compile an ASM file
$(UBS_PATH_OBJ)/%.S.o: %.S
	$(call ubs-compile-tag-flags,CC,ASM,$(UBS_CC_FLAGS))

# compile c++ file
$(UBS_PATH_OBJ)/%.cpp.o : %.cpp
	$(call ubs-compile,CXX)

# compile c file
$(UBS_PATH_OBJ)/%.c.o : %.c
	$(call ubs-compile,CC)

# compile Ada body files
$(UBS_PATH_OBJ)/%.o $(UBS_PATH_OBJ)/%.ali &:: %.adb
	$(call ubs-compile,ADA)
	$(call ubs-adadep)

# compile Ada spec files with no body
$(UBS_PATH_OBJ)/%.o $(UBS_PATH_OBJ)/%.ali &:: %.ads
	$(call ubs-compile,ADA)
	$(call ubs-adadep)

# ada binder
$(UBS_SRCS_ADA_BINDER): $(filter-out $(UBS_SRCS_ADA_BINDER:.adb=.o),$(UBS_OBJECTS))
	$(call ubs-create-target-folder)
	$(ubs-build) "BIND" "$@"
	@$(UBS_BIND) $(UBS_BIND_FLAGS) -x -o $@ $(UBS_ALIS)
	@$(UBS_SED) -ie 's#$(basename $@)#$(notdir $(basename $@))#' $@

# ada binder object
$(UBS_SRCS_ADA_BINDER:.adb=.o): $(UBS_SRCS_ADA_BINDER)
	$(ubs-build) "BINDCC" "$@"
	@$(UBS_ADA) $(UBS_ADA_FLAGS) -o $@ -c $<

# build static library project
lib_gen: $(UBS_LIBS_GEN) $(UBS_OBJECTS)
	$(ubs_banner_str 'LIB')
	$(call ubs-create-folder, $(UBS_PATH_LIBS_GEN))
	$(ubs-build) "AR" "$(UBS_PATH_LIBS_GEN)/$(UBS_PROJECT).a"
	@ar rcs $(UBS_PATH_LIBS_GEN)/$(UBS_PROJECT).a $(UBS_OBJECTS)
	$(ubs-info) "Library is generated in $(UBS_PATH_LIBS_GEN)\n"
	$(UBS_CP) $(UBS_LIBS_GEN) $(UBS_PATH_LIBS_GEN)/
	$(ubs_banner_str '-')

# preprocess a linker script
$(UBS_PATH_OBJ)/%.pp.ld : %.ld
	$(call ubs-create-target-folder)
	$(ubs-build) "CPP" "$<"
	@$(UBS_CPP) -E $(UBS_CPP_FLAGS) $< -o - | \
	    $(UBS_SED) -E 's/(0x[0-9a-fA-F]+)ULL/\1/g' > $@

# autogen a linker script if none given
$(UBS_LINKER_SCRIPT_AUTOGEN):
	$(call ubs-create-target-folder)
	$(ubs-build) "LDAUTO" "$@"
	@$(UBS_LD) --verbose | $(UBS_AWK) '/========/{f=!f; next} f' > $@

# cpp tidy rules
$(UBS_PATH_OBJ)/%.cpp.tidy: %.cpp
	$(ubs-build) "TIDY" "$@"
	@$(UBS_CLANG_TIDY) $(UBS_CLANG_TIDY_FLAGS) -extra-arg=-I$(UBS)/src $< 2>/dev/null
	$(call ubs-create-target-folder)
	@$(UBS_TOUCH) $@
	
tidy:: $(UBS_TIDY_ALL)
	$(ubs-info) "C++ static-analysis [clang-tidy]"
	$(ubs-done)

# static analysis stages are dynamically enabled depending on projects files types
sas:: $(if $(UBS_SRCS_ADB),$(UBS_SAS_STAGES))
coverity:: $(if $(UBS_SRCS_CXX),$(UBS_COVERITY_STAGES))


-include $(UBS_DEPS)

.PHONY: $(UBS_TARGETS_ALL) build-banner lib_gen
.SILENT: $(UBS_TARGETS_ALL) build-banner lib_gen

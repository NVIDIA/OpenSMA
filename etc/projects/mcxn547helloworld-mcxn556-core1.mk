# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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



CC_FLAGS   += $(MCU_COMPILE_FLAGS)
CXX_FLAGS  += $(MCU_COMPILE_FLAGS)
ADA_FLAGS  += $(MCU_COMPILE_FLAGS)
LINK_FLAGS += $(MCU_COMPILE_FLAGS)

GD_nv_$(BOARD)			:= 1

# SDK has static inline functions
CC_FLAGS   += -Wno-unused-function  \
              -Wno-implicit-function-declaration
LINK_FLAGS += -Wl,--print-memory-usage
LINK_FLAGS += -Wl,--sort-common=descending,--sort-section=alignment
LINK_FLAGS += -static
LINK_FLAGS += -flto -Wl,-z,noexecstack,-z,now
BIND_FLAGS := --RTS=$(PATH_ADARTS) -minimal

# -gnatp 	suppress runtime checks
# -gnato11 	overflow checks all strict
# -gnatec   
ADA_FLAGS += -gnatp -gnato11 \
			-gnatec=etc/platforms/global.adc \
			--RTS=$(PATH_ADARTS)
ADA_FLAGS += -fno-unwind-tables -fno-exceptions

# -- global defines ----------------------------------------------------------------------------
GD_SDK_OS_FREE_RTOS := 1
GD_SDK_DEBUGCONSOLE := 1

# -- flags -------------------------------------------------------------------------------------
UBS_LIBS 	+= -Wl,--start-group -lc -lgcc -Wl,--end-group
LINK_FLAGS 	+= -nolibc -nostdlib -no-pie -nostartfiles -static -Wl,--gc-sections 
LINK_FLAGS 	+= $(addprefix -Xlinker ,-Map=$(UBS_PATH_OUT)/output.map)

# -- includes ----------------------------------------------------------------------------------
SYSTEM_INCLUDES +=  \
	$(PATH_SDK)/CMSIS/Core/Include \
	$(PATH_SDK)/components/lists \
	$(PATH_SDK)/components/uart \
	$(PATH_SDK_DEVICE) \
	$(PATH_SDK_DEVICE)/drivers \
	$(PATH_SDK_DEVICE)/utilities/str \
	$(PATH_SDK_DEVICE)/utilities/debug_console_lite \
	$(PATH_BOARD)

# -- sources -----------------------------------------------------------------------------------
SOURCES += \
	$(PATH_PROJECT_SYS)/start.S \
	$(PATH_PROJECT_SYS)/gpio_interrupt_handler.cpp \
	$(PATH_SDK_DEVICE)/drivers/fsl_clock.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_common_arm.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_common.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_gpio.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_lpflexcomm.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_lpuart.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_reset.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_spc.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_lpi2c.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_edma.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_edma_soc.c \
	$(PATH_SDK_DEVICE)/utilities/debug_console_lite/fsl_assert.c \
	$(PATH_SDK_DEVICE)/utilities/debug_console_lite/fsl_debug_console.c \
	$(PATH_SDK_DEVICE)/utilities/fsl_sbrk.c \
	$(PATH_SDK_DEVICE)/utilities/fsl_syscall_stub.c \
	$(PATH_SDK_DEVICE)/utilities/str/fsl_str.c \
	$(PATH_SDK_DEVICE)/utilities/fsl_memcpy.S \
	$(PATH_SDK_COMPONENTS)/lists/fsl_component_generic_list.c \
	$(PATH_SDK_COMPONENTS)/osa/fsl_os_abstraction_free_rtos.c \
	$(PATH_SDK_COMPONENTS)/uart/fsl_adapter_lpuart.c \
	$(PATH_PROJECT)/main.cpp \
	$(PATH_PROJECT)/system_init_core1.c \
	$(PATH_SDK_DEVICE)/gcc/startup_MCXN556S_cm33_core1.S

# -- features ----------------------------------------------------------------------------------
FEATURE_reg_table := 1
SOURCES			  += $(SOURCES_reg_table) $(PATH_PROJECT)/regconfig.cpp
FEATURE_reg_ds	  := 1
SOURCES			  += $(SOURCES_reg_ds)

# -- NXP SDK -------------------------------------------------------------------------------
SYSTEM_INCLUDES +=  \
	$(PATH_SDK)/middleware/usb/device \
	$(PATH_SDK)/middleware/usb/device/class \
	$(PATH_SDK)/middleware/usb/phy/ \
	$(PATH_SDK)/middleware/usb/device/ \
	$(PATH_SDK)/middleware/usb/device/class/ \
	$(PATH_SDK)/middleware/usb/include/ \
	$(PATH_SDK)/middleware/multicore/mcmgr/src/ \
	$(PATH_SDK)/middleware/multicore/mcmgr/inc/ \
	$(PATH_SDK)/middleware/multicore/mcmgr/inc/platform/mcxnx4x/ \
	$(PATH_SDK_COMPONENTS)/osa/ \
	$(PATH_SYS)/sys/usb/ \
	$(PATH_SDK_DEVICE)/drivers/cm33_core1/ \
	$(PATH_SDK_DEVICE)/drivers/romapi/ \
	$(PATH_SDK_DEVICE)/drivers/romapi/flash \
	$(PATH_SDK_DEVICE)/drivers/romapi/nboot \
	$(PATH_SDK_DEVICE)/drivers/romapi/mem_interface \
	$(PATH_SDK_DEVICE)/drivers/romapi/runbootloader \
	$(PATH_SDK_DEVICE)/periph

SOURCES += \
	$(PATH_SDK)/middleware/usb/device/usb_device_dci.c \
	$(PATH_SDK)/middleware/usb/device/usb_device_ehci.c \
	$(PATH_SDK)/middleware/usb/phy/usb_phy.c \
	$(PATH_SDK)/middleware/usb/device/class/usb_device_class.c \
	$(PATH_SDK)/middleware/usb/device/class/usb_device_hid.c \
	$(PATH_SDK)/middleware/usb/device/usb_device_ch9.c \
	$(PATH_SDK)/middleware/multicore/mcmgr/src/mcmgr.c \
	$(PATH_SDK)/middleware/multicore/mcmgr/src/mcmgr_internal_core_api_mcxnx4x.c \
	$(PATH_SYS)/sys/usb/usb_device_mctp.c \
	$(PATH_SYS)/sys/usb/usb_device_spi.c \
	$(PATH_SDK_DEVICE)/drivers/romapi/flash/src/fsl_flash.c \
	$(PATH_SDK_DEVICE)/drivers/romapi/mem_interface/src/fsl_mem_interface.c \
	$(PATH_SDK_DEVICE)/drivers/romapi/runbootloader/src/fsl_runbootloader.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_ctimer.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_cdog.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_lpadc.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_vref.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_wwdt.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_lpspi.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_lpspi_edma.c \
	$(PATH_SDK_DEVICE)/drivers/romapi/nboot/src/fsl_nboot.c


include etc/projects/module-crypto.mk

FREERTOS_SRC_ARCH	= \
	$(PATH_FREERTOS)/portable/GCC/ARM_CM33_NTZ/non_secure/mpu_wrappers_v2_asm.c \
	$(PATH_FREERTOS)/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c \
	$(PATH_FREERTOS)/portable/GCC/ARM_CM33_NTZ/non_secure/port.c

include etc/projects/module-spdm.mk
include etc/projects/module-pldm-fd.mk
include etc/projects/plats-pldm-fd.mk
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



CC_FLAGS   += $(MCU_COMPILE_FLAGS)
CXX_FLAGS  += $(MCU_COMPILE_FLAGS)
ADA_FLAGS  += $(MCU_COMPILE_FLAGS)
LINK_FLAGS += $(MCU_COMPILE_FLAGS)

GD_nv_$(BOARD)			:= 1

CC_FLAGS   += -Wno-unused-function # SDK has static inline functions
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
GD_SDK_OS_FREE_RTOS 	:= 1 
GD_SDK_DEBUGCONSOLE 	:= 1

# -- flags -------------------------------------------------------------------------------------
UBS_LIBS 	+= -Wl,--start-group -lc -lgcc -Wl,--end-group
LINK_FLAGS 	+= -nolibc -nostdlib -no-pie -nostartfiles -static -Wl,--gc-sections 
LINK_FLAGS 	+= $(addprefix -Xlinker ,-Map=$(UBS_PATH_OUT)/output.map)

# -- includes ----------------------------------------------------------------------------------
SYSTEM_INCLUDES +=  \
	$(PATH_SDK)/CMSIS/Core/Include \
	$(PATH_SDK)/components/lists \
	$(PATH_SDK)/components/uart \
	$(PATH_SDK)/devices/MCXN236 \
	$(PATH_SDK)/devices/MCXN236/drivers \
	$(PATH_SDK)/devices/MCXN236/utilities/str \
	$(PATH_SDK)/devices/MCXN236/utilities/debug_console_lite \
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
	$(PATH_SDK_DEVICE)/drivers/fsl_i3c.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_i3c_edma.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_edma.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_edma_soc.c \
	$(PATH_SDK_DEVICE)/system_MCXN236.c \
	$(PATH_SDK_DEVICE)/utilities/debug_console_lite/fsl_assert.c \
	$(PATH_SDK_DEVICE)/utilities/debug_console_lite/fsl_debug_console.c \
	$(PATH_SDK_DEVICE)/utilities/fsl_sbrk.c \
	$(PATH_SDK_DEVICE)/utilities/fsl_syscall_stub.c \
	$(PATH_SDK_DEVICE)/utilities/str/fsl_str.c \
	$(PATH_SDK_DEVICE)/gcc/startup_MCXN236.S \
	$(PATH_SDK_DEVICE)/utilities/fsl_memcpy.S \
	$(PATH_SDK_COMPONENTS)/lists/fsl_component_generic_list.c \
	$(PATH_SDK_COMPONENTS)/osa/fsl_os_abstraction_free_rtos.c \
	$(PATH_SDK_COMPONENTS)/uart/fsl_adapter_lpuart.c \
	$(PATH_BOARD)/board/peripherals.c \
	$(PATH_BOARD)/board/pin_mux.c \
	$(PATH_BOARD)/board/clock_config.c \
	$(PATH_PROJECT)/main.cpp

# -- NXP SDK -------------------------------------------------------------------------------
SYSTEM_INCLUDES +=  \
	$(PATH_SDK)/middleware/usb/device \
	$(PATH_SDK)/middleware/usb/device/class \
	$(PATH_SDK)/middleware/usb/phy/ \
	$(PATH_SDK)/middleware/usb/device/ \
	$(PATH_SDK)/middleware/usb/device/class/ \
	$(PATH_SDK)/middleware/usb/include/ \
	$(PATH_SDK_COMPONENTS)/osa/ \
	$(UBS_PATH_SRC)/sys/mcxn236/sys/usb/ \
	$(PATH_SDK_DEVICE)/drivers/romapi/ \
	$(PATH_SDK_DEVICE)/drivers/romapi/flash \
	$(PATH_SDK_DEVICE)/drivers/romapi/nboot \
	$(PATH_SDK_DEVICE)/drivers/romapi/mem_interface \
	$(PATH_SDK_DEVICE)/drivers/romapi/runbootloader \
	$(PATH_SDK_DEVICE)/periph1

SOURCES += \
	$(PATH_SDK)/middleware/usb/device/usb_device_dci.c \
	$(PATH_SDK)/middleware/usb/device/usb_device_ehci.c \
	$(PATH_SDK)/middleware/usb/phy/usb_phy.c \
	$(PATH_SDK)/middleware/usb/device/class/usb_device_class.c \
	$(PATH_SDK)/middleware/usb/device/class/usb_device_hid.c \
	$(PATH_SDK)/middleware/usb/device/usb_device_ch9.c \
	$(UBS_PATH_SRC)/sys/mcxn236/sys/usb/usb_device_mctp.c \
	$(PATH_SDK_DEVICE)/drivers/romapi/flash/src/fsl_flash.c \
	$(PATH_SDK_DEVICE)/drivers/romapi/mem_interface/src/fsl_mem_interface.c \
	$(PATH_SDK_DEVICE)/drivers/romapi/runbootloader/src/fsl_runbootloader.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_ctimer.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_cdog.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_lpadc.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_vref.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_wwdt.c \
	$(PATH_SDK_DEVICE)/drivers/fsl_lpspi.c \
	$(PATH_SDK_DEVICE)/drivers/romapi/nboot/src/fsl_nboot.c 


# -- CRYPTO flags ------------------------------------------------------------------------------
CC_FLAGS += -DMBEDTLS_MCUX_ELS_API -DMBEDTLS_MCUX_USE_ELS -DMBEDTLS_CONFIG_FILE=\"els_pkc_mbedtls_config.h\" \
			-DMBEDTLS_MCUX_ELS_PKC_API -DMBEDTLS_MCUX_USE_PKC -DMBEDTLS_MEMORY_BUFFER_ALLOC_C -DMBEDTLS_PLATFORM_MEMORY \
			-DMBEDTLS_PLATFORM_EXIT_ALT -DMBEDTLS_PLATFORM_NO_STD_FUNCTIONS

# -- NXP CRYPTO SDK ----------------------------------------------------------------------------
SYSTEM_INCLUDES +=  \
	$(PATH_SDK)/middleware/mbedtls/port/ \
	$(PATH_SDK)/middleware/mbedtls/port/els \
	$(PATH_SDK)/middleware/mbedtls/port/pkc \
	$(PATH_SDK)/middleware/mbedtls/include/ \
	$(PATH_SDK)/middleware/mbedtls/include/mbedtls/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/platforms/mcxn/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/platforms/mcxn/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClHash/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClCore/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClSession/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxCsslCPreProcessor/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxCsslFlowProtection/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxCsslSecureCounter/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClRandom/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEls/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClHmac/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClKey/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClMemory/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClHashModes/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClExample/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClRandomModes/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClPkc/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClRsa/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClMath/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxCsslMemory/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxCsslParamIntegrity/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClPrng/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClAes/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClTrng/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/compiler/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/includes/platform/mcxn/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClMac/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClBuffer/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxCsslDataIntegrity/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClCipherModes/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClPadding/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClCipher/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClAeadModes/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClAead/inc/ \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClMacModes/inc/ \
	$(PATH_SDK_COMPONENTS)/osa/config/ # path for fsl_os_abstraction_config.h

SOURCES += \
	$(PATH_SDK)/middleware/mbedtls/library/bignum.c \
	$(PATH_SDK)/middleware/mbedtls/library/ecp.c \
	$(PATH_SDK)/middleware/mbedtls/library/ecp_curves.c \
	$(PATH_SDK)/middleware/mbedtls/library/memory_buffer_alloc.c \
	$(PATH_SDK)/middleware/mbedtls/library/platform.c \
	$(PATH_SDK)/middleware/mbedtls/library/sha1.c \
	$(PATH_SDK)/middleware/mbedtls/library/platform_util.c \
	$(PATH_SDK)/middleware/mbedtls/library/ecdsa.c \
	$(PATH_SDK)/middleware/mbedtls/port/els/aes_alt.c \
	$(PATH_SDK)/middleware/mbedtls/port/els/ctr_drbg_alt.c \
	$(PATH_SDK)/middleware/mbedtls/port/els/els_mbedtls.c \
	$(PATH_SDK)/middleware/mbedtls/port/els/entropy_poll_alt.c \
	$(PATH_SDK)/middleware/mbedtls/port/els/sha256_alt.c \
	$(PATH_SDK)/middleware/mbedtls/port/els/sha512_alt.c \
	$(PATH_SDK)/middleware/mbedtls/port/pkc/ecc_alt.c \
	$(PATH_SDK)/middleware/mbedtls/port/pkc/ecdsa_alt.c \
	$(PATH_SDK)/middleware/mbedtls/port/pkc/els_pkc_mbedtls.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEls/src/mcuxClEls_Cipher.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEls/src/mcuxClEls_Common.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEls/src/mcuxClEls_Hash.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEls/src/mcuxClEls_KeyManagement.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEls/src/mcuxClEls_Rng.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Internal_Types.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Internal_InterleaveTwoScalars.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_ECDSA_Internal_BlindedSecretKeyGen.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_WeierECC_Internal_BlindedSecretKeyGen.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_WeierECC_Internal_BlindedSecretKeyGen_FUP.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Internal_ConvertPoint_FUP.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Internal_PointArithmetic.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Internal_PointArithmetic_FUP.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Internal_PointCheck.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Internal_PointCheck_FUP.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Internal_PointMult.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Internal_SecurePointMult_CoZMontLadder.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Internal_SecurePointMult_CoZMontLadder_FUP.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_PointMult.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_PointMult_FUP.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Sign.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Sign_FUP.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Verify.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Verify_FUP.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClMath/src/mcuxClMath_QDash.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClMath/src/mcuxClMath_NDash.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClMath/src/mcuxClMath_ModInv.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClMath/src/mcuxClMath_Utils.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClMath/src/mcuxClMath_QDash_FUP.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClMath/src/mcuxClMath_NDash_FUP.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClMath/src/mcuxClMath_ModInv_FUP.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClMath/src/mcuxClMath_ReduceModEven.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClMemory/src/mcuxClMemory.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClPkc/src/mcuxClPkc_Calculate.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClPkc/src/mcuxClPkc_ImportExport.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClPkc/src/mcuxClPkc_Initialize.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClPkc/src/mcuxClPkc_UPTRT.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClPrng/src/mcuxClPrng_ELS.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Internal_InterleaveTwoScalars.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Internal_Interleave_FUP.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Internal_ConvertPoint_FUP.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Internal_PointArithmetic.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Internal_PointArithmetic_FUP.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Internal_PointCheck.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Internal_PointCheck_FUP.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Internal_SecurePointMult_CoZMontLadder.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Internal_SecurePointMult_CoZMontLadder_FUP.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Internal_SetupEnvironment.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Internal_PointMult.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_PointMult.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_PointMult_FUP.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Sign.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Sign_FUP.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Verify.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClEcc/src/mcuxClEcc_Weier_Verify_FUP.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClHash/src/mcuxClHash_api_multipart_common.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClHash/src/mcuxClHash_api_multipart_compute.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClHash/src/mcuxClHash_api_oneshot_compute.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClHash/src/mcuxClHash_Internal.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClHashModes/src/mcuxClHashModes_Core_els_sha2.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClHashModes/src/mcuxClHashModes_Internal_els_sha2.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClRandom/src/mcuxClRandom_DRBG.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClRandom/src/mcuxClRandom_PRNG.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClRandomModes/src/mcuxClRandomModes_CtrDrbg.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClRandomModes/src/mcuxClRandomModes_CtrDrbg_Els.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClRandomModes/src/mcuxClRandomModes_CtrDrbg_PrDisabled.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClRandomModes/src/mcuxClRandomModes_ElsMode.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClRandomModes/src/mcuxClRandomModes_NormalMode.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClRandomModes/src/mcuxClRandomModes_PrDisabled.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClRandomModes/src/mcuxClRandomModes_TestMode.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClSession/src/mcuxClSession.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxClTrng/src/mcuxClTrng_ELS.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxCsslMemory/src/mcuxCsslMemory_Compare.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxCsslMemory/src/mcuxCsslMemory_Copy.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/comps/mcuxCsslParamIntegrity/src/mcuxCsslParamIntegrity32.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/platforms/mcxn/mcux_els.c \
	$(PATH_SDK_COMPONENTS)/els_pkc/src/platforms/mcxn/mcux_pkc.c \

FREERTOS_SRC_ARCH	= \
	$(PATH_FREERTOS)/portable/GCC/ARM_CM33_NTZ/non_secure/mpu_wrappers_v2_asm.c \
	$(PATH_FREERTOS)/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c \
	$(PATH_FREERTOS)/portable/GCC/ARM_CM33_NTZ/non_secure/port.c

include etc/projects/module-spdm.mk
include etc/projects/module-pldm-fd.mk
include etc/projects/plats-pldm-fd.mk
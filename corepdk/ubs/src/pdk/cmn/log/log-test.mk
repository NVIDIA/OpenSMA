#-----------------------------------------------------------------------------------------------
#-                 Copyright (c) 2024, NVIDIA Corporation.  All Rights Reserved.              --
#-----------------------------------------------------------------------------------------------
#-   NVIDIA Corporation and its licensors retain all intellectual property and proprietary    --
#-   rights in and to this software and related documentation.  Any use, reproduction,        --
#-   disclosure or distribution of this software and related documentation without an         --
#-   express license agreement from NVIDIA Corporation is strictly prohibited.                --
#-----------------------------------------------------------------------------------------------

# NOTE: GD_PDK_CMN_LOG_TEST_STRING_1 and GD_PDK_CMN_LOG_TEST_STRING_2 are defined in
# src/ubs/ubs-cmn.mk so they're available during compilation. We reference them here for tests.

ifndef PDK_CMN_LOG_ALL
UBS_UNITTEST_EXTRA += pdk-cmn-log

PDK_CMN_LOG_TESTS := pdk-cmn-log-zero pdk-cmn-log-magic
PDK_CMN_LOG_ALL   := pdk-cmn-log $(PDK_CMN_LOG_TESTS)


pdk-cmn-log-zero: 
	@$(ubs_banner_str 'CMN::LOG-ZERO-OVERHEAD-UNITTEST')
	$(ubs-info) "testing zero-overhead logging...\n"

	# check: before and after sizes
	sz0=$$(stat -c%s $(UBS_TARGET)); \
	here0=$$(objdump --source --demangle $(UBS_TARGET) | grep -c "log::here()"); \
	$(ubs-info) "unittest: rebuilding with logging disabled...\n"; \
	$(MAKE) --makefile $(UBS_PATH_ROOT)/ubs.mk distclean > /dev/null;  \
	$(MAKE) --makefile $(UBS_PATH_ROOT)/ubs.mk unittest \
		PDK_CMN_LOG_ALL=1 GD_PDK_CMN_LOG_DBG_LEVEL_PERSISTENT=0 GD_PDK_CMN_LOG_DBG_LEVEL_CONSOLE=0 \
		> /dev/null; \
	sz1=$$(stat -c%s $(UBS_TARGET) 2>/dev/null); \
	here1=$$(objdump --source --demangle $(UBS_TARGET) | grep -c "log::here()"); \
	$(ubs-info) "unittest: before and after binary sizes (bytes) $$sz0 > $$sz1\n"; \
	if [ "$$sz0" -le "$$sz1" ]; then \
		$(ubs-error) "binary did not reduce in size\n"; \
	fi; \
	$(ubs-info) "unittest: before and after log::info counts $$here0 > $$here1 \n"; \
	if [ "$$here0" -le "$$here1" ]; then \
		$(ubs-error) "log::here code detected\n"; \
	fi


	# check: String elimination - test strings should NOT be in binary
	sc=$$(strings $(UBS_TARGET) | grep -c $(GD_PDK_CMN_LOG_TEST_STRING_1)); \
	if [ "$$sc" -ne 0 ]; then \
		$(ubs-error) "test string $(STRING_COUNT_1) PDK_CMN_LOG_TEST_STRING_1 found in binary\n"; \
		exit 1; \
	fi
	sc=$$(strings $(UBS_TARGET) | grep -c $(GD_PDK_CMN_LOG_TEST_STRING_2)); \
	if [ "$$sc" -ne 0 ]; then \
		$(ubs-error) "test string PDK_CMN_LOG_TEST_STRING_2 found in binary\n"; \
		exit 1; \
	fi
	$(ubs-info) "unittest: test string elimination verified\n"

pdk-cmn-log-magic: 
	@$(ubs_banner_str 'CMN::LOG-MAGICNUMER-UNITTEST')
	$(ubs-info) "unittest: rebuilding with magic numbers...\n"; \
	$(MAKE) --makefile $(UBS_PATH_ROOT)/ubs.mk distclean > /dev/null;  \
	$(MAKE) --makefile $(UBS_PATH_ROOT)/ubs.mk unittest PDK_CMN_LOG_ALL=1  \
	    GD_PDK_CMN_LOG_SL_CONSOLE=1 \
	    GD_PDK_CMN_LOG_SL_PERSISTENT=1 > /dev/null;

pdk-cmn-log: $(PDK_CMN_LOG_TESTS)


.PHONY: $(PDK_CMN_LOG_ALL)
.SILENT: $(PDK_CMN_LOG_ALL)
.NOTPARALLEL: $(PDK_CMN_LOG_ALL)
endif

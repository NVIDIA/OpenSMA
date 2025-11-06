MCU_COVERAGE_PROJECT ?= $(PROJECT)
# if GD_NV_COVERAGE is defined, but not testrunner or *-coverage, return error
ifneq ($(origin GD_NV_COVERAGE),undefined)
  ifeq ($(filter testrunner %-coverage,$(MCU_COVERAGE_PROJECT)),)
    $(error GD_NV_COVERAGE should only be defined for testrunner or *-coverage projects, current project: $(MCU_COVERAGE_PROJECT))
  endif
endif
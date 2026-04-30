/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "nv/bootloader.h"

#include <FreeRTOSConfig.h>
#include <portmacrocommon.h>

#include "nv/flash/task.h"
#include "nv/ipc/supervisor.h"
#include "nv/ipc/task.h"
#include "nv/logger/log.h"
#include "nv/nv.h"
#include "nv/pldm/task.h"
#include "nv/spdm/task.h"
#include "nv/watchdog/boot.h"
#include "sys/common/utils.h"
#include "nv/watchdog/runtime.h"
#include "sys/common/stack_protect.h"
#include "nv/usb/task.h"
#include "nv/gpio/driver.h"
#include "nv/logger/log_fault.h"
using namespace nv;
using namespace nv::bootloader;

// Jump information bit flags
static constexpr uint32_t JUMP_INFO_SLOT_0_FLAG = 0x1;  // Bit 0: booted from slot 0
static constexpr uint32_t JUMP_INFO_SLOT_1_FLAG = 0x2;  // Bit 1: booted from slot 1

// Global variable to cache current boot index (0xfafafafa = uninitialized)
NV_SHARED_DATA uint32_t g_current_boot_index = 0xfafafafa;

void Driver::run_on_index(ImageIndex image_index)
{
    if (!nv::watchdog::Boot::able_to_switch()) {
        logger::info(logger::Event::BootReachMaxSwitchTime, {});
        nv::logger::FaultLogger::fault(nv::logger::Fault::OverSwtichSlotLimit);
        return;
    }
    nv::watchdog::Boot::increase_total_switch_time();

    user_app_boot_invoke_option_t boot_option = {
        .option{.B{.boot_image_index = static_cast<uint32_t>(image_index), .tag = Tag}}};

    *std::bit_cast<uint32_t*>(BootReasonAddr) = (CMC_SRS_WARM_MASK | CMC_SRS_SW_MASK);

    // write the jump information to the ram
    constexpr bool use_boot_rom = true;
    const auto     jump_to      = image_index;
    *std::bit_cast<volatile uint32_t*>(
        FmcJumpInformationAddr) = ((jump_to == ImageIndex::Image0) ? 0x1 : 0x2)
                                | ((use_boot_rom == true) ? 0x4 : 0x0);

    bootloader_user_entry(&boot_option);

    // coverity[no_escape] suppress warning for Infinite loop with no exit
    while (true) {};
}

Driver::ImageIndex Driver::current_boot_index()
{
    uint32_t BootImageValue = 0;
    if (g_current_boot_index == 0xfafafafa) {
        const uint32_t JumpInformationValue = get_fmc_jump_information();
        bool           jump_info_invalid    = JumpInformationValue == 0 ? true : false;

        if (jump_info_invalid || get_boot_src() != Driver::BootSourceFMC) {
            const uint32_t BootLogRegValue = *(
                std::bit_cast<volatile uint32_t*>(ElsAsBootLog0Addr));
            BootImageValue = SYSCON_ELS_AS_BOOT_LOG0_BOOT_IMAGE(BootLogRegValue);
        }
        else {
            if (JumpInformationValue & JUMP_INFO_SLOT_0_FLAG) {
                BootImageValue = 0;
            }
            else if (JumpInformationValue & JUMP_INFO_SLOT_1_FLAG) {
                BootImageValue = 1;
            }
            else {
                BootImageValue = 2;
            }
        }
        g_current_boot_index = BootImageValue;
    }
    else {
        BootImageValue = g_current_boot_index;
    }
    switch (BootImageValue) {
        case 0:
        case 1 : return static_cast<ImageIndex>(BootImageValue);
        default: return ImageIndex::Invalid;
    }
    return ImageIndex::Invalid;
}

void Driver::hw_init()
{
    /* initial all interrupt priority to lowest */
    for (uint32_t i = OR_IRQn; i <= CTI0_IRQn; i++) {
        NVIC_SetPriority(static_cast<IRQn_Type>(i), configLIBRARY_LOWEST_INTERRUPT_PRIORITY);
    }
    NVIC_SetPriority(SEC_VIO_IRQn, 0);
}

void __attribute__((no_stack_protector)) Driver::set_stack_cookie()
{
    // Set stack cookie to random value
    if constexpr (SSP_ENABLED) {
        uintptr_t                stack_cookie = 0;
        mbedtls_ctr_drbg_context ctr_drbg;
        mbedtls_ctr_drbg_init(&ctr_drbg);
        const int Ret = mbedtls_ctr_drbg_random(&ctr_drbg,
                                                // NOLINTNEXTLINE(*-reinterpret-cast)
                                                reinterpret_cast<uint8_t*>(&stack_cookie),
                                                sizeof(stack_cookie));
        mbedtls_ctr_drbg_free(&ctr_drbg);
        if (Ret != 0) {
            logger::info(logger::Event::BootStackGuardInitFail, {});
        }
        // NOLINTNEXTLINE(cert-dcl37-c,cert-dcl51-cpp)
        __stack_chk_guard = stack_cookie;
        // Clean up secrets
        stack_cookie = 0;
    }
}

void Driver::disable_all_flexcomm_irq()
{
    /* disable all flexcomm interrupt */
    for (uint32_t i = LP_FLEXCOMM0_IRQn; i <= LP_FLEXCOMM9_IRQn; i++) {
        NVIC_DisableIRQ(static_cast<IRQn_Type>(i));
    }
}

void Driver::check_boot_reason()
{
    const uint32_t Srs = get_boot_reason();

    if (Srs & CMC_SRS_WAKEUP_MASK) {
        nv::info("Reset by Wake-up\n");
    }
    if (Srs & CMC_SRS_POR_MASK) {
        nv::info("Reset by Power-on\n");
    }
    if (Srs & CMC_SRS_VD_MASK) {
        nv::info("Reset by Voltage Detect\n");
    }
    if (Srs & CMC_SRS_WARM_MASK) {
        nv::info("Reset by Warm Reset\n");
    }
    if (Srs & CMC_SRS_FATAL_MASK) {
        nv::info("Reset by Fatal Error\n");
    }
    if (Srs & CMC_SRS_PIN_MASK) {
        nv::info("Reset by Pin\n");
    }
    if (Srs & CMC_SRS_DAP_MASK) {
        nv::info("Reset by Debug Access Port\n");
    }
    if (Srs & CMC_SRS_RSTACK_MASK) {
        nv::info("Reset by Reset Timeout\n");
    }
    if (Srs & CMC_SRS_LPACK_MASK) {
        nv::info("Reset by Low Power Acknowledge Timeout\n");
    }
    if (Srs & CMC_SRS_SCG_MASK) {
        nv::info("Reset by System Clock Generator\n");
    }
    if (Srs & CMC_SRS_WWDT0_MASK) {
        nv::info("Reset by Windowed Watchdog 0\n");
    }
    if (Srs & CMC_SRS_SW_MASK) {
        nv::info("Reset by Software\n");
    }
    if (Srs & CMC_SRS_LOCKUP_MASK) {
        nv::info("Reset by Lockup Reset\n");
    }
    if (Srs & CMC_SRS_CPU1_MASK) {
        nv::info("Reset by CPU1\n");
    }
    if (Srs & CMC_SRS_VBAT_MASK) {
        nv::info("Reset by VBAT\n");
    }
    if (Srs & CMC_SRS_WWDT1_MASK) {
        nv::info("Reset by Windowed Watchdog 1\n");
    }
    if (Srs & CMC_SRS_CDOG0_MASK) {
        nv::info("Reset by Code Watchdog 0\n");
    }
    if (Srs & CMC_SRS_CDOG1_MASK) {
        nv::info("Reset by Code Watchdog 1\n");
    }
    if (Srs & CMC_SRS_JTAG_MASK) {
        nv::info("Reset by JTAG\n");
    }
    if (Srs & CMC_SRS_SECVIO_MASK) {
        nv::info("Reset by Security Violation\n");
    }
    if (Srs & CMC_SRS_TAMPER_MASK) {
        nv::info("Reset by Tamper\n");
    }
    logger::info(logger::Event::BootReason, logger::data_from_u32(Srs));

    const uint32_t original_boot_reason = get_original_boot_reason();
    logger::info(logger::Event::BootReasonOriginal,
                 logger::data_from_u32(original_boot_reason));
}

bool Driver::check_cust_mk_sk()
{
    std::array<uint8_t, CustMkSkSize> empty_key = {0};
    if (memcmp(&empty_key, std::bit_cast<void*>(CustMkSkAddr), sizeof(empty_key)) == 0) {
        logger::info(logger::Event::BootCustMkSkCheck, {1});
        return false;
    }
    else {
        logger::info(logger::Event::BootCustMkSkCheck, {0});
        return true;
    }
}

void __attribute__((no_stack_protector)) Driver::boot_init()
{
    using namespace std::chrono_literals;
    auto boot_timer = ipc::Timer::make(
        ipc::TimerId::Bootloader,
        std::chrono::milliseconds(nv::ipc::CheckTaskBootStatusMs),
        bootloader::Driver::on_timer,
        false);
    boot_timer.start();

    hw_init();
    enable_fault();
    check_boot_reason();
    check_cust_mk_sk();  // TBD: fault when CUST_MK_SK is empty

    auto boot_slot = current_boot_index();

    nv::info("boot from slot %d\n", boot_slot);
    logger::info(logger::Event::BootSlot, {static_cast<uint8_t>(boot_slot)});
    logger::info(logger::Event::BootDieId, logger::data_from_u32(SYSCON->DIEID));

    flash::Data update_state{};
    flash::Data update_slot{};
    flash::Data bootable_slot0{};
    flash::Data bootable_slot1{};

    // flash::Data w0{};
    // flashtask.set_pds_from_kernel(flash::Key::PdsBootableSlot0, w0);
    // flashtask.set_pds_from_kernel(flash::Key::PdsUpdateState, w0);
    // flashtask.set_pds_from_kernel(flash::Key::PdsBootableSlot1, w0);
    // flashtask.set_pds_from_kernel(flash::Key::PdsUpdateSlot, w0);

    flash::Flash::get_data_from_kernel(flash::Key::PdsUpdateState, update_state);
    flash::Flash::get_data_from_kernel(flash::Key::PdsUpdateSlot, update_slot);
    flash::Flash::get_data_from_kernel(flash::Key::PdsBootableSlot0, bootable_slot0);
    flash::Flash::get_data_from_kernel(flash::Key::PdsBootableSlot1, bootable_slot1);

    nv::info("PdsUpdateState :%d\n", update_state);
    nv::info("PdsUpdateSlot  :%d\n", update_slot);
    nv::info("bootable_slot0 :%d\n", bootable_slot0);
    nv::info("bootable_slot1 :%d\n", bootable_slot1);

    logger::info(logger::Event::BootLoaderReadPDS,
                 logger::data_from_two_u32(static_cast<uint32_t>(flash::Key::PdsUpdateState),
                                           update_state));
    logger::info(logger::Event::BootLoaderReadPDS,
                 logger::data_from_two_u32(static_cast<uint32_t>(flash::Key::PdsUpdateSlot),
                                           update_slot));
    logger::info(logger::Event::BootLoaderReadPDS,
                 logger::data_from_two_u32(static_cast<uint32_t>(flash::Key::PdsBootableSlot0),
                                           bootable_slot0));
    logger::info(logger::Event::BootLoaderReadPDS,
                 logger::data_from_two_u32(static_cast<uint32_t>(flash::Key::PdsBootableSlot1),
                                           bootable_slot1));

    ImageIndex inactive = ImageIndex::Begin;

    if (boot_slot == ImageIndex::Image0) {
        inactive = ImageIndex::Image1;
    }
    else if (boot_slot == ImageIndex::Image1) {
        inactive = ImageIndex::Image0;
    }
    else {
        return;
    }

    auto           boot_source        = get_boot_src_from_kernel();
    uint32_t       auth_result        = 0;
    bool           inactive_auth_pass = false;
    const uint32_t inactive_slot0     = 1;
    const uint32_t inactive_slot1     = 2;

    if (Driver::BootSourceFMC == boot_source) {
        auth_result = get_auth_result();
        // Bit 0: 0: slot 0 authentication failed, 1: slot 0 authentication passed
        // Bit 1: 0: slot 1 authentication failed, 1: slot 1 authentication passed
        inactive_auth_pass = (auth_result
                              & (inactive == ImageIndex::Image0 ? inactive_slot0
                                                                : inactive_slot1));
    }
    else {
        // No FMC case, let bootloader jump as the original flow
        inactive_auth_pass = true;
    }

    if ((update_state == static_cast<flash::Data>(State::Complete)
         && inactive == static_cast<ImageIndex>(update_slot))
        || (update_state == static_cast<flash::Data>(State::Stage)
            && boot_slot == static_cast<ImageIndex>(update_slot))) {
        if (!nv::watchdog::Boot::check_boot_failed(inactive) && inactive_auth_pass) {
            nv::watchdog::Boot::mark_switch(inactive);
            // We are not going to boot this slot, clear try time and keep its status
            nv::watchdog::Boot::clear_try_times(boot_slot);

            if constexpr (nv::ipc::EnableRuntimeWdt) {
                const bool
                    inactive_will_switch_by_wdt = nv::watchdog::Boot::get_runtime_flag(inactive)
                                                   >= nv::watchdog::Runtime::RetryThreshold
                                               && nv::watchdog::Boot::get_target_boot_slot()
                                                      == boot_slot;
                if (!inactive_will_switch_by_wdt) {
                    nv::info("[Bootloader] Switch on FW update to %d\n", inactive);
                    run_on_index(inactive);
                }
            }
            else {
                nv::info("[Bootloader] Switch on FW update to %d\n", inactive);
                run_on_index(inactive);
            }
        }
    }

    // Runtime watchdog
    if constexpr (nv::ipc::EnableRuntimeWdt) {
        // The main slot will retry for one time
        // If main slot boot failed twice, boot inactive
        const uint8_t    runtime_flag     = nv::watchdog::Boot::get_runtime_flag(boot_slot);
        const ImageIndex target_boot_slot = nv::watchdog::Boot::get_target_boot_slot();
        if (runtime_flag >= nv::watchdog::Runtime::RetryThreshold
            && (target_boot_slot != boot_slot)) {
            // 1. Mark as bootable in PDS
            // 2. Not mark as boot failed by boot WDT, otherwise boot WDT logic will try to
            // switch
            // 3. Inactive is not stage
            const bool InactiveAuthResult = is_inactive_auth_pass();
            bool       inactive_bootable  = boot_slot == ImageIndex::Image0 ? bootable_slot1
                                                                            : bootable_slot0;
            inactive_bootable             = (inactive_bootable
                                 && (!nv::watchdog::Boot::check_boot_failed(inactive))
                                 && InactiveAuthResult);

            // Stage consider as corrupt before activate
            const bool inactive_is_stage = (update_state
                                                == static_cast<flash::Data>(State::Stage)
                                            && inactive
                                                   == static_cast<ImageIndex>(update_slot));

            if (inactive_bootable && !inactive_is_stage) {
                nv::watchdog::Boot::set_target_boot_slot(inactive);
                run_on_index(inactive);
            }
        }
    }

    flash::Flash::set_data_from_kernel(flash::Key::NpdsBootReasonOriginal,
                                       get_original_boot_reason());

    const ApplicationFaultRecord app_fault_record = get_application_fault_record();

    flash::Flash::set_data_from_kernel(flash::Key::NpdsApplicationFaultMagic,
                                       app_fault_record.fault_magic);
    flash::Flash::set_data_from_kernel(flash::Key::NpdsCfsr, app_fault_record.cfsr);
    flash::Flash::set_data_from_kernel(flash::Key::NpdsHfsr, app_fault_record.hfsr);
    flash::Flash::set_data_from_kernel(flash::Key::NpdsWdtEventBits,
                                       app_fault_record.event_bits);

    // TBD: time for reset it
    write_application_fault_record(0x0, 0x0, 0x0, 0x0);

    // Check for USB port reset marker
    uint32_t usb_reset_marker = 0;
    nv::mainbox::read_mailbox_u32(nv::mainbox::MainBoxMemoryType::UsbPortReset,
                                  usb_reset_marker);
    if (usb_reset_marker == nv::usb::UsbPortResetMagicNumber) {
        // Clear mailbox
        nv::mainbox::write_mailbox_u32(nv::mainbox::MainBoxMemoryType::UsbPortReset, 0x0);
        logger::info(logger::Event::UsbPortReset, {});
    }

#if 0
    uint32_t auth_result     = get_auth_result();
    uint32_t fmc_status_code = get_fmc_status();
    nv::info("auth_result %d\n", auth_result);
    nv::info("fmc_status_code 0x%x\n", fmc_status_code);
#endif
}

bool Driver::set_image_bootable(ImageIndex index, bool bootable)
{
    flash::Key key = flash::Key::PdsStart;

    if (index == ImageIndex::Image0) {
        key = flash::Key::PdsBootableSlot0;
    }
    else if (index == ImageIndex::Image1) {
        key = flash::Key::PdsBootableSlot1;
    }
    else {
        return false;
    }

    auto status = flash::Flash::set_data(key, bootable);
    if (status != flash::Status::Ok) {
        return false;
    }

    if (bootable) {
        nv::watchdog::Boot::clear_boot_failed(index);
    }

    nv::info("set_slot_bootable slot %d bootable %d\n", index, bootable);
    return true;
}

bool Driver::set_inactive_bootable(bool bootable)
{
    auto image_index = Driver::current_boot_index();

    if (image_index == ImageIndex::Image0) {
        image_index = ImageIndex::Image1;
    }
    else if (image_index == ImageIndex::Image1) {
        image_index = ImageIndex::Image0;
    }
    else {
        return false;
    }

    return set_image_bootable(image_index, bootable);
}

void Driver::self_reset()
{
    NVIC_SystemReset();
}

void Driver::set_task_booted(nv::ipc::BootedEventBits boot_bit)
{
    auto event = ipc::Event::make(ipc::EventId::TaskBootStatus);
    event.set(boot_bit);
}

void Driver::on_timer([[maybe_unused]] ipc::Timer& id)
{
    using namespace std::chrono_literals;
    auto boot_source = get_boot_src_from_kernel();
    auto event       = ipc::Event::make(ipc::EventId::TaskBootStatus);
    auto value       = event.bits().value();
    if (value == nv::common::to_underlying(nv::ipc::BootedEventBits::BootStatusMask)) {
        auto boot_slot = current_boot_index();
        nv::watchdog::Boot::disable();
        nv::watchdog::Boot::reset_total_switch_time();
        nv::watchdog::Boot::clear_boot_failed(boot_slot);
        if constexpr (nv::ipc::EnableRuntimeWdt) {
            if (nv::watchdog::Runtime::is_reset(ImageIndex::Image0)) {
                const uint8_t runtime_flag = nv::watchdog::Boot::get_runtime_flag(
                    ImageIndex::Image0);
                nv::logger::error_no_wait(
                    nv::logger::Event::BootSwitchByWdt,
                    {static_cast<uint8_t>(ImageIndex::Image0), runtime_flag},
                    logger::OutputDirection::Both,
                    0s);
            }

            if (nv::watchdog::Runtime::is_reset(ImageIndex::Image1)) {
                const uint8_t runtime_flag = nv::watchdog::Boot::get_runtime_flag(
                    ImageIndex::Image1);
                nv::logger::error_no_wait(
                    nv::logger::Event::BootSwitchByWdt,
                    {static_cast<uint8_t>(ImageIndex::Image1), runtime_flag},
                    logger::OutputDirection::Both,
                    0s);
            }
        }

        nv::watchdog::Boot::update_booted(boot_slot);

        auto key = boot_slot == ImageIndex::Image0 ? flash::Key::PdsBootableSlot0
                                                   : flash::Key::PdsBootableSlot1;
        flash::Flash::set_data_from_kernel(key, true);

        nv::info("All task boot slot %d\n", boot_slot);

        const auto WdtPersistentValue = nv::watchdog::Boot::read_sticky();

        auto BootFailedOccured = nv::watchdog::Boot::is_boot_failed_occurred();
        if (BootFailedOccured) {
            nv::logger::error_no_wait(nv::logger::Event::BootWDTPersistent,
                                      nv::logger::data_from_u32(WdtPersistentValue),
                                      logger::OutputDirection::Both,
                                      0s);
        }
        else {
            nv::logger::info(nv::logger::Event::BootWDTPersistent,
                             nv::logger::data_from_u32(WdtPersistentValue));
        }

        if constexpr (nv::common::build::IsStreamBoot == false) {
            flash::Data update_state{};
            flash::Data update_slot{};
            flash::Flash::get_data_from_kernel(flash::Key::PdsUpdateState, update_state);
            flash::Flash::get_data_from_kernel(flash::Key::PdsUpdateSlot, update_slot);

            /*
            Current Slot is bootable. Do BG if
                1. Current slot is update slot
                2. Another slot cannot boot
            */

            if (update_state == static_cast<flash::Data>(State::Complete)) {
                if (update_slot == static_cast<flash::Data>(boot_slot)) {
                    /* trigger BG */
                    pldm::Task::pldm_bg_start();
                }
            }
            key = boot_slot == ImageIndex::Image0 ? flash::Key::PdsBootableSlot1
                                                  : flash::Key::PdsBootableSlot0;
            flash::Data inactive_bootable{};
            flash::Flash::get_data_from_kernel(key, inactive_bootable);
            if (!inactive_bootable) {
                /* trigger BG */
                pldm::Task::pldm_bg_start();
            }

            auto inactive = boot_slot == ImageIndex::Image0 ? ImageIndex::Image1
                                                            : ImageIndex::Image0;
            clear_inactive_alias(inactive);
        }
        const flash::Data BootReason    = get_boot_reason();
        const flash::Data FmcStatus     = get_fmc_status();
        const uint64_t    FmcVersion    = get_fmc_ordinal_number();
        const flash::Data FmcAuthResult = get_auth_result();
        const uint32_t    TRNGConfig    = get_TRNG_config();
        if (Driver::BootSourceFMC == boot_source) {
            nv::logger::info(nv::logger::Event::BootFmcAuthResult,
                             nv::logger::data_from_two_u32(FmcAuthResult, 0));
            nv::logger::info(nv::logger::Event::BootFmcStatusCode,
                             nv::logger::data_from_two_u32(FmcStatus, 0));
            nv::logger::info(nv::logger::Event::BootFmcOrdinalNumber,
                             *std::bit_cast<nv::logger::EventData*>(&FmcVersion));
            constexpr size_t  fmc_ordinal_size    = 5;
            constexpr uint8_t ten                 = 10;
            flash::Data       fmc_numeric_version = 0;
            for (size_t i = 0; i < fmc_ordinal_size; i++) {
                const auto b        = static_cast<uint8_t>(FmcVersion >> (i * 8u));
                fmc_numeric_version = fmc_numeric_version * ten
                                    + static_cast<flash::Data>(b - '0');
            }
            flash::Flash::set_data_from_kernel(flash::Key::NpdsFmcNumericVersion,
                                               fmc_numeric_version);
        }
        nv::logger::info(nv::logger::Event::TRNGConfigResult,
                         nv::logger::data_from_u32(TRNGConfig));
        flash::Flash::set_data_from_kernel(flash::Key::NpdsBootReason, BootReason);
        flash::Flash::set_data_from_kernel(flash::Key::NpdsFmcStatus, FmcStatus);
        flash::Flash::set_data_from_kernel(flash::Key::NpdsFmcAuthResult, FmcAuthResult);

        const FmcFaultRecord FmcFault{};
        get_fmc_fault(FmcFault.to_span());
        if (FmcFault.fault_magic == FmcFaultMagic) {
            nv::logger::info(
                nv::logger::Event::BootFmcFaultStatus1,
                nv::logger::data_from_two_u32(FmcFault.fault_hfsr, FmcFault.fault_cfsr));
            nv::logger::info(
                nv::logger::Event::BootFmcFaultStatus2,
                nv::logger::data_from_two_u32(FmcFault.fault_mmfar, FmcFault.fault_bfar));
        }

        if constexpr (nv::ipc::EnableRuntimeWdt) {
            nv::watchdog::Runtime::init(nv::ipc::RuntimeWatchdogResetMs, false);
            auto wdt_timer = ipc::Timer::make(
                ipc::TimerId::RuntimeWatchDog,
                std::chrono::milliseconds(nv::ipc::SupervisorCheckMs),
                nv::watchdog::Runtime::start_task_status_query,
                true);
            wdt_timer.start();
        }

        // REQ-GNL-REC-001 SMA_READY, default low, active high
        if constexpr (nv::ipc::EnableMcuReadyPin) {
            nv::gpio::Driver::write(nv::ipc::McuReadyPinPort, nv::ipc::McuReadyPinPin, 1U);
        }
    }
    else {
#if 0
        nv::info("Timeout slot %d\n", current_boot_index());
        const auto WdtPersistentValue = nv::watchdog::Boot::read_sticky();
        nv::logger::info(nv::logger::Event::BootWDTPersistent, nv::logger::data_from_u32(WdtPersistentValue));
#endif
    }
}

#if 0
void Driver::mark_updated(ImageIndex index)
{
    nv::watchdog::Boot::mark_updated(index);
}
#endif

uint8_t Driver::get_boot_src()
{
    uint8_t boot_src{};
    nv::flash::Flash::read_cmpa({std::bit_cast<uint8_t*>(&boot_src), sizeof(boot_src)}, 0);
    boot_src = (boot_src & BootSourceMask);
    return boot_src;
}

void Driver::set_fmc_wp()
{
    uint32_t val                              = *std::bit_cast<uint32_t*>(Ifr0WpRegister);
    val                                       = ((val & FmcWpSelMask) | FmcWpValue);
    *std::bit_cast<uint32_t*>(Ifr0WpRegister) = val;
}

uint32_t Driver::get_boot_reason()
{
    if (get_boot_src_from_kernel() == BootSourceInternalFlash) {
        return CMC0->SRS;
    }
    return *std::bit_cast<uint32_t*>(BootReasonAddr);
}

uint32_t Driver::get_auth_result()
{
    const auto FmcAuthResult = *std::bit_cast<uint32_t*>(AuthResultAddr);
    return FmcAuthResult;
}

uint32_t Driver::get_fmc_status()
{
    const auto FmcStatusCode = *std::bit_cast<uint32_t*>(FmcStatusCodeAddr);
    return FmcStatusCode;
}

uint64_t Driver::get_fmc_ordinal_number()
{
    const auto FmcOrdinalNumber = *std::bit_cast<uint64_t*>(FmcOrdinalNumberAddr);
    return FmcOrdinalNumber;
}

uint32_t Driver::get_TRNG_config()
{
    const auto TRNGConfigResult = *std::bit_cast<uint32_t*>(TRNGConfigAddr);
    return TRNGConfigResult;
}

void Driver::get_fmc_fault(const std::span<uint8_t>& buffer)
{
    memcpy(buffer.data(), std::bit_cast<void*>(FmcFaultStatusAddr), sizeof(FmcFaultRecord));
}

uint32_t Driver::get_fmc_jump_information()
{
    const auto FmcJumpInformation = *std::bit_cast<uint32_t*>(FmcJumpInformationAddr);
    return FmcJumpInformation;
}

void Driver::clear_inactive_alias(ImageIndex inactive_index)
{
    auto address = inactive_index == ImageIndex::Image0 ? AliasImage0Addr : AliasImage1Addr;
    memset(std::bit_cast<void*>(address), 0x0, AliasLength);
    address = inactive_index == ImageIndex::Image0 ? SignatureAk0Addr : SignatureAk1Addr;
    memset(std::bit_cast<void*>(address), 0x0, SignatureSize);
}

uint8_t Driver::get_boot_src_from_kernel()
{
    uint8_t boot_src{};
    memcpy(&boot_src, std::bit_cast<void*>(BootSourceAddr), sizeof(boot_src));
    boot_src = (boot_src & BootSourceMask);
    return boot_src;
}

Driver::ImageIndex Driver::get_previous_boot_slot()
{
    auto prev_boot = nv::watchdog::Boot::get_previous_booted_slot();
    if (prev_boot > 0) {
        return static_cast<ImageIndex>(prev_boot - 1);
    }
    return ImageIndex::Invalid;
}

bool Driver::is_inactive_auth_pass()
{
    auto boot_source          = get_boot_src_from_kernel();
    bool inactive_auth_result = false;
    // WAR: Not all platform are boot with FMC in this phase
    if (boot_source == Driver::BootSourceFMC
        || boot_source == Driver::BootSourceInternalFlash) {
        auto inactive          = current_boot_index() == ImageIndex::Image0 ? ImageIndex::Image1
                                                                            : ImageIndex::Image0;
        auto image_auth_result = get_auth_result();
        inactive_auth_result   = inactive == ImageIndex::Image0
                                   ? ((image_auth_result & 0x1u) > 0)
                                   : ((image_auth_result & 0x2u) > 0);
    }
    return inactive_auth_result;
}

void sys::bootloader::Driver::mark_inactive_auth_pass(bool pass)
{
    if (pass) {
        *std::bit_cast<uint32_t*>(AuthResultAddr) = (nv::common::bit(0) | nv::common::bit(1));
    }
    else {
        *std::bit_cast<uint32_t*>(AuthResultAddr) = 0;
    }
}

void Driver::enable_fault()
{
    constexpr uint32_t UsageFaultEn  = nv::common::bit(18U);
    constexpr uint32_t BusFaultEn    = nv::common::bit(17U);
    SCB->SHCSR                      |= (UsageFaultEn | BusFaultEn);
}

void Driver::write_original_boot_reason(uint32_t reason)
{
    *std::bit_cast<uint32_t*>(OriginalBootReasonAddr) = reason;
}

uint32_t Driver::get_original_boot_reason()
{
    return *std::bit_cast<uint32_t*>(OriginalBootReasonAddr);
}

void Driver::write_application_fault_record(uint32_t fault_magic,
                                            uint32_t cfsr,
                                            uint32_t hfsr,
                                            uint32_t event_bits)
{
    constexpr uint32_t CfsrOffset                                           = 4;
    constexpr uint32_t HfsrOffset                                           = 8;
    constexpr uint32_t EventBitsOffset                                      = 12;
    *std::bit_cast<uint32_t*>(ApplicationFaultRecordAddr)                   = fault_magic;
    *std::bit_cast<uint32_t*>(ApplicationFaultRecordAddr + CfsrOffset)      = cfsr;
    *std::bit_cast<uint32_t*>(ApplicationFaultRecordAddr + HfsrOffset)      = hfsr;
    *std::bit_cast<uint32_t*>(ApplicationFaultRecordAddr + EventBitsOffset) = event_bits;
}

Driver::ApplicationFaultRecord Driver::get_application_fault_record()
{
    return *std::bit_cast<Driver::ApplicationFaultRecord*>(ApplicationFaultRecordAddr);
}

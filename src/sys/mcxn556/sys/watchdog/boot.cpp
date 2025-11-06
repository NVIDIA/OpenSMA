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
#include "nv/watchdog/boot.h"

#include "fsl_clock.h"

#include "nv/bootloader.h"
#include "nv/nv.h"
#include "sys/common/utils.h"
#include "nv/logger/log.h"

using namespace nv::watchdog;

#define SYS_WATCHDOG_CDOG_INST CDOG0

void Boot::init(uint32_t reset_ms)
{
    cdog_config_t conf;
    CDOG_GetDefaultConfig(&conf);
    conf.timeout    = kCDOG_FaultCtrl_EnableReset;
    conf.miscompare = kCDOG_FaultCtrl_NoAction;
    conf.sequence   = kCDOG_FaultCtrl_NoAction;
    conf.state      = kCDOG_FaultCtrl_NoAction;
    conf.address    = kCDOG_FaultCtrl_NoAction;
    conf.irq_pause  = kCDOG_IrqPauseCtrl_Run;
    conf.debug_halt = kCDOG_DebugHaltCtrl_Pause;
    conf.lock       = kCDOG_LockCtrl_Unlock;

    CDOG_Init(SYS_WATCHDOG_CDOG_INST, &conf);
    const uint32_t ClockFreq = CLOCK_GetFreq(kCLOCK_BusClk);

    // Maximum time could set is around 28 seconds
    const uint32_t TargetReload    = nv::common::mul(((ClockFreq) / (OneSecMs)), reset_ms);
    SYS_WATCHDOG_CDOG_INST->RELOAD = TargetReload;
}

void Boot::disable()
{
    CDOG_Stop(SYS_WATCHDOG_CDOG_INST, CdogSecureConst);
}

void Boot::enable()
{
    CDOG_Start(SYS_WATCHDOG_CDOG_INST, SYS_WATCHDOG_CDOG_INST->RELOAD, CdogSecureConst);
}

void Boot::feed()
{
    CDOG_Check(SYS_WATCHDOG_CDOG_INST, CdogSecureConst);
}

bool Boot::is_active()
{
    const uint32_t Status = SYS_WATCHDOG_CDOG_INST->STATUS;
    if (((Status & StatusCurstMask) >> StatusCurstShift)
        == nv::common::to_underlying(State::Active)) {
        return true;
    }
    return false;
}

uint32_t Boot::read_ticks()
{
    return SYS_WATCHDOG_CDOG_INST->INSTRUCTION_TIMER;
}

void Boot::write_sticky(uint32_t value)
{
    CDOG_WritePersistent(SYS_WATCHDOG_CDOG_INST, value);
}

uint32_t Boot::read_sticky()
{
    return CDOG_ReadPersistent(SYS_WATCHDOG_CDOG_INST);
}

void Boot::start_watchdog(uint32_t reset_ms)
{
    using namespace std::chrono_literals;
    init(reset_ms);
    enable();

    const uint32_t BootReason = nv::bootloader::Driver::get_boot_reason();

    if ((BootReason & CMC_SRS_SW_MASK) == 0) {
        nv::bootloader::Driver::write_original_boot_reason(BootReason);
    }

    bool enable_wdt = true;
    auto boot_slot  = nv::bootloader::Driver::current_boot_index();
    auto inactive   = boot_slot == nv::bootloader::Driver::ImageIndex::Image0
                        ? nv::bootloader::Driver::ImageIndex::Image1
                        : nv::bootloader::Driver::ImageIndex::Image0;

    auto boot_source          = nv::bootloader::Driver::get_boot_src_from_kernel();
    bool inactive_auth_result = false;

    if (boot_source == sys::bootloader::Driver::BootSourceFMC) {
        // nv::info("[WDT] Boot from FMC\n");
        auto image_auth_result = nv::bootloader::Driver::get_auth_result();
        inactive_auth_result   = inactive == nv::bootloader::Driver::ImageIndex::Image0
                                   ? ((image_auth_result & 0x1u) > 0)
                                   : ((image_auth_result & 0x2u) > 0);
    }
    else if (boot_source == sys::bootloader::Driver::BootSourceInternalFlash) {
        // WAR: FMC not included in all platforms in this phase
        inactive_auth_result = auth_inactive_from_kernel();
        if constexpr (nv::ipc::EnableRuntimeWdt) {
            sys::bootloader::Driver::mark_inactive_auth_pass(inactive_auth_result);
        }
    }

    // nv::info("[WDT] inactive_auth_result %d\n", inactive_auth_result);

    // Clear WDT status on pin reset
    if (nv::bootloader::Driver::get_boot_reason() & CMC_SRS_PIN_MASK) {
        write_sticky(0);
    }

    update_boot_status();
    try_boot(boot_slot);

    // If inactive slot cannot pass auth, disable WDT and let the current slot boot
    if (!inactive_auth_result) {
        enable_wdt = false;
    }

    if (enable_wdt && check_boot_failed(boot_slot)) {
        enable_wdt = false;
        if (!check_boot_failed(inactive)) {
            mark_switch(inactive);
            nv::bootloader::Driver::run_on_index(inactive);
        }
    }

    if (!enable_wdt) {
        nv::logger::info(
            nv::logger::Event::BootWdtDisabled, {}, logger::OutputDirection::Both, 0s);
        disable();
    }
}

void Boot::clear_boot_failed(nv::bootloader::Driver::ImageIndex index)
{
    auto record = std::bit_cast<BootFailedRecord>(read_sticky());
    if (index == nv::bootloader::Driver::ImageIndex::Image0) {
        record.slot0_failed.status       = sys::watchdog::Success;
        record.slot0_failed.try_times    = 0;
        record.slot0_failed.switch_times = 0;
    }
    else {
        record.slot1_failed.status       = sys::watchdog::Success;
        record.slot1_failed.try_times    = 0;
        record.slot1_failed.switch_times = 0;
    }
    write_sticky(std::bit_cast<uint32_t>(record));
}

void Boot::clear_try_times(nv::bootloader::Driver::ImageIndex index)
{
    auto record = std::bit_cast<BootFailedRecord>(read_sticky());
    if (index == nv::bootloader::Driver::ImageIndex::Image0) {
        record.slot0_failed.try_times = 0;
    }
    else {
        record.slot1_failed.try_times = 0;
    }
    write_sticky(std::bit_cast<uint32_t>(record));
}

bool Boot::check_boot_failed(nv::bootloader::Driver::ImageIndex index)
{
    auto record = std::bit_cast<BootFailedRecord>(read_sticky());
    if (index == nv::bootloader::Driver::ImageIndex::Image0) {
        if (record.slot0_failed.status == sys::watchdog::Failed) {
            return true;
        }
    }
    else {
        if (record.slot1_failed.status == sys::watchdog::Failed) {
            return true;
        }
    }
    return false;
}

bool Boot::check_boot_success(nv::bootloader::Driver::ImageIndex index)
{
    auto record = std::bit_cast<BootFailedRecord>(read_sticky());
    if (index == nv::bootloader::Driver::ImageIndex::Image0) {
        return record.slot0_failed.status == sys::watchdog::Success;
    }
    else {
        return record.slot1_failed.status == sys::watchdog::Success;
    }
}

void Boot::try_boot(nv::bootloader::Driver::ImageIndex index)
{
    auto record = std::bit_cast<BootFailedRecord>(read_sticky());
    if (index == nv::bootloader::Driver::ImageIndex::Image0) {
        if (record.slot0_failed.status != sys::watchdog::Failed) {
            record.slot0_failed.try_times = std::min<uint32_t>(
                sys::watchdog::Boot::MaxNum, record.slot0_failed.try_times + 1);
        }
    }
    else {
        if (record.slot1_failed.status != sys::watchdog::Failed) {
            record.slot1_failed.try_times = std::min<uint32_t>(
                sys::watchdog::Boot::MaxNum, record.slot1_failed.try_times + 1);
        }
    }

    if (index < nv::bootloader::Driver::ImageIndex::Invalid) {
        record.prev_try_boot_slot = static_cast<uint8_t>(index) + 1;
    }

    write_sticky(std::bit_cast<uint32_t>(record));
}

void Boot::mark_switch(nv::bootloader::Driver::ImageIndex index)
{
    auto record = std::bit_cast<BootFailedRecord>(read_sticky());
    if (index == nv::bootloader::Driver::ImageIndex::Image0) {
        if (record.slot0_failed.status != sys::watchdog::Failed) {
            record.slot0_failed.status       = sys::watchdog::TryBoot;
            record.prev_try_boot_slot        = static_cast<uint8_t>(index) + 1;
            record.slot0_failed.switch_times = std::min<uint32_t>(
                sys::watchdog::Boot::MaxNum, record.slot0_failed.switch_times + 1);
        }
    }
    else {
        if (record.slot1_failed.status != sys::watchdog::Failed) {
            record.slot1_failed.status = sys::watchdog::TryBoot;
            if (index < nv::bootloader::Driver::ImageIndex::Invalid) {
                record.prev_try_boot_slot = static_cast<uint8_t>(index) + 1;
            }
            record.slot1_failed.switch_times = std::min<uint32_t>(
                sys::watchdog::Boot::MaxNum, record.slot1_failed.switch_times + 1);
        }
    }

    write_sticky(std::bit_cast<uint32_t>(record));
}

void Boot::update_boot_status()
{
    auto       record           = std::bit_cast<BootFailedRecord>(read_sticky());
    const auto Srs              = nv::bootloader::Driver::get_boot_reason();
    const bool WdtResetOccurred = (Srs & CMC_SRS_CDOG0_MASK) > 0;
    if (WdtResetOccurred && record.prev_try_boot_slot > 0) {
        const uint8_t PrevSLot = record.prev_try_boot_slot - 1;
        if (PrevSLot == 0) {
            record.slot0_failed.status = sys::watchdog::Failed;
        }
        else {
            record.slot1_failed.status = sys::watchdog::Failed;
        }
    }
    if constexpr (nv::ipc::EnableRuntimeWdt) {
        const bool RuntimeWdtResetOccurred = (Srs & CMC_SRS_WWDT1_MASK) > 0;
        if (RuntimeWdtResetOccurred && record.prev_try_boot_slot > 0) {
            const uint8_t     PrevSLot = record.prev_try_boot_slot - 1;
            constexpr uint8_t FlagMask = 3;
            if (PrevSLot == 0) {
                if (record.slot0_runtime_flag < FlagMask) {
                    record.slot0_runtime_flag = record.slot0_runtime_flag + 1;
                }
            }
            else {
                if (record.slot1_runtime_flag < FlagMask) {
                    record.slot1_runtime_flag = record.slot1_runtime_flag + 1;
                }
            }
        }
    }

    write_sticky(std::bit_cast<uint32_t>(record));
}

uint32_t Boot::get_previous_booted_slot()
{
    auto record = std::bit_cast<BootFailedRecord>(read_sticky());
    return record.prev_booted_slot;
}

void Boot::update_booted(nv::bootloader::Driver::ImageIndex index)
{
    auto record = std::bit_cast<BootFailedRecord>(read_sticky());
    if (index < nv::bootloader::Driver::ImageIndex::Invalid) {
        record.prev_booted_slot = static_cast<uint8_t>(index) + 1;
    }
    write_sticky(std::bit_cast<uint32_t>(record));
}

void Boot::update_wwdt_flag(nv::bootloader::Driver::ImageIndex index, bool is_reset)
{
    auto record = std::bit_cast<BootFailedRecord>(read_sticky());
    if (index < nv::bootloader::Driver::ImageIndex::Invalid) {
        if (index == nv::bootloader::Driver::ImageIndex::Image0) {
            record.slot0_runtime_flag = (is_reset > 0 ? 1 : 0);
        }
        else {
            record.slot1_runtime_flag = (is_reset > 0 ? 1 : 0);
        }
    }

    write_sticky(std::bit_cast<uint32_t>(record));
}

bool Boot::get_wwdt_flag(nv::bootloader::Driver::ImageIndex index)
{
    auto record = std::bit_cast<BootFailedRecord>(read_sticky());
    if (index == nv::bootloader::Driver::ImageIndex::Image0) {
        return record.slot0_runtime_flag;
    }
    else {
        return record.slot1_runtime_flag;
    }
}

#if 0
void Boot::clear_update()
{
    auto record           = std::bit_cast<BootFailedRecord>(read_sticky());
    record.update_occured = 0;
    record.update_slot    = 0;
    write_sticky(std::bit_cast<uint32_t>(record));
}

void Boot::mark_updated(nv::bootloader::Driver::ImageIndex index)
{
    auto record = std::bit_cast<BootFailedRecord>(read_sticky());
    if (index == nv::bootloader::Driver::ImageIndex::Image0) {
        record.slot0_failed.try_times    = 0;
        record.slot0_failed.switch_times = 0;
        record.slot0_failed.status       = sys::watchdog::Init;
    }
    else {
        record.slot1_failed.try_times    = 0;
        record.slot1_failed.switch_times = 0;
        record.slot1_failed.status       = sys::watchdog::Init;
    }
    record.update_occured = 1;
    record.update_slot    = static_cast<uint8_t>(index);

    write_sticky(std::bit_cast<uint32_t>(record));
}

bool Boot::is_update(nv::bootloader::Driver::ImageIndex index)
{
    auto record = std::bit_cast<BootFailedRecord>(read_sticky());
    return (record.update_occured && (record.update_slot == static_cast<uint8_t>(index)));
}
#endif

// WAR: Not all platform are boot with FMC in this phase
// Authenticate inactive instead of using FMC authenticate result
#include "fsl_flash.h"
#include "fsl_flash_ffr.h"
#include "fsl_nboot.h"

// NOLINTNEXTLINE(*-avoid-non-const-global-variables*)
nboot_context_t nboot_context{};

bool sys::watchdog::Boot::auth_inactive_from_kernel()
{
#ifdef CPU_MCXN547VDF
    cmpa_cfg_info_t nboot_cmpa_read{};
    // NOLINTNEXTLINE(misc-const-correctness)
    cfpa_cfg_info_t nboot_cfpa_read{};
    flash_config_t  nboot_flash_config{};

    constexpr static uint32_t NumRokthUsage = 4;
    using RokthUsageMaskShift               = std::array<uint32_t, 2>;
    constexpr static std::array<RokthUsageMaskShift, NumRokthUsage> RokthUsageMaskShifts{
        RokthUsageMaskShift{  0x7, 0x0},
        RokthUsageMaskShift{ 0x38,   3},
        RokthUsageMaskShift{0x1c0,   6},
        RokthUsageMaskShift{0xE00,   9}
    };
    constexpr static uint32_t InactiveImageAddress   = sys::flash::config::Slot1FwAddress;
    constexpr static uint32_t SocLiefcycleCfgMask    = 0xFF;
    constexpr static uint32_t SocLiefcycleUpperMask  = 0xFFFF0000;
    constexpr static uint32_t SocLiefcycleLowerMask  = 0x0000FFFF;
    constexpr static uint32_t SocLiefcycleUpperShift = 16;
    constexpr static uint32_t AuthStatusMask         = 0xFFFFFFFF;

    auto sts = FLASH_Init(&nboot_flash_config);
    if (sts != kStatus_Success) {
        return false;
    }

    sts = FFR_Init(&nboot_flash_config);
    if (sts != kStatus_Success) {
        return false;
    }

    sts = FFR_GetCustomerInfieldData(&nboot_flash_config,
                                     std::bit_cast<uint8_t*>(&nboot_cfpa_read),
                                     0,
                                     sizeof(nboot_cfpa_read));
    if (sts != kStatus_Success) {
        return false;
    }

    sts = FFR_GetCustomerData(&nboot_flash_config,
                              std::bit_cast<uint8_t*>(&nboot_cmpa_read),
                              0,
                              sizeof(nboot_cmpa_read));
    if (sts != kStatus_Success) {
        return false;
    }

    nboot_bool_t                 is_signature_verified = kNBOOT_FALSE;
    nboot_img_auth_ecdsa_parms_t parms{};

    // Initialize the NBOOT context
    const nboot_status_protected_t Status = NBOOT_ContextInit(&nboot_context);
    if (Status != kStatus_NBOOT_Success) {
        nv::info("Failed to initialize NBOOT\n");
        return false;
    }

    // Address of the image to authenticate
    const uint32_t ImageStartAddress = InactiveImageAddress;

    parms.soc_trustedFirmwareVersion          = nboot_cfpa_read.nsFwVersion;
    parms.soc_RoTNVM.soc_rootKeyRevocation[0] = kNBOOT_RootKey_Enabled;
    parms.soc_RoTNVM.soc_rootKeyRevocation[1] = kNBOOT_RootKey_Enabled;
    parms.soc_RoTNVM.soc_rootKeyRevocation[2] = kNBOOT_RootKey_Enabled;
    parms.soc_RoTNVM.soc_rootKeyRevocation[3] = kNBOOT_RootKey_Enabled;
    parms.soc_RoTNVM.soc_imageKeyRevocation   = nboot_cfpa_read.imageKeyRevoke;

    memset(static_cast<void*>(parms.soc_RoTNVM.soc_rkh), 0, sizeof(parms.soc_RoTNVM.soc_rkh));

    memcpy(static_cast<void*>(parms.soc_RoTNVM.soc_rkh),
           static_cast<void*>(nboot_cmpa_read.rotkh),
           sizeof(nboot_cmpa_read.rotkh));

    parms.soc_RoTNVM.soc_rootKeyUsage[0] = (nboot_cmpa_read.rokthUsage
                                            & RokthUsageMaskShifts[0][0])
                                        >> RokthUsageMaskShifts[0][1];
    parms.soc_RoTNVM.soc_rootKeyUsage[1] = (nboot_cmpa_read.rokthUsage
                                            & RokthUsageMaskShifts[1][0])
                                        >> RokthUsageMaskShifts[1][1];
    parms.soc_RoTNVM.soc_rootKeyUsage[2] = (nboot_cmpa_read.rokthUsage
                                            & RokthUsageMaskShifts[2][0])
                                        >> RokthUsageMaskShifts[2][1];
    parms.soc_RoTNVM.soc_rootKeyUsage[3] = (nboot_cmpa_read.rokthUsage
                                            & RokthUsageMaskShifts[3][0])
                                        >> RokthUsageMaskShifts[3][1];

    parms.soc_RoTNVM.soc_numberOfRootKeys     = 4;
    parms.soc_RoTNVM.soc_rootKeyTypeAndLength = kNBOOT_RootKey_Ecdsa_P384;
    parms.soc_RoTNVM.soc_lifecycle            = SYSCON->ELS_AS_CFG0
                                   & SocLiefcycleCfgMask;  // nboot_lc_oemLocked;
    parms.soc_RoTNVM.soc_lifecycle = (((~parms.soc_RoTNVM.soc_lifecycle)
                                       << SocLiefcycleUpperShift)
                                      & SocLiefcycleUpperMask)
                                   | (parms.soc_RoTNVM.soc_lifecycle & SocLiefcycleLowerMask);

    // nv::info("[Crypto] AUTH IMAGE 0x%x\n", ImageStartAddress);

    const nboot_status_protected_t AuthStatus = NBOOT_ImgAuthenticateEcdsa(
        &nboot_context,
        std::bit_cast<uint8_t*>(ImageStartAddress),
        &is_signature_verified,
        &parms);
#if 0
    if ((uint32_t)AuthStatus == kStatus_NBOOT_Success && is_signature_verified == kNBOOT_TRUE) {
        nv::info("verified successfully\n");
    }
    else {
        nv::info("verified failed 0x%x\n", is_signature_verified);
    }
#endif

    FLASH_Deinit(&nboot_flash_config);

    // After NBOOT_ContextDeinit is called, then NBOOT_ImgAuthenticateEcdsa will lead to fault
    // even if we call NBOOT_ContextInit again. NBOOT_ContextDeinit(&nboot_context);
    const uint32_t AuthSts = (AuthStatus & AuthStatusMask);
    return (is_signature_verified == kNBOOT_TRUE) && AuthSts == kStatus_NBOOT_Success;
#else
    return true;
#endif
}

bool Boot::is_boot_failed_occurred()
{
    auto record = std::bit_cast<BootFailedRecord>(read_sticky());
    return record.slot0_failed.status == sys::watchdog::Failed
        || record.slot1_failed.status == sys::watchdog::Failed;
}

uint8_t Boot::get_runtime_flag(nv::bootloader::Driver::ImageIndex index)
{
    auto record = std::bit_cast<BootFailedRecord>(read_sticky());
    if (index == nv::bootloader::Driver::ImageIndex::Image0) {
        return record.slot0_runtime_flag;
    }
    else {
        return record.slot1_runtime_flag;
    }
}

nv::bootloader::Driver::ImageIndex Boot::get_target_boot_slot()
{
    auto record = std::bit_cast<BootFailedRecord>(read_sticky());

    const uint8_t target_boot_slot = record.target_boot_slot;
    if (target_boot_slot == 0) {
        return nv::bootloader::Driver::ImageIndex::Invalid;
    }
    return static_cast<nv::bootloader::Driver::ImageIndex>(target_boot_slot - 1);
}

void Boot::set_target_boot_slot(nv::bootloader::Driver::ImageIndex index)
{
    auto record = std::bit_cast<BootFailedRecord>(read_sticky());
    if (index < nv::bootloader::Driver::ImageIndex::End) {
        record.target_boot_slot = static_cast<uint8_t>(index) + 1;
    }
    write_sticky(std::bit_cast<uint32_t>(record));
}

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
#include "sys/flash/fccob.h"

#include <FreeRTOS.h>
#include <FreeRTOSConfig.h>
#include <portmacrocommon.h>
#include <task.h>

#include "sys/common/utils.h"
#include "sys/ctimer/ctimer.h"
#include "mpu_syscall_numbers.h"

using namespace sys::flash;

// Privileged functions
#if defined(__cplusplus)
extern "C" {
#endif

NV_PRIVILEGED_FUNCTION status_t Fccob_Driver_Phrase_Write_Priv(nv::flash::Address address,
                                                               const std::span<uint8_t>& buffer)
{
    return FccobDriver::phrase_write_impl(address, buffer);
}

NV_PRIVILEGED_FUNCTION status_t Fccob_Driver_Read_Priv(nv::flash::Address        address,
                                                       uint32_t                  length,
                                                       const std::span<uint8_t>& buffer)
{
    return FccobDriver::read_impl(address, length, buffer);
}

NV_PRIVILEGED_FUNCTION status_t Fccob_Driver_Erase_Priv(nv::flash::Address address)
{
    return FccobDriver::erase_impl(address);
}

#if defined(__cplusplus)
}
#endif

inline void FccobDriver::prepare_fccob_cmd(FccobCmdCode cmd)
{
    volatile FMUTEST_Type* reg   = FMU0TEST;
    auto                   fstat = std::bit_cast<volatile FstatReg*>(&reg->FSTAT);
    if (fstat->cmdabt) {
        fstat->cmdabt = 1;
    }
    if (fstat->pviol) {
        fstat->pviol = 1;
    }
    if (fstat->accerr) {
        fstat->accerr = 1;
    }
    reg->FCCOB0 = std::to_underlying(cmd);
    fstat->ccif = 1;
}

inline status_t FccobDriver::wait_command_complete()
{
    volatile FMUTEST_Type* reg   = FMU0TEST;
    auto                   fstat = std::bit_cast<volatile FstatReg*>(&reg->FSTAT);
    // PERDY
    uint32_t timeout_count = 0;
    while (fstat->ccif == 0) {
        timeout_count++;
        sys::ctimer::Driver::delay_for_us(UsecDelay);
        if (timeout_count >= WaitCounts) {
            return kStatus_Timeout;
        }
    }
    return kStatus_Success;
}

inline status_t FccobDriver::wait_write_enable()
{
    volatile FMUTEST_Type* reg           = FMU0TEST;
    auto                   fstat         = std::bit_cast<volatile FstatReg*>(&reg->FSTAT);
    uint32_t               timeout_count = 0;
    while (fstat->pewen == 0) {
        timeout_count++;
        sys::ctimer::Driver::delay_for_us(UsecDelay);
        if (timeout_count >= WaitCounts) {
            return kStatus_Timeout;
        }
    }
    return kStatus_Success;
}

inline status_t FccobDriver::wait_write_ready()
{
    volatile FMUTEST_Type* reg   = FMU0TEST;
    auto                   fstat = std::bit_cast<volatile FstatReg*>(&reg->FSTAT);
    // PERDY
    uint32_t timeout_count = 0;
    while (fstat->perdy == 0) {
        timeout_count++;
        sys::ctimer::Driver::delay_for_us(UsecDelay);
        if (timeout_count >= WaitCounts) {
            return kStatus_Timeout;
        }
    };
    // uint32_t val = *(uint32_t*)(fstat);
    // nv::info("wait_write_ready fstat value:0x%x\n", val);
    fstat->perdy = 1;
    return kStatus_Success;
}

status_t FccobDriver::write(uint32_t address, uint32_t length, const nv::flash::Buffer& buffer)
{
    if (length % PhraseSize || buffer.size() < length) {
        return kStatus_InvalidArgument;
    }

    if (!verify_erased(address, length)) {
        return kStatusMemoryCumulativeWrite;
    }

    // nv::info("FCCOB write at 0x%x length:0x%x\n", address, length);
    uint32_t remain_length   = length;
    uint32_t offset          = 0;
    uint32_t current_address = address;
    while (remain_length > 0) {
        auto status = phrase_write(current_address, buffer, offset);
        if (status != kStatus_Success) {
            return status;
        }
        remain_length   -= PhraseSize;
        offset          += PhraseSize;
        current_address += PhraseSize;
    }
    return kStatus_Success;
}

status_t FccobDriver::phrase_write(nv::flash::Address       address,
                                   const nv::flash::Buffer& buffer,
                                   uint32_t                 offset)
{
    if (buffer.size() == 0 or offset > buffer.size()) {
        return kStatus_InvalidArgument;
    }
    auto buffer_span = std::span<const uint8_t>(buffer).subspan(offset);
    return phrase_write_svc(address, buffer_span);
}

NV_SYS_CALL status_t FccobDriver::phrase_write_svc(nv::flash::Address              address,
                                                   const std::span<const uint8_t>& buffer)
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern Fccob_Driver_Phrase_Write_Priv                \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_Fccob_Driver_Phrase_Write              \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_Fccob_Driver_Phrase_Write            \n"
        " Privileged_Fccob_Driver_Phrase_Write:                 \n"
        "     pop {r0}                                          \n"
        "     b Fccob_Driver_Phrase_Write_Priv                  \n"
        " Unprivileged_Fccob_Driver_Phrase_Write:               \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_Fccob_Driver_Phrase_Write)
        : "memory");
#else
    return Fccob_Driver_Phrase_Write_Priv(address, buffer);
#endif
    // coverity[cert_msc52_cpp_violation] - Expect no return
}

NV_PRIVILEGED_FUNCTION status_t FccobDriver::phrase_write_impl(
    nv::flash::Address address, const std::span<const uint8_t>& buffer)
{
    if (!config::is_valid_address(address) or buffer.size() == 0
        or !config::is_valid_address(address, buffer.size() - 1)) {
        return kStatus_OutOfRange;
    }
    volatile FMUTEST_Type* reg   = FMU0TEST;
    auto                   fstat = std::bit_cast<volatile FstatReg*>(&reg->FSTAT);

    if (fstat->ccif == 0) {
        return kStatus_Busy;
    }
    // nv::info("Write at 0x%x start\n",address);
    prepare_fccob_cmd(FccobCmdCode::PhraseProgram);
    auto status = wait_write_enable();
    if (status != kStatus_Success) {
        return status;
    }

    for (uint32_t i = 0; i < 4; i++) {
        *std::bit_cast<volatile uint32_t*>(address + i * 4) = *std::bit_cast<uint32_t*>(
            static_cast<const uint8_t*>(buffer.data()) + i * 4);
    }

    status = wait_write_ready();
    if (status != kStatus_Success) {
        return status;
    }
    status = wait_command_complete();
    if (status != kStatus_Success) {
        return status;
    }
    // nv::info("Write at 0x%x end\n",address);

    L1CACHE_InvalidateCodeCache();
    return kStatus_Success;
}

bool FccobDriver::verify_erased(nv::flash::Address address, uint32_t length)
{
    if (length % PhraseSize || length > nv::flash::BufferSize) {
        return false;
    }
    nv::flash::Buffer buffer;
    read(address, length, buffer);
    for (uint32_t i = 0; i < length; i++) {
        if (buffer.at(i) != AllOneByte) {
            return false;
        }
    }
    return true;
// nv::info("FCCOB verify_erased at 0x%x length:0x%x\n", address, length);
#if 0
    //TBD: ReadAllOnePhrase operation not working when boot from image 1
    uint32_t remain_length   = length;
    uint32_t current_address = address;
    while (remain_length > 0) {
        auto erased = verify_phrase_erased(current_address);
        if (!erased) {
            return false;
        }
        remain_length   -= PhraseSize;
        current_address += PhraseSize;
    }
#endif

    return true;
}

bool FccobDriver::verify_phrase_erased(nv::flash::Address address)
{
    volatile FMUTEST_Type* reg   = FMU0TEST;
    auto                   fstat = std::bit_cast<volatile FstatReg*>(&reg->FSTAT);

    reg->FCCOB2 = address;
    prepare_fccob_cmd(FccobCmdCode::ReadAllOnePhrase);
    auto status = wait_command_complete();
    if (status != kStatus_Success) {
        return status;
    }
    // nv::info("verify erase at 0x%x fstat value:0x%x\n" , address, *(uint32_t*)(fstat));
    return fstat->fail == 0;
}

status_t FccobDriver::erase(nv::flash::Address address)
{
    return erase_svc(address);
}

NV_SYS_CALL status_t FccobDriver::erase_svc(nv::flash::Address address)
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern Fccob_Driver_Erase_Priv                       \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_Fccob_Driver_Erase                     \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_Fccob_Driver_Erase                   \n"
        " Privileged_Fccob_Driver_Erase:                        \n"
        "     pop {r0}                                          \n"
        "     b Fccob_Driver_Erase_Priv                         \n"
        " Unprivileged_Fccob_Driver_Erase:                      \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_Fccob_Driver_Erase)
        : "memory");
#else
    return Fccob_Driver_Erase_Priv(address);
#endif
    // coverity[cert_msc52_cpp_violation] - Expect no return
}

NV_PRIVILEGED_FUNCTION status_t FccobDriver::erase_impl(nv::flash::Address address)
{
    if (!config::is_valid_address(address)
        or address > config::MaxAddress - config::SectorSize) {
        return kStatus_OutOfRange;
    }
    volatile FMUTEST_Type* reg   = FMU0TEST;
    auto                   fstat = std::bit_cast<volatile FstatReg*>(&reg->FSTAT);
    // nv::info("FccobDriver::erase\n");
    if (fstat->ccif == 0) {
        return kStatus_Busy;
    }

    prepare_fccob_cmd(FccobCmdCode::SectorErase);
    auto status = wait_write_enable();
    if (status != kStatus_Success) {
        return status;
    }

    for (uint32_t i = 0; i < 4; i++) {
        *std::bit_cast<volatile uint32_t*>(address + i * 4) = AllOneWord;
    }

    status = wait_write_ready();
    if (status != kStatus_Success) {
        return status;
    }

    status = wait_command_complete();
    if (status != kStatus_Success) {
        return status;
    }

    L1CACHE_InvalidateCodeCache();
    return kStatus_Success;
}

status_t
FccobDriver::read(nv::flash::Address address, uint32_t length, const std::span<uint8_t>& buffer)
{
    return read_svc(address, length, buffer);
}

NV_SYS_CALL status_t FccobDriver::read_svc(nv::flash::Address        address,
                                           uint32_t                  length,
                                           const std::span<uint8_t>& buffer)
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern Fccob_Driver_Read_Priv                        \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_Fccob_Driver_Read                      \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_Fccob_Driver_Read                    \n"
        " Privileged_Fccob_Driver_Read:                         \n"
        "     pop {r0}                                          \n"
        "     b Fccob_Driver_Read_Priv                          \n"
        " Unprivileged_Fccob_Driver_Read:                       \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_Fccob_Driver_Read)
        : "memory");
#else
    return Fccob_Driver_Read_Priv(address, length, buffer);
#endif
    // coverity[cert_msc52_cpp_violation] - Expect no return
}

NV_PRIVILEGED_FUNCTION status_t FccobDriver::read_impl(nv::flash::Address        address,
                                                       uint32_t                  length,
                                                       const std::span<uint8_t>& buffer)
{
    if (length == 0 or buffer.size() < length) {
        return kStatus_InvalidArgument;
    }
    if (!config::is_valid_address(address) or !config::is_valid_address(address, length - 1)) {
        return kStatus_OutOfRange;
    }
    memcpy(buffer.data(), std::bit_cast<void*>(address), length);
    return kStatus_Success;
}
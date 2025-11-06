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
#include "nv/spdm/spdm_cert_chain.h"

#include <array>
#include <bitset>
#include <cstring>
#include <ranges>

#include "nv/flash/flash.h"
#include "nv/logger/common.h"
#include "nv/logger/log.h"
#include "nv/nv.h"
#include "nv/spdm/ak_generate_helper.h"
#include "nv/spdm/ik_generate_helper.h"
#include "nv/spdm/spdm_dummy_certificate.h"
#include "nv/spdm/task.h"
#include "mpu_syscall_numbers.h"
#include "spdm_cert_chain.h"

using namespace nv;
using namespace sys;

namespace nv::spdm::cert {
namespace {

#if defined(__cplusplus)
extern "C" {
#endif

NV_PRIVILEGED_FUNCTION uint16_t SPDM_Get_L4_Cert_Len_Priv()
{
    return nv::spdm::cert::get_l4_cert_len_impl();
}

NV_PRIVILEGED_FUNCTION uint16_t SPDM_Get_L5_Cert_Len_Priv()
{
    return nv::spdm::cert::get_l5_cert_len_impl();
}

NV_PRIVILEGED_FUNCTION uint16_t SPDM_Get_L4_Csr_Len_Priv()
{
    return nv::spdm::cert::get_l4_csr_len_impl();
}

NV_PRIVILEGED_FUNCTION uint16_t SPDM_Read_L4_Csr_Priv(std::span<uint8_t>& input_buffer)
{
    return nv::spdm::cert::read_l4_csr_impl(input_buffer);
}

NV_PRIVILEGED_FUNCTION bool
SPDM_Construct_L5_Cert_Priv(nv::spdm::certlib::CertArray& l5_cert_array)
{
    return nv::spdm::cert::construct_l5_cert_impl(l5_cert_array);
}

NV_PRIVILEGED_FUNCTION bool
SPDM_Read_DevIk_Request_Priv(nv::spdm::ik::DevIkRequest&   dev_ik_req,
                             const uint32_t                dda_ordinal_number,
                             nv::spdm::certlib::CertArray& l3_cert_array)
{
    return nv::spdm::cert::read_devik_request_impl(
        dev_ik_req, dda_ordinal_number, l3_cert_array);
}

NV_PRIVILEGED_FUNCTION bool
SPDM_Construct_L4_Cert_Priv(nv::spdm::ik::DevIkHelper&    dev_ik_helper,
                            nv::spdm::certlib::CertArray& l4_cert_array)
{
    return nv::spdm::cert::construct_l4_cert_impl(dev_ik_helper, l4_cert_array);
}

NV_PRIVILEGED_FUNCTION uint16_t SPDM_Read_L4_Cert_Priv(std::span<uint8_t>& input_buffer)
{
    return nv::spdm::cert::read_l4_cert_impl(input_buffer);
}

NV_PRIVILEGED_FUNCTION uint16_t SPDM_Read_L5_Cert_Priv(std::span<uint8_t>& input_buffer)
{
    return nv::spdm::cert::read_l5_cert_impl(input_buffer);
}

NV_PRIVILEGED_FUNCTION void
SPDM_Get_L5_Private_Key_Priv(nv::spdm::certlib::Ecdsa384PrivateKeyArray& l5_private_key)
{
    nv::spdm::cert::get_l5_private_key_impl(l5_private_key);
}

NV_PRIVILEGED_FUNCTION void SPDM_Erase_L4_Cert_Priv()
{
    nv::spdm::cert::erase_l4_cert_impl();
}

NV_PRIVILEGED_FUNCTION void SPDM_Erase_L5_Cert_Priv()
{
    nv::spdm::cert::erase_l5_cert_impl();
}

#if defined(__cplusplus)
}
#endif

static nv::flash::Address get_virtual_address(nv::flash::Address phy_cert_location)
{
    const nv::flash::Address VirCertLocation = nv::flash::Flash::get_flash_address(
        phy_cert_location, nv::bootloader::Driver::current_boot_index());
    return VirCertLocation;
}
// this function will read the length as the same as read_span size
static void read_from_flash(nv::flash::Address        read_vir_address,
                            const std::span<uint8_t>& read_span)
{
    uint32_t complete_size = 0;
    while (complete_size < read_span.size()) {
        const uint32_t ReadSize = complete_size + 256 <= read_span.size()
                                    ? 256
                                    : read_span.size() - complete_size;
        const auto     Chunk    = read_span.subspan(complete_size, ReadSize);
        if (nv::flash::Flash::read(read_vir_address, Chunk) != nv::flash::Status::Ok) {
            return;
        }
        if (read_vir_address = nv::common::add(read_vir_address, ReadSize);
            read_vir_address == std::numeric_limits<decltype(read_vir_address)>::max()) {
            return;
        }
        if (complete_size = nv::common::add(complete_size, ReadSize);
            complete_size == std::numeric_limits<decltype(complete_size)>::max()) {
            return;
        }
    }
    return;
}

static uint16_t get_cert_len(const std::array<uint8_t, 4>& cert_data_span)
{
    uint16_t cert_len = 0;
    // certficate start with 0x3082xxxx ,where xxxx is length
    constexpr uint8_t X509SeqToken            = 0x30;
    constexpr uint8_t X509LengthEncodingToken = 0x82;
    constexpr uint8_t X509LengthByte          = 4;

    if (cert_data_span[0] == X509SeqToken && cert_data_span[1] == X509LengthEncodingToken) {
        cert_len += (cert_data_span[2] << 8u);
        if (cert_len > std::numeric_limits<decltype(cert_len)>::max() - X509LengthByte) {
            return 0;
        }
        cert_len += X509LengthByte;
        if (cert_len > std::numeric_limits<decltype(cert_len)>::max() - cert_data_span[3]) {
            return 0;
        }
        cert_len += cert_data_span[3];

        if (cert_len > sizeof(nv::spdm::certlib::CertArray)) {
            return 0;
        }
    }

    return cert_len;
}

template<typename... Args>
requires(sizeof...(Args) <= sizeof(nv::logger::EventData))
     && (std::conjunction_v<std::is_same<Args, uint8_t>...>)
     && (sizeof(nv::logger::EventData) == sizeof(uint64_t))
static void spdm_log_helper(nv::logger::EventStructItem event, const Args... event_payload)
{
    nv::logger::EventData log_data{event_payload...};
    nv::logger::info(event, log_data);
    nv::info("spdm log:0x%x, data:0x%x%x%x%x%x%x%x%x\n",
             event,
             log_data.at(0),
             log_data.at(1),
             log_data.at(2),
             log_data.at(3),
             log_data.at(4),
             log_data.at(5),
             log_data.at(6),
             log_data.at(7));
}

}  // namespace
uint16_t get_l1_cert_len()
{
    std::array<uint8_t, 4> cert_length_data{0x00};
    if (nv::ipc::SpdmDummyCertificates == true) {
        std::copy(nv::spdm::dummyCertificate::DummyL1Cert.begin(),
                  nv::spdm::dummyCertificate::DummyL1Cert.begin() + cert_length_data.size(),
                  cert_length_data.begin());
    }
    else if (nv::ipc::SpdmDummyCertificates == false) {
        std::copy(nv::spdm::cert::HardCodeL1Cert.begin(),
                  nv::spdm::cert::HardCodeL1Cert.begin() + cert_length_data.size(),
                  cert_length_data.begin());
    }
    return get_cert_len(cert_length_data);
}

uint16_t get_l2_cert_len()
{
    std::array<uint8_t, 4> cert_length_data{0x00};
    if (nv::ipc::SpdmDummyCertificates == true) {
        std::copy(nv::spdm::dummyCertificate::DummyL2Cert.begin(),
                  nv::spdm::dummyCertificate::DummyL2Cert.begin() + cert_length_data.size(),
                  cert_length_data.begin());
    }
    else if (nv::ipc::SpdmDummyCertificates == false) {
        std::copy(nv::spdm::cert::HardCodeL2Cert.begin(),
                  nv::spdm::cert::HardCodeL2Cert.begin() + cert_length_data.size(),
                  cert_length_data.begin());
    }
    return get_cert_len(cert_length_data);
}
uint16_t get_l3_cert_len()
{
    std::array<uint8_t, 4> cert_length_data{};
    if (nv::ipc::SpdmDummyCertificates == true) {
        std::copy(nv::spdm::dummyCertificate::DummyL3Cert.begin(),
                  nv::spdm::dummyCertificate::DummyL3Cert.begin() + cert_length_data.size(),
                  cert_length_data.begin());
        return get_cert_len(cert_length_data);
    }
    const std::span<uint8_t> CertLengthDataView{cert_length_data};
    read_from_flash(get_virtual_address(nv::spdm::cert::L3CertPhyAddr), CertLengthDataView);
    return get_cert_len(cert_length_data);
}

uint16_t get_l4_cert_len()
{
    return nv::spdm::cert::get_l4_cert_len_svc();
}

NV_SYS_CALL uint16_t get_l4_cert_len_svc()
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern SPDM_Get_L4_Cert_Len_Priv                     \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_SPDM_Get_L4_Cert_Len                   \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_SPDM_Get_L4_Cert_Len                 \n"
        " Privileged_SPDM_Get_L4_Cert_Len:                      \n"
        "     pop {r0}                                          \n"
        "     b SPDM_Get_L4_Cert_Len_Priv                       \n"
        " Unprivileged_SPDM_Get_L4_Cert_Len:                    \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_SPDM_Get_L4_Cert_Len)
        : "memory");
#else
    return SPDM_Get_L4_Cert_Len_Priv();
#endif
    // coverity[cert_msc52_cpp_violation] - Expect no return
}

NV_PRIVILEGED_FUNCTION uint16_t get_l4_cert_len_impl()
{
    std::array<uint8_t, 4> cert_length_data{};
    if constexpr (nv::ipc::SpdmDummyCertificates == true) {
        std::copy(nv::spdm::dummyCertificate::DummyL4Cert.begin(),
                  nv::spdm::dummyCertificate::DummyL4Cert.begin() + cert_length_data.size(),
                  cert_length_data.begin());
        return get_cert_len(cert_length_data);
    }
    else {
        // NOLINTBEGIN
        auto& l4_cache = *std::bit_cast<std::array<uint8_t, 0x800>*>(
            reinterpret_cast<uint8_t*>(L4CertRamAddr));
        // NOLINTEND
        std::copy(l4_cache.begin(), l4_cache.begin() + 4, cert_length_data.begin());
        return get_cert_len(cert_length_data);
    }
}

uint16_t get_l5_cert_len()
{
    return nv::spdm::cert::get_l5_cert_len_svc();
}

NV_SYS_CALL uint16_t get_l5_cert_len_svc()
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern SPDM_Get_L5_Cert_Len_Priv                     \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_SPDM_Get_L5_Cert_Len                   \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_SPDM_Get_L5_Cert_Len                 \n"
        " Privileged_SPDM_Get_L5_Cert_Len:                      \n"
        "     pop {r0}                                          \n"
        "     b SPDM_Get_L5_Cert_Len_Priv                       \n"
        " Unprivileged_SPDM_Get_L5_Cert_Len:                    \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_SPDM_Get_L5_Cert_Len)
        : "memory");
#else
    return SPDM_Get_L5_Cert_Len_Priv();
#endif
    // coverity[cert_msc52_cpp_violation] - Expect no return
}

NV_PRIVILEGED_FUNCTION uint16_t get_l5_cert_len_impl()
{
    std::array<uint8_t, 4> cert_length_data{};
    if constexpr (nv::ipc::SpdmDummyCertificates == true) {
        std::copy(nv::spdm::dummyCertificate::DummyL5Cert.begin(),
                  nv::spdm::dummyCertificate::DummyL5Cert.begin() + cert_length_data.size(),
                  cert_length_data.begin());
        return get_cert_len(cert_length_data);
    }
    else {
        const auto ReadRamAddr = static_cast<uint32_t>(
                                     nv::bootloader::Driver::current_boot_index())
                                      == 0
                                   ? nv::spdm::cert::AkCertRamAddr0
                                   : nv::spdm::cert::AkCertRamAddr1;
        // NOLINTBEGIN
        cert_length_data = *std::bit_cast<std::array<uint8_t, 4>*>(
            reinterpret_cast<uint8_t*>(ReadRamAddr));
        // NOLINTEND
        return get_cert_len(cert_length_data);
    }
}

uint16_t read_l1_cert(std::span<uint8_t> input_buffer)
{
    auto                         cert_len = get_l1_cert_len();
    nv::spdm::certlib::CertArray hard_code_cert{0x00};
    if (nv::ipc::SpdmDummyCertificates == true) {
        hard_code_cert = nv::spdm::dummyCertificate::DummyL1Cert;
    }
    else if (nv::ipc::SpdmDummyCertificates == false) {
        hard_code_cert = nv::spdm::cert::HardCodeL1Cert;
    }
    std::copy(hard_code_cert.begin(), hard_code_cert.begin() + cert_len, input_buffer.begin());
    return cert_len;
}
uint16_t read_l2_cert(std::span<uint8_t> input_buffer)
{
    auto                         cert_len = get_l2_cert_len();
    nv::spdm::certlib::CertArray hard_code_cert{0x00};
    if (nv::ipc::SpdmDummyCertificates == true) {
        hard_code_cert = nv::spdm::dummyCertificate::DummyL2Cert;
    }
    else if (nv::ipc::SpdmDummyCertificates == false) {
        hard_code_cert = nv::spdm::cert::HardCodeL2Cert;
    }
    std::copy(hard_code_cert.begin(), hard_code_cert.begin() + cert_len, input_buffer.begin());
    return cert_len;
}
uint16_t read_l3_cert(std::span<uint8_t> input_buffer)
{
    auto cert_len = get_l3_cert_len();
    if (cert_len > input_buffer.size()) {
        return 0;
    }
    if (nv::ipc::SpdmDummyCertificates == true) {
        std::copy(nv::spdm::dummyCertificate::DummyL3Cert.begin(),
                  nv::spdm::dummyCertificate::DummyL3Cert.begin() + cert_len,
                  input_buffer.begin());
    }
    else if (nv::ipc::SpdmDummyCertificates == false) {
        read_from_flash(get_virtual_address(nv::spdm::cert::L3CertPhyAddr),
                        input_buffer.subspan(0, cert_len));
    }
    return cert_len;
}

uint16_t get_l4_csr_len()
{
    return nv::spdm::cert::get_l4_csr_len_svc();
}

NV_SYS_CALL uint16_t get_l4_csr_len_svc()
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern SPDM_Get_L4_Csr_Len_Priv                      \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_SPDM_Get_L4_Csr_Len                    \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_SPDM_Get_L4_Csr_Len                  \n"
        " Privileged_SPDM_Get_L4_Csr_Len:                       \n"
        "     pop {r0}                                          \n"
        "     b SPDM_Get_L4_Csr_Len_Priv                        \n"
        " Unprivileged_SPDM_Get_L4_Csr_Len:                     \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_SPDM_Get_L4_Csr_Len)
        : "memory");
#else
    return SPDM_Get_L4_Csr_Len_Priv();
#endif
    // coverity[cert_msc52_cpp_violation] - Expect no return
}

NV_PRIVILEGED_FUNCTION uint16_t get_l4_csr_len_impl()
{
    // NOLINTBEGIN
    nv::spdm::certlib::CsrHeader& csr_header = *std::bit_cast<nv::spdm::certlib::CsrHeader*>(
        reinterpret_cast<uint8_t*>(nv::spdm::cert::L4CsrRamAddr));
    // NOLINTEND
    if (csr_header.hdr_size > std::numeric_limits<decltype(csr_header.hdr_size)>::max()
                                  - sizeof(nv::spdm::certlib::CsrHeader)
        || csr_header.hdr_size + sizeof(nv::spdm::certlib::CsrHeader)
               > nv::spdm::certlib::MaxCertSize
        || csr_header.hdr_size == 0) {  // limit the read size, avoid reading
        // the invalid size.
        nv::info("csr size invalid %d", (csr_header.hdr_size + 0));
        return 0;
    }
    return csr_header.hdr_size + sizeof(nv::spdm::certlib::CsrHeader);
}

uint16_t read_l4_csr(std::span<uint8_t>& input_buffer)
{
    return nv::spdm::cert::read_l4_csr_svc(input_buffer);
}

NV_SYS_CALL uint16_t read_l4_csr_svc(std::span<uint8_t>& input_buffer)
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern SPDM_Read_L4_Csr_Priv                         \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_SPDM_Read_L4_Csr                       \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_SPDM_Read_L4_Csr                     \n"
        " Privileged_SPDM_Read_L4_Csr:                          \n"
        "     pop {r0}                                          \n"
        "     b SPDM_Read_L4_Csr_Priv                           \n"
        " Unprivileged_SPDM_Read_L4_Csr:                        \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_SPDM_Read_L4_Csr)
        : "memory");
#else
    return SPDM_Read_L4_Csr_Priv(input_buffer);
#endif
    // coverity[cert_msc52_cpp_violation] - Expect no return
}

NV_PRIVILEGED_FUNCTION uint16_t read_l4_csr_impl(std::span<uint8_t>& input_buffer)
{
    if (!nv::ipc::Supervisor::inst()
             .task(nv::ipc::TaskId::Mctp)
             .checking_parameter_is_from_self_stack(input_buffer)) {
        return 0;
    }
    auto cert_len = get_l4_csr_len();
    // NOLINTBEGIN
    auto& csr = *std::bit_cast<std::array<uint8_t, 0x800>*>(
        reinterpret_cast<uint8_t*>(L4CsrRamAddr));
    // NOLINTEND
    if (cert_len > input_buffer.size() || cert_len > csr.size()) {
        return 0;
    }
    std::copy(csr.begin(), csr.begin() + cert_len, input_buffer.begin());
    return cert_len;
}

bool construct_l5_cert(nv::spdm::certlib::CertArray& l5_cert_array)
{
    return construct_l5_cert_svc(l5_cert_array);
}

NV_SYS_CALL bool construct_l5_cert_svc(nv::spdm::certlib::CertArray& l5_cert_array)
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern SPDM_Construct_L5_Cert_Priv                    \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_SPDM_Construct_L5_Cert                  \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_SPDM_Construct_L5_Cert                \n"
        " Privileged_SPDM_Construct_L5_Cert:                     \n"
        "     pop {r0}                                          \n"
        "     b SPDM_Construct_L5_Cert_Priv                      \n"
        " Unprivileged_SPDM_Construct_L5_Cert:                   \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_SPDM_Construct_L5_Cert)
        : "memory");
#else
    return SPDM_Construct_L5_Cert_Priv(l5_cert_array);
#endif
    // coverity[cert_msc52_cpp_violation] - Expect no return
}

NV_PRIVILEGED_FUNCTION
bool construct_l5_cert_impl(nv::spdm::certlib::CertArray& l5_cert_array)
{
    if (!nv::ipc::Supervisor::inst()
             .task(nv::ipc::TaskId::Spdm)
             .checking_parameter_is_from_self_stack(l5_cert_array)) {
        return false;
    }

    const auto L5CertAddr = static_cast<uint32_t>(nv::bootloader::Driver::current_boot_index())
                                 == 0
                              ? nv::spdm::cert::AkCertRamAddr0
                              : nv::spdm::cert::AkCertRamAddr1;
    const auto L5SignatureAddr = static_cast<uint32_t>(
                                     nv::bootloader::Driver::current_boot_index())
                                      == 0
                                   ? nv::spdm::cert::AkSignatureRamAddr0
                                   : nv::spdm::cert::AkSignatureRamAddr1;
    // NOLINTBEGIN
    auto& dev_ak_cert = *std::bit_cast<nv::spdm::ak::DevAkTemplate*>(
        reinterpret_cast<uint8_t*>(L5CertAddr));
    // NOLINTEND
    /* turn the generated signature into cert format*/
    // NOLINTBEGIN
    auto& signature = *std::bit_cast<std::array<uint8_t, 96>*>(
        reinterpret_cast<uint8_t*>(L5SignatureAddr));
    // NOLINTEND

    nv::spdm::certlib::Signature cert_sign{};

    {
        uint32_t r_start_offset = 0;
        uint32_t s_start_offset = nv::spdm::certlib::Ecdsa384PublicKeySize / 2;
        bool     r_need_padding = false;
        bool     s_need_padding = false;

        /* find the start offset of signature r value */
        for (auto it = signature.begin();
             it != signature.begin() + nv::spdm::certlib::Ecdsa384PublicKeySize / 2
             && *it == 0x00;
             it++) {
            if (r_start_offset > std::numeric_limits<decltype(r_start_offset)>::max() - 1) {
                return false;
            }
            r_start_offset++;
        }
        // edge case, all of the signature value is zero
        if (r_start_offset >= nv::spdm::certlib::Ecdsa384PublicKeySize / 2) {
            r_start_offset = nv::spdm::certlib::Ecdsa384PublicKeySize / 2 - 1;
            nv::info("r signature is zero\n");
        }
        /* find the start offset of signature s value */
        for (auto it = signature.begin() + nv::spdm::certlib::Ecdsa384PublicKeySize / 2;
             it != signature.end() && *it == 0x00;
             it++) {
            if (s_start_offset > std::numeric_limits<decltype(s_start_offset)>::max() - 1) {
                return false;
            }
            s_start_offset++;
        }
        // edge case, all of the signature value is zero
        if (s_start_offset >= nv::spdm::certlib::Ecdsa384PublicKeySize) {
            s_start_offset = nv::spdm::certlib::Ecdsa384PublicKeySize - 1;
            nv::info("s signature is zero\n");
        }
        constexpr uint8_t SignMask = 0x80u;
        if ((signature.at(r_start_offset) & SignMask) != 0) {
            r_need_padding = true;
        }
        if ((signature.at(s_start_offset) & SignMask) != 0) {
            s_need_padding = true;
        }
        cert_sign.r_length_token = (r_need_padding == true ? 1 : 0)
                                 + nv::spdm::certlib::Ecdsa384PublicKeySize / 2
                                 - r_start_offset;
        cert_sign.s_length_token = (s_need_padding == true ? 1 : 0)
                                 + nv::spdm::certlib::Ecdsa384PublicKeySize - s_start_offset;
        std::copy(signature.begin() + r_start_offset,
                  signature.begin() + nv::spdm::certlib::Ecdsa384PublicKeySize / 2,
                  cert_sign.r_value.begin() + ((r_need_padding == true ? 1 : 0)));
        std::copy(signature.begin() + s_start_offset,
                  signature.end(),
                  cert_sign.s_value.begin() + ((s_need_padding == true ? 1 : 0)));
    }
    // update the length of signature and certificate
    cert_sign.sequence_small.length = cert_sign.r_length_token + cert_sign.s_length_token
                                    + sizeof(cert_sign.s_int_token)
                                    + sizeof(cert_sign.s_length_token)
                                    + sizeof(cert_sign.r_int_token)
                                    + sizeof(cert_sign.r_length_token);
    cert_sign.bit_string.length = cert_sign.sequence_small.length
                                + sizeof(cert_sign.sequence_small)
                                + sizeof(cert_sign.bit_string.padding_length);
    const uint32_t WholeCertLength = sizeof(nv::spdm::ak::DevAkTemplate)
                                   + ObjectIdentifier_1_2_840_10045_4_3_3.size()
                                   + cert_sign.bit_string.length
                                   + sizeof(cert_sign.bit_string.length)
                                   + sizeof(cert_sign.bit_string.token)
                                   - sizeof(dev_ak_cert.total_certificate);
    dev_ak_cert.total_certificate.length_msb = WholeCertLength >> 8u;
    dev_ak_cert.total_certificate.length_lsb = WholeCertLength % 256u;

    /* save the construct alais cert into ram*/
    // NOLINTBEGIN
    nv::spdm::certlib::CertArray&
        alais_cert_array = *std::bit_cast<nv::spdm::certlib::CertArray*>(
            reinterpret_cast<uint8_t*>(L5CertAddr));
    // NOLINTEND
    auto  cert_it         = alais_cert_array.begin() + sizeof(nv::spdm::ak::DevAkTemplate);
    auto& bit_string_view = *std::bit_cast<std::array<uint8_t, sizeof(Signature::bit_string)>*>(
        &cert_sign.bit_string);
    auto& sequence_small_view = *std::bit_cast<
        std::array<uint8_t, sizeof(Signature::sequence_small)>*>(&cert_sign.sequence_small);
    cert_it = std::copy(nv::spdm::certlib::ObjectIdentifier_1_2_840_10045_4_3_3.begin(),
                        nv::spdm::certlib::ObjectIdentifier_1_2_840_10045_4_3_3.end(),
                        cert_it);
    cert_it = std::copy(bit_string_view.begin(), bit_string_view.end(), cert_it);
    cert_it = std::copy(sequence_small_view.begin(), sequence_small_view.end(), cert_it);

    // write the signature into buffer
    *cert_it = cert_sign.r_int_token;
    ++cert_it;
    *cert_it = cert_sign.r_length_token;
    ++cert_it;

    cert_it = std::copy(cert_sign.r_value.begin(),
                        cert_sign.r_value.begin() + cert_sign.r_length_token,
                        cert_it);

    *cert_it = cert_sign.s_int_token;
    ++cert_it;
    *cert_it = cert_sign.s_length_token;
    ++cert_it;
    std::copy(cert_sign.s_value.begin(),
              cert_sign.s_value.begin() + cert_sign.s_length_token,
              cert_it);

    if (alais_cert_array.size() > l5_cert_array.size()) {
        return false;
    }
    std::copy(alais_cert_array.begin(), alais_cert_array.end(), l5_cert_array.begin());
    return true;
}

bool generate_l5_cert()
{
    auto l5_cert_array = nv::spdm::certlib::CertArray();
    if (!construct_l5_cert(l5_cert_array)) {
        erase_l5_cert();
        return false;
    }
    // check dev Ik is successful generated.
    if (get_l4_cert_len() == 0) {
        spdm_log_helper(nv::logger::Event::SpdmDevAkGenerateFail,
                        static_cast<uint8_t>(DevAkGenerateErrorCode::L4CertNotFound));
        erase_l5_cert();
        return false;
    }

    nv::spdm::certlib::CertArray l4_cert{};
    auto                         l4_cert_span = std::span<uint8_t>(l4_cert);
    nv::spdm::cert::read_l4_cert(l4_cert_span);
    if (!nv::spdm::certlib::validate_certificate_signature(l4_cert, l5_cert_array)) {
        spdm_log_helper(nv::logger::Event::SpdmDevAkGenerateFail,
                        static_cast<uint8_t>(DevAkGenerateErrorCode::VerifySignatureFail));
        erase_l5_cert();
        return false;
    }

    return true;
}

bool read_devik_request(nv::spdm::ik::DevIkRequest&   dev_ik_req,
                        uint32_t                      dda_ordinal_number,
                        nv::spdm::certlib::CertArray& l3_cert_array)
{
    return read_devik_request_svc(dev_ik_req, dda_ordinal_number, l3_cert_array);
}

NV_SYS_CALL bool read_devik_request_svc(nv::spdm::ik::DevIkRequest&   dev_ik_req,
                                        uint32_t                      dda_ordinal_number,
                                        nv::spdm::certlib::CertArray& l3_cert_array)
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern SPDM_Read_DevIk_Request_Priv                  \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_SPDM_Read_DevIk_Request                \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_SPDM_Read_DevIk_Request              \n"
        " Privileged_SPDM_Read_DevIk_Request:                   \n"
        "     pop {r0}                                          \n"
        "     b SPDM_Read_DevIk_Request_Priv                    \n"
        " Unprivileged_SPDM_Read_DevIk_Request:                 \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_SPDM_Read_DevIk_Request)
        : "memory");
#else
    return SPDM_Read_DevIk_Request_Priv(dev_ik_req, dda_ordinal_number, l3_cert_array);
#endif
    // coverity[cert_msc52_cpp_violation] - Expect no return
}

NV_PRIVILEGED_FUNCTION bool read_devik_request_impl(nv::spdm::ik::DevIkRequest& dev_ik_req,
                                                    uint32_t dda_ordinal_number,
                                                    nv::spdm::certlib::CertArray& l3_cert_array)
{
    if (!nv::ipc::Supervisor::inst()
             .task(nv::ipc::TaskId::Spdm)
             .checking_parameter_is_from_self_stack(dev_ik_req, l3_cert_array)) {
        return false;
    }
    // NOLINTBEGIN
    auto& cert_buffer = *std::bit_cast<nv::spdm::certlib::CertArray*>(
        reinterpret_cast<uint8_t*>(get_virtual_address(nv::spdm::cert::L3CertPhyAddr)));
    // NOLINTEND

    if (cert_buffer.size() > l3_cert_array.size()) {
        return false;
    }

    std::copy(cert_buffer.begin(), cert_buffer.end(), l3_cert_array.begin());
    // serial number
    // NOLINTBEGIN
    auto const& UuidArrayOnRam = *std::bit_cast<std::array<uint8_t, 16u>*>(
        reinterpret_cast<uint8_t*>(DevUuidRamAddr));
    // NOLINTEND
    dev_ik_req.serial_number = UuidArrayOnRam;

    // not found dda number in efuse
    if (dda_ordinal_number == 0) {
        spdm_log_helper(nv::logger::Event::SpdmDevIkGenerateFail,
                        static_cast<uint8_t>(DevIkGenerateErrorCode::DdaNumberNotFound));
        return false;
    }

    // check the dda ordinal number of l3 cert in flash is the same on efuse
    const uint32_t DdaOrdinalNumberFlash = nv::spdm::certlib::parse_dda_ordinal_number(
        l3_cert_array);
    if (dda_ordinal_number != DdaOrdinalNumberFlash) {
        spdm_log_helper(nv::logger::Event::SpdmDevIkGenerateFail,
                        static_cast<uint8_t>(DevIkGenerateErrorCode::DdaNumberMismatch));
        return false;
    }

    for (uint8_t& dda_char : std::ranges::reverse_view(dev_ik_req.dda_ordinal_number)) {
        constexpr uint32_t Base10 = 10;
        dda_char = nv::spdm::certlib::hex_in_int_to_ascii(dda_ordinal_number % Base10);
        dda_ordinal_number /= Base10;
    }
    // fmc_ordinal_number from ram
    // NOLINTBEGIN
    const std::array<uint8_t, 5>& FmcOrdinalNumber = *std::bit_cast<std::array<uint8_t, 5>*>(
        reinterpret_cast<uint8_t*>(FmcOrdinalNumberRamAddr));
    // NOLINTEND
    dev_ik_req.fmc_ordinal_number = FmcOrdinalNumber;

    // read public key from ram
    // NOLINTBEGIN
    auto const& L4PubKeyArrayOnRam = *std::bit_cast<std::array<uint8_t, 96>*>(
        reinterpret_cast<uint8_t*>(L4PubKeyRamAddr));
    // NOLINTEND
    dev_ik_req.public_key = L4PubKeyArrayOnRam;
    // turn serial number into the printable value.
    for (uint32_t i = 0; i < dev_ik_req.serial_number.size(); i++) {
        const uint8_t Number                       = dev_ik_req.serial_number.at(i);
        const uint8_t UpperHalfByte                = Number / 16;
        const uint8_t LowerHalfByte                = Number % 16;
        dev_ik_req.subject_serial_number.at(i * 2) = nv::spdm::certlib::hex_in_int_to_ascii(
            UpperHalfByte);
        dev_ik_req.subject_serial_number.at(i * 2 + 1) = nv::spdm::certlib::hex_in_int_to_ascii(
            LowerHalfByte);
    }

    // find the L3 subject_key_identifier on the flash
    std::span<const uint8_t> l3_cert_span(l3_cert_array);
    dev_ik_req.authority_key_identifier = nv::spdm::certlib::find_subject_key_identifier(
        l3_cert_span);
    return true;
}

bool construct_l4_cert(nv::spdm::ik::DevIkHelper&    dev_ik_helper,
                       nv::spdm::certlib::CertArray& l4_cert_array)
{
    return construct_l4_cert_svc(dev_ik_helper, l4_cert_array);
}

NV_SYS_CALL bool construct_l4_cert_svc(nv::spdm::ik::DevIkHelper&    dev_ik_helper,
                                       nv::spdm::certlib::CertArray& l4_cert_array)
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern SPDM_Construct_L4_Cert_Priv                    \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_SPDM_Construct_L4_Cert                 \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_SPDM_Construct_L4_Cert               \n"
        " Privileged_SPDM_Construct_L4_Cert:                    \n"
        "     pop {r0}                                          \n"
        "     b SPDM_Construct_L4_Cert_Priv                      \n"
        " Unprivileged_SPDM_Construct_L4_Cert:                  \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_SPDM_Construct_L4_Cert)
        : "memory");
#else
    return SPDM_Construct_L4_Cert_Priv(dev_ik_helper, l4_cert_array);
#endif
    // coverity[cert_msc52_cpp_violation] - Expect no return
}

NV_PRIVILEGED_FUNCTION bool construct_l4_cert_impl(nv::spdm::ik::DevIkHelper&    dev_ik_helper,
                                                   nv::spdm::certlib::CertArray& l4_cert_array)
{
    if (!nv::ipc::Supervisor::inst()
             .task(nv::ipc::TaskId::Spdm)
             .checking_parameter_is_from_self_stack(dev_ik_helper, l4_cert_array)) {
        return false;
    }
    // get spdm cache buffer
    // NOLINTBEGIN
    auto& cert_buffer = *std::bit_cast<nv::spdm::certlib::CertArray*>(
        reinterpret_cast<uint8_t*>(L4CertRamAddr));
    // NOLINTEND
    std::span<uint8_t> cert_span_on_cache(cert_buffer);
    dev_ik_helper.construct_cert(cert_span_on_cache);
    if (cert_buffer.size() > l4_cert_array.size()) {
        return false;
    }
    std::copy(cert_buffer.begin(), cert_buffer.end(), l4_cert_array.begin());

    return true;
}

bool generate_l4_cert()
{
    // check L3 is exist
    if (nv::spdm::cert::get_l3_cert_len() == 0) {
        spdm_log_helper(nv::logger::Event::SpdmDevIkGenerateFail,
                        static_cast<uint8_t>(DevIkGenerateErrorCode::L3CertNotFound));
        return false;
    }

    // dda_ordinal_number from otp
    constexpr uint32_t DdaOrdinalEfuseAddr = 31;
    uint32_t           dda_ordinal_number  = 0;
    // efuse read fail
    if (nv::flash::Status::Ok
        != nv::flash::Flash::read_efuse(DdaOrdinalEfuseAddr, dda_ordinal_number)) {
        spdm_log_helper(nv::logger::Event::SpdmDevIkGenerateFail,
                        static_cast<uint8_t>(DevIkGenerateErrorCode::DdaNumberReadEfuseFail));
        return false;
    }

    auto l3_cert_array = nv::spdm::certlib::CertArray();

    nv::spdm::ik::DevIkRequest dev_ik_req{};
    if (!read_devik_request(dev_ik_req, dda_ordinal_number, l3_cert_array)) {
        return false;
    }
    // dev_ik_req.subject_key_identifier
    // calculate the hash value of pub key
    constexpr uint32_t
        Ecdsa384PublicWithUncompressTokenTokenSize = nv::spdm::certlib::Ecdsa384PublicKeySize
                                                   + 1;
    std::array<uint8_t, Ecdsa384PublicWithUncompressTokenTokenSize>
        public_key_with_compress_token{};
    public_key_with_compress_token.at(0) = 0x04;
    std::copy(dev_ik_req.public_key.begin(),
              dev_ik_req.public_key.end(),
              public_key_with_compress_token.begin() + 1);
    nv::spdm::crypto::spdm_hash_sha1(dev_ik_req.subject_key_identifier.data(),
                                     public_key_with_compress_token.data(),
                                     public_key_with_compress_token.size());
    //   Ecdsa-Sig-Value  ::=  SEQUENCE  {
    //        r     INTEGER,
    //        s     INTEGER  }
    // prepare the signature of the sequence
    {
        // r part of signature read from otp
        constexpr std::array<uint32_t, 12> SignatureEfuseAddrR = {
            32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43};

        auto r_value_iter = dev_ik_req.signature.r_value.rbegin();
        for (const uint32_t EfuseAddr : std::ranges::reverse_view(SignatureEfuseAddrR)) {
            uint32_t opt_data = 0;
            // read efuse fail
            if (nv::flash::Status::Ok != nv::flash::Flash::read_efuse(EfuseAddr, opt_data)) {
                spdm_log_helper(
                    nv::logger::Event::SpdmDevIkGenerateFail,
                    static_cast<uint8_t>(DevIkGenerateErrorCode::SignatureReadEfuseFail),
                    static_cast<uint8_t>(EfuseAddr));
                return false;
            }
            auto opt_data_view = *std::bit_cast<
                std::array<uint8_t, sizeof(decltype(opt_data))>*>(&opt_data);
            for (const uint8_t OptDataChar : std::ranges::reverse_view(opt_data_view)) {
                // coverity[cert_ctr50_cpp_violation]
                *r_value_iter++ = OptDataChar;
            }
        }
        // check the length value
        uint32_t leading_zero_length = 0;
        for (auto it = dev_ik_req.signature.r_value.begin();
             it != dev_ik_req.signature.r_value.end() && *it == 0;
             it++) {
            // coverity[cert_int30_c_violation]
            leading_zero_length++;
        }
        uint32_t           copy_offset              = 0;
        constexpr uint8_t  SignMask                 = 0x80;
        constexpr uint32_t MaxLengthOfHalfSignature = 49;

        // check need the padding for uint or not
        if (leading_zero_length == MaxLengthOfHalfSignature) {  // edge case
            dev_ik_req.signature.r_length_token = 0;
            copy_offset                         = 0;
        }
        else if ((dev_ik_req.signature.r_value.at(leading_zero_length) & SignMask) != 0) {
            dev_ik_req.signature.r_length_token = MaxLengthOfHalfSignature - leading_zero_length
                                                + 1;
            copy_offset = 1;
        }
        else {
            dev_ik_req.signature.r_length_token = MaxLengthOfHalfSignature
                                                - leading_zero_length;
            copy_offset = 0;
        }

        // move the r_value to the start of array
        decltype(dev_ik_req.signature.r_value) temp{0};
        std::copy(dev_ik_req.signature.r_value.begin() + leading_zero_length,
                  dev_ik_req.signature.r_value.end(),
                  temp.begin() + copy_offset);
        dev_ik_req.signature.r_value = temp;
    }
    {
        // s part of signature read from otp
        constexpr std::array<uint32_t, 12> SignatureEfuseAddrS = {
            44, 45, 46, 47, 48, 49, 50, 63, 64, 65, 66, 67};

        auto s_value_iter = dev_ik_req.signature.s_value.rbegin();
        for (const uint32_t EfuseAddr : std::ranges::reverse_view(SignatureEfuseAddrS)) {
            uint32_t opt_data = 0;
            // read efuse fail
            if (nv::flash::Status::Ok != nv::flash::Flash::read_efuse(EfuseAddr, opt_data)) {
                spdm_log_helper(
                    nv::logger::Event::SpdmDevIkGenerateFail,
                    static_cast<uint8_t>(DevIkGenerateErrorCode::SignatureReadEfuseFail),
                    static_cast<uint8_t>(EfuseAddr));
                return false;
            }
            auto opt_data_view = *std::bit_cast<
                std::array<uint8_t, sizeof(decltype(opt_data))>*>(&opt_data);
            for (const uint8_t OptDataChar : std::ranges::reverse_view(opt_data_view)) {
                // coverity[cert_ctr50_cpp_violation]
                *s_value_iter++ = OptDataChar;
            }
        }
        // check the length value
        uint32_t leading_zero_length = 0;
        for (auto it = dev_ik_req.signature.s_value.begin();
             it != dev_ik_req.signature.s_value.end() && *it == 0;
             it++) {
            // coverity[cert_int30_c_violation]
            leading_zero_length++;
        }
        uint32_t           copy_offset              = 0;
        constexpr uint8_t  SignMask                 = 0x80;
        constexpr uint32_t MaxLengthOfHalfSignature = 49;
        // check need the padding for uint or not
        if (leading_zero_length == MaxLengthOfHalfSignature) {  // edge case
            dev_ik_req.signature.s_length_token = 0;
            copy_offset                         = 0;
        }
        else if ((dev_ik_req.signature.s_value.at(leading_zero_length) & SignMask) != 0) {
            dev_ik_req.signature.s_length_token = MaxLengthOfHalfSignature - leading_zero_length
                                                + 1;
            copy_offset = 1;
        }
        else {
            dev_ik_req.signature.s_length_token = MaxLengthOfHalfSignature
                                                - leading_zero_length;
            copy_offset = 0;
        }

        // move the s_value to the start of array
        decltype(dev_ik_req.signature.s_value) temp{0};
        std::copy(dev_ik_req.signature.s_value.begin() + leading_zero_length,
                  dev_ik_req.signature.s_value.end(),
                  temp.begin() + copy_offset);
        dev_ik_req.signature.s_value = temp;
    }
    // calculate sequence length on signature
    // coverity[cert_int31_c_violation]
    dev_ik_req.signature.sequence_small.length = dev_ik_req.signature.r_length_token
                                               + dev_ik_req.signature.s_length_token
                                               + sizeof(dev_ik_req.signature.s_int_token)
                                               + sizeof(dev_ik_req.signature.s_length_token)
                                               + sizeof(dev_ik_req.signature.r_int_token)
                                               + sizeof(dev_ik_req.signature.r_length_token);
    // calculate bit string length on signature
    // coverity[cert_int31_c_violation]
    dev_ik_req.signature.bit_string
        .length = dev_ik_req.signature.sequence_small.length
                + sizeof(decltype(dev_ik_req.signature.sequence_small))
                + sizeof(dev_ik_req.signature.bit_string.padding_length);

    // start to reconstruct the ik cert
    auto dev_ik_helper = nv::spdm::ik::DevIkHelper(dev_ik_req);
    auto l4_cert_array = nv::spdm::certlib::CertArray();

    if (!construct_l4_cert(dev_ik_helper, l4_cert_array)) {
        erase_l4_cert();
        return false;
    }
    if (!nv::spdm::certlib::validate_certificate_signature(l3_cert_array, l4_cert_array)) {
        spdm_log_helper(nv::logger::Event::SpdmDevIkGenerateFail,
                        static_cast<uint8_t>(DevIkGenerateErrorCode::VerifySignatureFail));
        erase_l4_cert();
        return false;
    }
    return true;
}

void erase_l4_cert()
{
    nv::spdm::cert::erase_l4_cert_svc();
}

void erase_l5_cert()
{
    nv::spdm::cert::erase_l5_cert_svc();
}

NV_SYS_CALL void erase_l4_cert_svc()
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern SPDM_Erase_L4_Cert_Priv                       \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_SPDM_Erase_L4_Cert                     \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_SPDM_Erase_L4_Cert                   \n"
        " Privileged_SPDM_Erase_L4_Cert:                        \n"
        "     pop {r0}                                          \n"
        "     b SPDM_Erase_L4_Cert_Priv                         \n"
        " Unprivileged_SPDM_Erase_L4_Cert:                      \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_SPDM_Erase_L4_Cert)
        : "memory");
#else
    SPDM_Erase_L4_Cert_Priv();
#endif
}

NV_SYS_CALL void erase_l5_cert_svc()
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern SPDM_Erase_L5_Cert_Priv                       \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_SPDM_Erase_L5_Cert                     \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_SPDM_Erase_L5_Cert                   \n"
        " Privileged_SPDM_Erase_L5_Cert:                        \n"
        "     pop {r0}                                          \n"
        "     b SPDM_Erase_L5_Cert_Priv                         \n"
        " Unprivileged_SPDM_Erase_L5_Cert:                      \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_SPDM_Erase_L5_Cert)
        : "memory");
#else
    SPDM_Erase_L5_Cert_Priv();
#endif
}

NV_PRIVILEGED_FUNCTION void erase_l4_cert_impl()
{
    nv::spdm::certlib::CertArray& l4_cert_array = *std::bit_cast<nv::spdm::certlib::CertArray*>(
        reinterpret_cast<uint8_t*>(L4CertRamAddr));  // NOLINT
    l4_cert_array.fill(0x00);
}

NV_PRIVILEGED_FUNCTION void erase_l5_cert_impl()
{
    const auto L5CertAddr = static_cast<uint32_t>(nv::bootloader::Driver::current_boot_index())
                                 == 0
                              ? nv::spdm::cert::AkCertRamAddr0
                              : nv::spdm::cert::AkCertRamAddr1;

    nv::spdm::certlib::CertArray& l5_cert_array = *std::bit_cast<nv::spdm::certlib::CertArray*>(
        reinterpret_cast<uint8_t*>(L5CertAddr));  // NOLINT
    l5_cert_array.fill(0x00);
}
uint16_t read_l4_cert(std::span<uint8_t>& input_buffer)
{
    return nv::spdm::cert::read_l4_cert_svc(input_buffer);
}

NV_SYS_CALL uint16_t read_l4_cert_svc(std::span<uint8_t>& input_buffer)
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern SPDM_Read_L4_Cert_Priv                        \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_SPDM_Read_L4_Cert                      \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_SPDM_Read_L4_Cert                    \n"
        " Privileged_SPDM_Read_L4_Cert:                         \n"
        "     pop {r0}                                          \n"
        "     b SPDM_Read_L4_Cert_Priv                          \n"
        " Unprivileged_SPDM_Read_L4_Cert:                       \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_SPDM_Read_L4_Cert)
        : "memory");
#else
    return SPDM_Read_L4_Cert_Priv(input_buffer);
#endif
    // coverity[cert_msc52_cpp_violation] - Expect no return
}

NV_PRIVILEGED_FUNCTION uint16_t read_l4_cert_impl(std::span<uint8_t>& input_buffer)
{
    if (!nv::ipc::Supervisor::inst()
             .task(nv::ipc::TaskId::Spdm)
             .checking_parameter_is_from_self_stack(input_buffer)) {
        return 0;
    }
    auto cert_len = get_l4_cert_len();
    if (cert_len > input_buffer.size()) {
        return 0;
    }
    if (nv::ipc::SpdmDummyCertificates == true) {
        std::copy(nv::spdm::dummyCertificate::DummyL4Cert.begin(),
                  nv::spdm::dummyCertificate::DummyL4Cert.begin() + cert_len,
                  input_buffer.begin());
    }
    else if (nv::ipc::SpdmDummyCertificates == false) {
        // NOLINTBEGIN
        auto& cert = *std::bit_cast<std::array<uint8_t, 0x800>*>(
            reinterpret_cast<uint8_t*>(L4CertRamAddr));
        // NOLINTEND
        std::copy(cert.begin(), cert.begin() + cert_len, input_buffer.begin());
    }
    return cert_len;
}

uint16_t read_l5_cert(std::span<uint8_t>& input_buffer)
{
    return nv::spdm::cert::read_l5_cert_svc(input_buffer);
}

NV_SYS_CALL uint16_t read_l5_cert_svc(std::span<uint8_t>& input_buffer)
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern SPDM_Read_L5_Cert_Priv                        \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_SPDM_Read_L5_Cert                      \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_SPDM_Read_L5_Cert                    \n"
        " Privileged_SPDM_Read_L5_Cert:                         \n"
        "     pop {r0}                                          \n"
        "     b SPDM_Read_L5_Cert_Priv                          \n"
        " Unprivileged_SPDM_Read_L5_Cert:                       \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_SPDM_Read_L5_Cert)
        : "memory");
#else
    return SPDM_Read_L5_Cert_Priv(input_buffer);
#endif
    // coverity[cert_msc52_cpp_violation] - Expect no return
}

NV_PRIVILEGED_FUNCTION uint16_t read_l5_cert_impl(std::span<uint8_t>& input_buffer)
{
    if (!nv::ipc::Supervisor::inst()
             .task(nv::ipc::TaskId::Spdm)
             .checking_parameter_is_from_self_stack(input_buffer)) {
        return 0;
    }
    auto cert_len = get_l5_cert_len();
    if (cert_len > input_buffer.size()) {
        return 0;
    }
    if constexpr (nv::ipc::SpdmDummyCertificates == true) {
        std::copy(nv::spdm::dummyCertificate::DummyL5Cert.begin(),
                  nv::spdm::dummyCertificate::DummyL5Cert.begin() + cert_len,
                  input_buffer.begin());
    }
    else if (nv::ipc::SpdmDummyCertificates == false) {
        const auto ReadRamAddr = static_cast<uint32_t>(
                                     nv::bootloader::Driver::current_boot_index())
                                      == 0
                                   ? nv::spdm::cert::AkCertRamAddr0
                                   : nv::spdm::cert::AkCertRamAddr1;
        // NOLINTBEGIN
        auto& cert = *std::bit_cast<nv::spdm::certlib::CertArray*>(
            reinterpret_cast<uint8_t*>(ReadRamAddr));
        // NOLINTEND
        std::copy(cert.begin(), cert.begin() + cert_len, input_buffer.begin());
    }

    return cert_len;
}
void get_l5_private_key(Ecdsa384PrivateKeyArray& l5_private_key)
{
    return nv::spdm::cert::get_l5_private_key_svc(l5_private_key);
}

NV_SYS_CALL void get_l5_private_key_svc(Ecdsa384PrivateKeyArray& l5_private_key)
{
#if ((configENABLE_MPU == 1) && (configUSE_MPU_WRAPPERS_V1 == 0))
    __asm volatile(  // NOLINT
        " .syntax unified                                       \n"
        " .extern SPDM_Get_L5_Private_Key_Priv                  \n"
        "                                                       \n"
        " push {r0}                                             \n"
        " mrs r0, ipsr                                          \n"
        " cmp r0, #0                                            \n"
        " bne Privileged_SPDM_Get_L5_Private_Key                \n"
        " mrs r0, control                                       \n"
        " tst r0, #1                                            \n"
        " bne Unprivileged_SPDM_Get_L5_Private_Key              \n"
        " Privileged_SPDM_Get_L5_Private_Key:                   \n"
        "     pop {r0}                                          \n"
        "     b SPDM_Get_L5_Private_Key_Priv                    \n"
        " Unprivileged_SPDM_Get_L5_Private_Key:                 \n"
        "     pop {r0}                                          \n"
        "     svc %0                                            \n"
        "                                                       \n"
        :
        : "i"(NV_SYSTEM_CALL_SPDM_Get_L5_Private_Key)
        : "memory");
#else
    return SPDM_Get_L5_Private_Key_Priv(l5_private_key);
#endif
}

NV_PRIVILEGED_FUNCTION void get_l5_private_key_impl(Ecdsa384PrivateKeyArray& l5_private_key)
{
    if (!nv::ipc::Supervisor::inst()
             .task(nv::ipc::TaskId::Spdm)
             .checking_parameter_is_from_self_stack(l5_private_key)) {
        return;
    }

    if constexpr (nv::ipc::SpdmDummyCertificates == true) {
        std::copy(nv::spdm::dummyCertificate::DummyL5PrivateKey.begin(),
                  nv::spdm::dummyCertificate::DummyL5PrivateKey.begin()
                      + sizeof(l5_private_key),
                  l5_private_key.begin());
        return;
    }
    else {
        const auto ReadRamAddr = static_cast<uint32_t>(
                                     nv::bootloader::Driver::current_boot_index())
                                      == 0
                                   ? nv::spdm::cert::AkPriKeyRamAddr0
                                   : nv::spdm::cert::AkPriKeyRamAddr1;

        // NOLINTBEGIN
        std::copy(reinterpret_cast<uint8_t*>(ReadRamAddr),
                  reinterpret_cast<uint8_t*>(ReadRamAddr) + sizeof(l5_private_key),
                  l5_private_key.begin());
        // NOLINTEND
    }
}

std::array<uint8_t, MaxCertChainLength> get_cert_chain()
{
    std::array<uint8_t, MaxCertChainLength>      cert_chain_buffer{};
    const std::span<uint8_t, MaxCertChainLength> CertChainBufferView{cert_chain_buffer};
    // prepare cert chain format
    // length(2 Bytes) + Reserved(2 Bytes) + RootHash(48 Bytes) + certicates
    CertchainheaderT& header = *std::bit_cast<CertchainheaderT*>(
        CertChainBufferView.subspan(0, 2 + 2 + nv::spdm::crypto::Sha384HashSize).data());
    header.reserved = 0;
    header.length   = get_cert_chain_length(0);

    uint16_t cert_offset  = sizeof(CertchainheaderT);
    cert_offset          += read_l1_cert(CertChainBufferView.subspan(cert_offset));
    cert_offset          += read_l2_cert(CertChainBufferView.subspan(cert_offset));
    cert_offset          += read_l3_cert(CertChainBufferView.subspan(cert_offset));
    auto l4_cert_span     = CertChainBufferView.subspan(cert_offset);
    auto l4_cert_len      = read_l4_cert(l4_cert_span);
    if (UINT16_MAX - cert_offset < l4_cert_len) {
        nv::error("invalid certificate chain length");
        return cert_chain_buffer;
    }
    cert_offset       += l4_cert_len;
    auto l5_cert_span  = CertChainBufferView.subspan(cert_offset);
    read_l5_cert(l5_cert_span);

    const nv::spdm::crypto::CryptoStatus CryptoRet = nv::spdm::crypto::spdm_hash_data(
        &header.root_hash[0],
        CertChainBufferView.subspan(sizeof(CertchainheaderT)).data(),
        get_l1_cert_len());
    if (CryptoRet != nv::spdm::crypto::CryptoStatus::Success) {
        nv::info("root hash fail %d", CryptoRet);
    }

    return cert_chain_buffer;
}

/*
 *  get_digest_for_slot()
 *
 *  This function gets the digest for the certificate chain associated
 *  with the slot number.  Fixed hash algorithm of SHA_384.  Digest
 *  buffer is assumed to be large enough to hold the hash.
 *
 *  slot_num is 0 based
 *
 *  Returns: number of bytes in digest
 */
uint16_t get_digest_for_slot(uint8_t slot_num, uint8_t* digest)
{
    if (!valid_slot(slot_num)) {
        nv::info("get digest at invalid slot %d", slot_num);
        return nv::spdm::crypto::Sha384HashSize;
    }
    std::array<uint8_t, MaxCertChainLength> cert_chain_buffer = get_cert_chain();
    const nv::spdm::crypto::CryptoStatus    CryptoRet = nv::spdm::crypto::spdm_hash_data(
        digest, cert_chain_buffer.data(), get_cert_chain_length(0));
    if (CryptoRet != nv::spdm::crypto::CryptoStatus::Success) {
        nv::info("digest of slot fail %d", CryptoRet);
    }
    return nv::spdm::crypto::Sha384HashSize;
}

uint8_t get_slot_mask()
{
    // check the L3~L5 cert are exist.
    if (get_l5_cert_len() != 0 && get_l4_cert_len() != 0 && get_l3_cert_len() != 0) {
        return 0x1u;
    }
    // no cert avaliable
    return 0x0u;
}

bool valid_slot(uint8_t slot_id)
{
    std::bitset<MaxSlots> slot_available_index(get_slot_mask());
    if (slot_id >= 8 || slot_available_index[slot_id] == 0) {
        return false;
    }
    return true;
}

bool verify_l2_l3_cert()
{
    CertArray l2_cert{};
    CertArray l3_cert{};
    // read the l2 and l3 cert from flash
    if (get_l2_cert_len() == 0) {
        spdm_log_helper(nv::logger::Event::SpdmL3CertGenerateFail,
                        static_cast<uint8_t>(L3CertGenerateErrorCode::L2CertNotExist));
        return false;
    }
    read_l2_cert(std::span<uint8_t>(l2_cert));
    if (get_l3_cert_len() == 0) {
        spdm_log_helper(nv::logger::Event::SpdmL3CertGenerateFail,
                        static_cast<uint8_t>(L3CertGenerateErrorCode::L3CertNotExist));
        return false;
    }
    read_l3_cert(std::span<uint8_t>(l3_cert));
    if (nv::spdm::certlib::validate_certificate_signature(l2_cert, l3_cert) != true) {
        spdm_log_helper(nv::logger::Event::SpdmL3CertGenerateFail,
                        static_cast<uint8_t>(L3CertGenerateErrorCode::VerifySignatureFail));
        return false;
    }
    return true;
}

uint16_t get_cert_data_for_slot(uint8_t slot_num, uint8_t* data, uint16_t offset, uint16_t size)
{
    if (!valid_slot(slot_num)) {
        nv::info("get certificate data at invalid slot %d\n", slot_num);
        return 0;
    }

    std::array<uint8_t, MaxCertChainLength> cert_chain_buffer = get_cert_chain();

    if (offset > std::numeric_limits<decltype(offset)>::max() - size) {
        return 0;
    }
    uint16_t       end_index   = offset + size;
    const uint16_t StartIndex  = offset;
    const uint16_t TotalLength = get_cert_chain_length(slot_num);

    if (StartIndex >= TotalLength) {
        return 0;
    }
    if (end_index > TotalLength) {
        end_index = TotalLength;
    }
    std::memcpy(data, &cert_chain_buffer.data()[StartIndex], end_index - StartIndex);
    return end_index - StartIndex;
}

// read 5 cert to check the length
uint16_t get_cert_chain_length(uint8_t slot)
{
    uint32_t total_cert_chain_length = 0;
    if (!valid_slot(slot)) {
        nv::info("get certificate chain length at invalid slot %d", slot);
        return total_cert_chain_length;
    }

    total_cert_chain_length += get_l1_cert_len();
    total_cert_chain_length += get_l2_cert_len();
    total_cert_chain_length += get_l3_cert_len();
    total_cert_chain_length += get_l4_cert_len();
    total_cert_chain_length += get_l5_cert_len();

    total_cert_chain_length += sizeof(CertchainheaderT);
    if (total_cert_chain_length > MaxCertChainLength) {
        nv::info("invalid certificate chain length");
        return 0;
    }
    else {
        return (uint16_t)total_cert_chain_length;
    }
}

}  // namespace nv::spdm::cert
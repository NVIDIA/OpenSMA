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

#include "nv/spdm/task.h"

#include <chrono>
#include <utility>
#include "nv/flash/flash.h"
#include "nv/logger/log.h"
#include "nv/logger/common.h"
#include "nv/bootloader.h"
#include "nv/mctp/interface.h"
#include "nv/nv.h"
using namespace std::chrono_literals;
using namespace nv::ipc;
using namespace nv;

namespace nv::spdm {
namespace {

enum class SpdmLockFuseBlockCode : uint32_t
{
    LockSuccess     = 0,
    ReadFuseFail    = 1,
    ProgramFuseFail = 2,
    AlreadyLock     = 3
};

void lock_fuse_block()
{
    constexpr uint32_t EfuseLockFieldIndex = 0;
    uint32_t           current_fuse_data   = 0;
    // read current fuse lock configuration
    auto flash_status = nv::flash::Flash::read_efuse(EfuseLockFieldIndex, current_fuse_data);
    if (nv::flash::Status::Ok != flash_status) {
        nv::logger::error(nv::logger::Event::SpdmLockFuseBlock,
                          nv::logger::data_from_two_u32(
                              std::to_underlying(SpdmLockFuseBlockCode::ReadFuseFail),
                              std::to_underlying(flash_status)));
        return;
    }
    /**
     * @brief Lock Fuse Config
     *
     * - Byte_0
     *
     *   Reserved
     *
     * - Byte_1
     *
     *   OSCAA_KEY_LOCK[0]\(Bit_7) + PRINCE_CFG_LOCK[2:0] + BOOT_CFG_LOCK[2:0] + Reserved(Bit_0)
     *
     * - Byte_2
     *
     *   CUST_LOCK1[2:0]\(Bit_7) + CUST_LOCK0[2:0] + OSCAA_KEY_LOCK[2:1]\(Bit_0)
     *
     * - Byte_3
     *
     *   Reserved[1:0]\(Bit_7) + CUST_LOCK3[2:0] + CUST_LOCK2[2:0]\(Bit_0)
     *
     *
     * Item to be set
     *
     * - PRINCE_CFG_LOCK[0]
     *
     * - OSCAA_KEY_LOCK[0]
     *
     * - CUST_LOCK0[0]
     *
     * - CUST_LOCK1[0]
     *
     */
    constexpr uint32_t LockAllFuseConfig = 0x00249000;
    if ((current_fuse_data & LockAllFuseConfig) == LockAllFuseConfig) {  // do nothing
                                                                         // when fuse is
                                                                         // already
                                                                         // locked.
        nv::logger::info(nv::logger::Event::SpdmLockFuseBlock,
                         nv::logger::data_from_two_u32(
                             std::to_underlying(SpdmLockFuseBlockCode::AlreadyLock),
                             std::to_underlying(nv::flash::Status::Ok)));
    }
    else {  // lock fuse.
        flash_status = nv::flash::Flash::program_efuse(EfuseLockFieldIndex, LockAllFuseConfig);
        if (nv::flash::Status::Ok != flash_status) {
            nv::logger::error(nv::logger::Event::SpdmLockFuseBlock,
                              nv::logger::data_from_two_u32(
                                  std::to_underlying(SpdmLockFuseBlockCode::ProgramFuseFail),
                                  std::to_underlying(flash_status)));
        }
        else {
            nv::logger::info(nv::logger::Event::SpdmLockFuseBlock,
                             nv::logger::data_from_two_u32(
                                 std::to_underlying(SpdmLockFuseBlockCode::LockSuccess),
                                 std::to_underlying(flash_status)));
        }
    }
}
}  // namespace
std::array<pdk::spdm::platforms::res::ada::library::PlatformContext, SpdmResponderNumber>&
Task::get_context_instance()
{
    auto& task = nv::spdm::Task::get_task();
    return task.context_instance;
}

nv::mctp::Header& Task::get_last_header()
{
    auto& task = nv::spdm::Task::get_task();
    return task.last_request_header;
}

nv::mctp::PrivateHeader& Task::get_last_private_header()
{
    auto& task = nv::spdm::Task::get_task();
    return task.last_request_private_header;
}

measurement::MeasurementCacheT& Task::get_measurement_cache()
{
    auto& task = nv::spdm::Task::get_task();
    return task.measurement_cache;
}

nv::spdm::Task& Task::get_task()
{
    auto& task = nv::ipc::Supervisor::inst().task(nv::ipc::TaskId::Spdm);
    // NOLINTNEXTLINE(*-reinterpret-cast)
    auto& spdm_task = reinterpret_cast<nv::spdm::Task&>(task);
    return spdm_task;
}

void Task::make()
{
    NV_TASK_DATA static Task task;
    // last check, reserved 281 words for future usage
    constexpr auto StackSize = std::max(8224 + 512, int(configMINIMAL_STACK_SIZE));
    NV_STACK static sys::ipc::TaskStack<StackSize> stack;
    // NOLINTNEXTLINE(*-reinterpret-cast)
    const std::span<uint8_t> Priv(reinterpret_cast<uint8_t*>(&task), sizeof(Task));
    task.setup(stack.span(), Priv, Priority::Spdm, Task::entrypoint);
    measurement::spdm_init_all_measurement_cache();
}

void Task::entrypoint(void* params)
{
    NV_ASSERT(params != nullptr);
    using namespace std::chrono_literals;
    auto& task = *static_cast<Task*>(params);
    task.main();
}

// send message to the spdm library
nv::spdm::Status Task::spdm_tx(const nv::mctp::Packet& packet)
{
    nv::ipc::Queue& queue           = nv::ipc::Queue::make(nv::ipc::QueueId::SpdmRx);
    const size_t    ItemSize        = queue.item_size();
    const auto      PacketInterface = static_cast<mctp::Client>(packet.priv.packet_interface);

    // drop the packet from incorrect interface
    // support only usb / i2c(need to open configuration in config.h)
    if constexpr (nv::ipc::SpdmI2cResponder) {
        if (PacketInterface != mctp::Client::UsUsb && PacketInterface != mctp::Client::UsI2c) {
            return nv::spdm::Status::InvalidInterface;
        }
    }
    else {
        if (PacketInterface != mctp::Client::UsUsb) {
            return nv::spdm::Status::InvalidInterface;
        }
    }

    nv::ipc::Event&        event        = nv::ipc::Event::make(nv::ipc::EventId::SpdmTask);
    nv::ipc::Event::Status event_status = Event::Status::Ok;
    auto item         = nv::ipc::Queue::Item(std::bit_cast<uint8_t*>(&packet), ItemSize);
    auto queue_status = queue.send(item, 100ms);
    if (queue_status != nv::ipc::Queue::Status::Ok) {
        return nv::spdm::Status::QueueSendFail;
    }

    if (PacketInterface == mctp::Client::UsUsb) {
        event_status = event.set(spdm::Task::EventBits::UsbRxBit, false);
    }

    if (PacketInterface == mctp::Client::UsI2c) {
        event_status = event.set(spdm::Task::EventBits::I2cRxBit, false);
    }

    if (event_status != nv::ipc::Event::Status::Ok) {
        return nv::spdm::Status::EventSetFail;
    }
    return nv::spdm::Status::Ok;
}

Task::Task()
: ipc::Task(ipc::TaskId::Spdm, "SPDM")
, _event(ipc::Event::make(ipc::EventId::SpdmTask))
, _tx(ipc::Queue::make(ipc::QueueId::MctpSpdmRequest))
, _rx(ipc::Queue::make(ipc::QueueId::SpdmRx))

{}

[[noreturn]] void Task::main()
{
    nv::bootloader::Driver::set_task_booted(nv::ipc::BootedEventBits::Spdm);
    auto event = nv::ipc::Event::make(nv::ipc::EventId::SpdmTask);
    event.wait(EventBits::CertReadyBit, true, false);
    if (event.set(nv::spdm::Task::AuthenticateTaskIdle) != nv::ipc::Event::Status::Ok) {
        nv::info("spdm task start set AuthenticateTaskIdle fail\n");
    }

    // start to generate the cert
    // use dummy certificate
    if constexpr (nv::ipc::SpdmDummyCertificates == true) {
        nv::info("use dummy certificate\n");
    }
    else {
        if (nv::spdm::cert::verify_l2_l3_cert() and nv::spdm::cert::generate_l4_cert()
            and nv::spdm::cert::generate_l5_cert()) {
            lock_fuse_block();
        }
    }

    // update dbg nonce if necessary
    nv::spdm::measurement::update_dbg_token_nonce_meas();

    // coverity[no_escape] - no escape is expected
    while (true) {
        // start the spdm responsder main function
        pdk::spdm::app::res::ada::library::spdm_responder_main();
    }
}
}  // namespace nv::spdm
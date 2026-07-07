/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES.
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
#include "nv/spdm/spdm_crypto_helper.h"

#include <array>
#include <bit>
#include <span>
#include <type_traits>
#include <variant>

#include "nv/crypto/key_clear_guard.h"
#include "nv/ipc/event.h"
#include "nv/ipc/queue.h"
#include "nv/ipc/supervisor.h"
#include "nv/spdm/task.h"
#include "sys/crypto/crypto.h"

namespace nv::spdm::crypto {
namespace {

constexpr std::chrono::milliseconds PrimitiveQueueTimeout{100};

bool is_valid_requester_core(nv::ipc::CoreId core_id)
{
    return core_id == nv::ipc::CoreId::Core0 || core_id == nv::ipc::CoreId::Core1;
}

bool is_valid_queue(nv::ipc::QueueId queue_id, size_t item_size)
{
    return queue_id != nv::ipc::QueueId::End
        && nv::ipc::Queue::make(queue_id).item_size() == item_size;
}

nv::ipc::TaskId get_request_task_id(const CryptoPrimitiveRequestParameters& request)
{
    return std::visit([](const auto& req) { return req.request_task_id; }, request);
}

nv::ipc::CoreId get_request_core_id(const CryptoPrimitiveRequestParameters& request)
{
    return std::visit([](const auto& req) { return req.request_core_id; }, request);
}

uint32_t get_request_id(const CryptoPrimitiveRequestParameters& request)
{
    return std::visit([](const auto& req) { return req.request_id; }, request);
}

uint32_t get_request_id_for_primitive(const CryptoPrimitiveRequestParameters& request)
{
    return std::visit(
        [](const auto& req) -> uint32_t {
            using Request = std::decay_t<decltype(req)>;
            if constexpr (std::is_same_v<Request, TrngGenerateRequestParameter>) {
                return static_cast<uint32_t>(CryptoPrimitiveRequestId::TrngGenerate);
            }
            else if constexpr (std::is_same_v<Request, PufWrapRequestParameter>) {
                return static_cast<uint32_t>(CryptoPrimitiveRequestId::PufWrap);
            }
            else if constexpr (std::is_same_v<Request, PufUnwrapRequestParameter>) {
                return static_cast<uint32_t>(CryptoPrimitiveRequestId::PufUnwrap);
            }
            else {
                static_assert(std::is_same_v<Request, Aes256GcmEncryptRequestParameter>);
                return static_cast<uint32_t>(CryptoPrimitiveRequestId::Aes256GcmEncrypt);
            }
        },
        request);
}

void set_request_metadata(CryptoPrimitiveRequestParameters& request,
                          nv::ipc::TaskId                   task_id,
                          nv::ipc::CoreId                   core_id,
                          uint32_t                          request_id)
{
    std::visit(
        [task_id, core_id, request_id](auto& req) {
            req.request_task_id = task_id;
            req.request_core_id = core_id;
            req.request_id      = request_id;
        },
        request);
}

nv::spdm::Status send_primitive_response(const CryptoPrimitiveResponseParameters& response,
                                         std::chrono::milliseconds                timeout,
                                         nv::ipc::CoreId                          dest_core)
{
    const auto response_queue_id = get_primitive_queue_id(CryptoPrimitiveQueueType::Response);
    if (!is_valid_queue(response_queue_id, sizeof(response))) {
        return nv::spdm::Status::InvalidInterface;
    }

    const auto& response_view = *std::bit_cast<const std::array<uint8_t, sizeof(response)>*>(
        &response);
    const nv::ipc::Queue::ConstItem response_item(response_view.data(), response_view.size());
    auto                            send_status = nv::ipc::Queue::make(response_queue_id)
                           .send(response_item, timeout, dest_core);
    if (send_status != nv::ipc::Queue::Status::Ok) {
        return nv::spdm::Status::QueueSendFail;
    }
    return nv::spdm::Status::Ok;
}

bool response_matches_request(const CryptoPrimitiveResponseParameters& response,
                              nv::ipc::TaskId                          task_id,
                              uint32_t                                 request_id)
{
    return response.request_task_id == task_id && response.request_id == request_id;
}

void drain_response_queue(nv::ipc::Queue& response_queue)
{
    std::array<uint8_t, sizeof(CryptoPrimitiveResponseParameters)> stale_response{};
    const nv::crypto::KeyClearGuard stale_response_guard{std::span<uint8_t>(stale_response)};
    nv::ipc::Queue::Item            stale_item(stale_response.data(), stale_response.size());
    while (response_queue.recv(stale_item, std::chrono::milliseconds{0})
           == nv::ipc::Queue::Status::Ok) {}
}

bool receive_matching_response(nv::ipc::Queue&                    response_queue,
                               CryptoPrimitiveResponseParameters& response,
                               nv::ipc::TaskId                    task_id,
                               uint32_t                           request_id,
                               std::chrono::milliseconds          timeout)
{
    auto& response_view = *std::bit_cast<std::array<uint8_t, sizeof(response)>*>(&response);
    nv::ipc::Queue::Item response_item(response_view.data(), response_view.size());
    for (size_t attempt = 0; attempt <= response_queue.max_items(); ++attempt) {
        if (response_queue.recv(response_item, timeout) != nv::ipc::Queue::Status::Ok) {
            return false;
        }
        if (response_matches_request(response, task_id, request_id)) {
            return true;
        }
    }
    return false;
}

}  // namespace

__attribute__((weak)) bool is_primitive_request_queue_enabled()
{
    return false;
}

__attribute__((weak)) nv::ipc::QueueId
get_primitive_queue_id([[maybe_unused]] CryptoPrimitiveQueueType queue_type)
{
    return nv::ipc::QueueId::End;
}

nv::spdm::Status submit_primitive_request(CryptoPrimitiveRequestParameters&  req,
                                          CryptoPrimitiveResponseParameters& response,
                                          std::chrono::milliseconds          timeout)
{
    if (!is_primitive_request_queue_enabled()) {
        return nv::spdm::Status::NotSupported;
    }

    const auto request_queue_id  = get_primitive_queue_id(CryptoPrimitiveQueueType::Request);
    const auto response_queue_id = get_primitive_queue_id(CryptoPrimitiveQueueType::Response);
    if (!is_valid_queue(request_queue_id, sizeof(req))
        || !is_valid_queue(response_queue_id, sizeof(response))) {
        return nv::spdm::Status::InvalidInterface;
    }

    const auto requester_task_id = ipc::Supervisor::inst().current_task_id();
    if (requester_task_id == ipc::TaskId::Spdm) {
        return nv::spdm::Status::InvalidInterface;
    }

    const auto requester_core_id = nv::ipc::get_current_core();
    if (!is_valid_requester_core(requester_core_id)) {
        return nv::spdm::Status::InvalidInterface;
    }

    auto spdm_task_event = ipc::Event::make(ipc::EventId::SpdmTask);

    const auto idle_bits = spdm_task_event.wait(
        nv::spdm::Task::AuthenticateTaskIdle, true, false, timeout);
    if (!idle_bits || ((*idle_bits & nv::spdm::Task::AuthenticateTaskIdle) == 0U)) {
        return nv::spdm::Status::Busy;
    }

    auto release_spdm_idle = [&spdm_task_event]() {
        return spdm_task_event.set(nv::spdm::Task::EventBits::AuthenticateTaskIdle,
                                   nv::ipc::get_current_core());
    };

    auto& response_queue = nv::ipc::Queue::make(response_queue_id);
    drain_response_queue(response_queue);

    const uint32_t request_id = get_request_id_for_primitive(req);
    set_request_metadata(req, requester_task_id, requester_core_id, request_id);

    auto&                request_view = *std::bit_cast<std::array<uint8_t, sizeof(req)>*>(&req);
    nv::ipc::Queue::Item req_item(request_view.data(), request_view.size());
    auto&                request_queue = nv::ipc::Queue::make(request_queue_id);
    if (request_queue.send(req_item, timeout) != nv::ipc::Queue::Status::Ok) {
        (void)release_spdm_idle();
        return nv::spdm::Status::QueueSendFail;
    }

    if (spdm_task_event.set(nv::spdm::Task::EventBits::CryptoPrimitiveRequestBit,
                            nv::ipc::CoreId::Core0)
        != nv::ipc::Event::Status::Ok) {
        (void)request_queue.recv(req_item, std::chrono::milliseconds{0});
        (void)release_spdm_idle();
        return nv::spdm::Status::EventSetFail;
    }

    if (!receive_matching_response(
            response_queue, response, requester_task_id, request_id, timeout)) {
        (void)release_spdm_idle();
        return nv::spdm::Status::Timeout;
    }

    const auto release_status = release_spdm_idle();
    return release_status == nv::ipc::Event::Status::Ok ? nv::spdm::Status::Ok
                                                        : nv::spdm::Status::EventSetFail;
}

void process_primitive_request_queue()
{
    if (!is_primitive_request_queue_enabled()) {
        return;
    }

    const auto request_queue_id = get_primitive_queue_id(CryptoPrimitiveQueueType::Request);
    if (!is_valid_queue(request_queue_id, sizeof(CryptoPrimitiveRequestParameters))) {
        return;
    }

    nv::ipc::Queue& queue = nv::ipc::Queue::make(request_queue_id);

    CryptoPrimitiveRequestParameters request_parameters{};
    auto&                            request_parameters_view = *std::bit_cast<
                                   std::array<uint8_t, sizeof(decltype(request_parameters))>*>(&request_parameters);
    nv::ipc::Queue::Item item(request_parameters_view.data(), request_parameters_view.size());

    const auto status = queue.recv(item, PrimitiveQueueTimeout);
    if (status == nv::ipc::Queue::Status::Ok) {
        const auto requester_task_id = get_request_task_id(request_parameters);
        const auto requester_core_id = get_request_core_id(request_parameters);
        CryptoPrimitiveResponseParameters response_parameters{
            .request_task_id = requester_task_id,
            .request_id      = get_request_id(request_parameters),
            .status          = CryptoStatus::FailUnknown,
            .output          = {},
        };
        const nv::crypto::KeyClearGuard response_guard{
            std::span<uint8_t>(response_parameters.output)};
        bool response_ready = false;

        if (!is_valid_requester_core(requester_core_id)) {
            response_ready = false;
        }
        else if (std::get_if<TrngGenerateRequestParameter>(&request_parameters)) {
            response_parameters.status = sys::crypto::trng_generate(
                std::span<uint8_t>(response_parameters.output.data(), UdsKeyBytes));
            response_ready = true;
        }
        else if (auto* request = std::get_if<PufWrapRequestParameter>(&request_parameters)) {
            const nv::crypto::KeyClearGuard request_guard{std::span<uint8_t>(request->key)};
            response_parameters.status = sys::crypto::puf_wrap(
                std::span<const uint8_t>(request->key),
                std::span<uint8_t>(response_parameters.output));
            response_ready = true;
        }
        else if (auto* request = std::get_if<PufUnwrapRequestParameter>(&request_parameters)) {
            const nv::crypto::KeyClearGuard request_guard{std::span<uint8_t>(request->wrapped)};
            response_parameters.status = sys::crypto::puf_unwrap(
                std::span<const uint8_t>(request->wrapped),
                std::span<uint8_t>(response_parameters.output.data(), UdsKeyBytes));
            response_ready = true;
        }
        else if (auto* request = std::get_if<Aes256GcmEncryptRequestParameter>(
                     &request_parameters)) {
            const nv::crypto::KeyClearGuard request_key_guard{std::span<uint8_t>(request->key)};
            const nv::crypto::KeyClearGuard plaintext_guard{
                std::span<uint8_t>(request->plaintext)};
            if (request->aad_size > request->aad.size()
                || request->plaintext_size > request->plaintext.size()
                || request->plaintext_size + nv::crypto::AesGcmTagBytes
                       > response_parameters.output.size()) {
                response_parameters.status = CryptoStatus::FailAesGcmInvalidInput;
            }
            else {
                response_parameters.status = sys::crypto::aes_256_gcm_encrypt(
                    request->key,
                    request->iv,
                    std::span<const uint8_t>(request->aad.data(), request->aad_size),
                    std::span<const uint8_t>(request->plaintext.data(),
                                             request->plaintext_size),
                    std::span<uint8_t>(response_parameters.output.data(),
                                       request->plaintext_size),
                    std::span<uint8_t, nv::crypto::AesGcmTagBytes>(
                        response_parameters.output.data() + request->plaintext_size,
                        nv::crypto::AesGcmTagBytes));
            }
            response_ready = true;
        }

        if (response_ready) {
            (void)send_primitive_response(
                response_parameters, PrimitiveQueueTimeout, requester_core_id);
        }
    }

    if (queue.size() != 0) {
        (void)nv::ipc::Event::make(nv::ipc::EventId::SpdmTask)
            .set(nv::spdm::Task::EventBits::CryptoPrimitiveRequestBit);
    }
}

}  // namespace nv::spdm::crypto

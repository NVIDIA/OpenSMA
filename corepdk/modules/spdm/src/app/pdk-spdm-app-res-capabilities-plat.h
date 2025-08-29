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
#pragma once
#include <stdint.h>

#include "pdk-spdm-app-res-capabilities.h"
namespace pdk::spdm::platforms::res::capabilities {
/**
 * @brief Get the ct exponent value for responder.
 *
 * The equation for CT shall be 2^CTExponent microseconds (µs).
 * For example, if CTExponent is 10 , CT is 2^10 =1024 µs.
 *
 * @return uint8_t
 */
uint8_t get_ct_exponent();
/**
 * @brief Get the cache capability value for responder.
 *
 * If set, the Responder supports the ability to cache the Negotiated
 * State across a reset. This allows the Requester to skip reissuing the
 * GET_VERSION, GET_CAPABILITIES and NEGOTIATE_ALGORITHMS requests after a
 * reset. The Responder shall cache the selected cryptographic algorithms as one
 * of the parameters of the Negotiated State. If the Requester chooses to skip
 * issuing these requests after the reset, the Requester shall also cache the
 * same selected cryptographic algorithms.
 *
 * @return pdk::spdm::app::res::capabilities::CacheCap
 */
pdk::spdm::app::res::capabilities::CacheCap get_flag_cache();
/**
 * @brief Get the certificate capability value for responder.
 *
 * If set, Responder supports DIGESTS and CERTIFICATE response messages.
 *
 * @return pdk::spdm::app::res::capabilities::CertCap
 */
pdk::spdm::app::res::capabilities::CertCap get_flag_cert();
/**
 * @brief Get the challenge capability value for responder.
 *
 * If set, Responder supports CHALLENGE_AUTH response message.
 *
 * @return pdk::spdm::app::res::capabilities::ChalCap
 */
pdk::spdm::app::res::capabilities::ChalCap get_flag_chal();
/**
 * @brief Get the measurement capability value for responder.
 *
 * MEASUREMENT response capabilities of the Responder.
 *
 * - 00b
 *   The Responder does not support MEASUREMENTS response capabilities.
 *
 * - 01b
 *   The Responder supports MEASUREMENTS response but cannot perform signature
 * generation.
 *
 * - 10b
 *    The Responder supports MEASUREMENTS response and can generate signatures.
 *
 * - 11b
 *    Reserved.
 *
 * @return pdk::spdm::app::res::capabilities::MeasCap
 */
pdk::spdm::app::res::capabilities::MeasCap get_flag_meas();
/**
 * @brief Get the measurement with fresh capability value for responder.
 *
 * Details
 *
 * - If not set
 *   As part of MEASUREMENTS response message, the Responder may return
 * MEASUREMENTS that were computed during the last Responder’s reset.
 *
 * - If set
 *   The Responder supports recomputing all MEASUREMENTS without requiring a
 * reset or restart, and shall always return fresh MEASUREMENTS as part of
 * MEASUREMENTS response message.
 *
 * @return pdk::spdm::app::res::capabilities::MeasFreshCap
 */
pdk::spdm::app::res::capabilities::MeasFreshCap get_flag_meas_fresh();
/**
 * @brief Get the encrypt capability value for responder.
 *
 * If set, Responder supports message encryption. If set, one or more of
 * PSK_CAP or KEY_EX_CAP fields shall be specified accordingly to indicate
 * support.
 *
 * @return pdk::spdm::app::res::capabilities::EncryptCap
 */
pdk::spdm::app::res::capabilities::EncryptCap get_flag_encrypt();
/**
 * @brief Get the mac capability value for responder.
 *
 * If set, Responder supports message authentication. If set, one or more
 * of PSK_CAP or KEY_EX_CAP fields shall be specified accordingly to indicate
 * support.
 *
 * @return pdk::spdm::app::res::capabilities::MacCap
 */
pdk::spdm::app::res::capabilities::MacCap get_flag_mac();
/**
 * @brief Get the mutal authenticate capability value for responder.
 *
 * If set, Responder supports mutual authentication.
 *
 * @return pdk::spdm::app::res::capabilities::MutAuthCap
 */
pdk::spdm::app::res::capabilities::MutAuthCap get_flag_mut_auth();
/**
 * @brief Get the key exchange capability value for responder.
 *
 * If set, Responder supports KEY_EXCHANGE messages. If set, one or more
 * of ENCRYPT_CAP and MAC_CAP shall be set.
 *
 * @return pdk::spdm::app::res::capabilities::KeyExCap
 */
pdk::spdm::app::res::capabilities::KeyExCap get_flag_key_ex();
/**
 * @brief Get the pre-shared key capability value for responder.
 *
 * Pre-Shared Key capabilities of the Responder.
 *
 * - 00b
 *   Responder does not support Pre-Shared Key capabilities.
 *
 * - 01b
 *   Responder supports Pre-Shared Key but does not provide ResponderContext for
 * session key derivation.
 *
 * - 10b
 *   Responder supports Pre-Shared Key and provides ResponderContext for session
 * key derivation.
 *
 * - 11b
 *   Reserved.
 *
 * If supported, one or more of ENCRYPT_CAP and MAC_CAP shall be set
 *
 * @return pdk::spdm::app::res::capabilities::PskCap
 */
pdk::spdm::app::res::capabilities::PskCap get_flag_psk();
/**
 * @brief Get the encap capability value for responder.
 *
 * If set, Responder supports GET_ENCAPSULATED_REQUEST,
 * ENCAPSULATED_REQUEST, DELIVER_ENCAPSULATED_RESPONSE, and
 * ENCAPSULATED_RESPONSE_ACK messages. If mutual authentication is supported,
 * this field shall be set.
 *
 * @return pdk::spdm::app::res::capabilities::EncapCap
 */
pdk::spdm::app::res::capabilities::EncapCap get_flag_encap();
/**
 * @brief Get the heartbeat capability value for responder.
 *
 * If set, Responder supports HEARTBEAT messages.
 *
 * @return pdk::spdm::app::res::capabilities::HbeatCap
 */
pdk::spdm::app::res::capabilities::HbeatCap get_flag_hbeat();
/**
 * @brief Get the key update capability value for responder.
 *
 * If set, Responder supports KEY_UPDATE messages.
 *
 * @return pdk::spdm::app::res::capabilities::KeyUpdCap
 */
pdk::spdm::app::res::capabilities::KeyUpdCap get_flag_key_upd();
/**
 * @brief Get the hand shake in the clear capability value for responder.
 *
 * If set, the Responder can only send and receive messages without
 * encryption and message authentication during the Session Handshake Phase. If
 * set, KEY_EX_CAP shall also be set. Setting this bit leads to changes in the
 * contents of certain SPDM messages, discussed in other parts of this
 * specification. If the Responder does not support encryption and message
 * authentication, then this bit shall be zero.
 *
 * @return pdk::spdm::app::res::capabilities::HandshakeInTheClearCap
 */
pdk::spdm::app::res::capabilities::HandshakeInTheClearCap get_flag_handshake_in_the_clear();
/**
 * @brief Get the public key identifying capability value for responder.
 *
 * If set, the public key of the Responder was provisioned to the
 * Requester. The transport layer is responsible for identifying the Requester.
 * In this case, CERT_CAP of the Responder shall be 0.
 *
 * @return pdk::spdm::app::res::capabilities::PubKeyIdCap
 */
pdk::spdm::app::res::capabilities::PubKeyIdCap get_flag_pub_key_id();

}  // namespace pdk::spdm::platforms::res::capabilities

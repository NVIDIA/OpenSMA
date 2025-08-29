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
#include "pdk-spdm-app-res-ada-library.h"

#include "pdk-spdm-app-res-ada-library-plat.h"
namespace pdk::spdm::app::res::ada::library {

#ifdef __cplusplus
extern "C" {
#endif
/*
 *  spdm_platform_initialize()
 *
 *  This function allocates the platform data structure for holding any
 *  platform specific data for the instance of the responder.  Currently usage,
 * we have two statically allocated instances.  The responder will pass in the
 * pointer associated with the instance for the other platform APIs.
 *
 *  [out] instance - return the pointer to the platform data structure for
 *                   the instance of the responder
 *
 *  Returns: void
 */
void spdm_platform_context_initialize_c(
    pdk::spdm::platforms::res::ada::library::PlatformContext** instance)
{
    pdk::spdm::platforms::res::ada::library::spdm_platform_context_initialize(instance);
    return;
}

/*
 * has_data_for_spdm_responsder()
 *
 * This function returns true when there is data for the responder to consume.
 * This function will be called by spdm library and it will return until there
 * is a data.
 *
 * Returns: 0 for error.
 */
int has_data_for_spdm_responsder_c(void)
{
    using namespace pdk::spdm::platforms::res::ada::library;

    return static_cast<std::underlying_type_t<ResponsderNumber>>(
        has_data_for_spdm_responsder());
}

/*
 * get_data_from_spdm_responsder()
 *
 * This function is called by the SPDM responder to send data to the transport
 * layer.  This is primarily for the SPDM responder to send responses to the
 * requester.
 *
 * buffer is a pointer to the buffer containing bytes to send
 * num_bytes is the number of bytes to send
 *
 * Returns: void
 */
void get_data_from_spdm_responsder_c(const void* const buffer, const size_t num_bytes)
{
    // check if the input parameter is not valid
    if (buffer == nullptr) {
        return;
    }

    pdk::spdm::platforms::res::ada::library::get_data_from_spdm_responsder(
        std::span<const uint8_t>(static_cast<const uint8_t*>(buffer), num_bytes));

    return;
}

/*
 * send_data_to_spdm_responsder()
 *
 * This function is called by the SPDM responder to read data.
 *
 * buffer is a pointer to the buffer to copy the data into
 * *num_bytes is the number of bytes in the buffer, routine returns number of
 * bytes copied into buffer
 *
 * Returns: void
 */
void send_data_to_spdm_responsder_c(void* const buffer, size_t* const num_bytes)
{
    if ((buffer == nullptr) || (num_bytes == nullptr)) {
        return;
    }

    *num_bytes = pdk::spdm::platforms::res::ada::library::send_data_to_spdm_responsder(
        std::span<uint8_t>(static_cast<uint8_t*>(buffer), *num_bytes));

    return;
}

#ifdef __cplusplus
}
#endif

}  // namespace pdk::spdm::app::res::ada::library
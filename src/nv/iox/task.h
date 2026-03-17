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

#include "nv/ipc/task.h"
#include "nv/ipc/event.h"
#include "nv/ipc/queue.h"

#include "nv/iox/iox.h"
#include "nv/i2c/common.h"
#include "nv/mctp/nsm_type_5.h"

#include NV_IPC_CONFIG_H

namespace nv::iox {

class Task : public ipc::Task
{
public:
    static constexpr size_t  DataSize    = 76;
    static constexpr size_t  IoxNum      = nv::ipc::IoxNum;
    static constexpr uint8_t IoxBaseAddr = nv::ipc::IoxI2cBaseAddr;

    enum class RequestType : uint16_t
    {
        I2cRequest = 6,
        VrGpioRequest,
        GpioSpoofingUpdate,
    };

    struct [[gnu::packed]] Request
    {
        RequestType type;
        uint16_t    additional_info;
        uint8_t     data[DataSize];
    };

    struct [[gnu::packed]] VrGpioRequest
    {
        uint8_t ioxAddr;
        uint8_t ioxOper;
        uint8_t pinSize;
        uint8_t pinVals[DataSize - sizeof(ioxAddr) - sizeof(ioxOper) - sizeof(pinSize)];
    };

    struct [[gnu::packed]] GpioSpoofingUpdate
    {
        bool    spoofingActive;
        uint8_t numEntries;
        uint8_t reserved[2];
        struct [[gnu::packed]] Entry
        {
            uint16_t gpioIndex;
            uint8_t  activated;
            uint8_t  reserved;
        } entries[nv::mctp::MaxGPIOSpoofingEntries];
    };

    using Buffer = std::array<uint8_t, 2>;

    Task();

    [[noreturn]] void main();

    static void make();
    static void entrypoint(void* params);
    // I2C request from USB
    static bool send_i2c_request(ipchandler::Id    src_id,
                                 uint8_t           address,
                                 ipchandler::Id    i2c_ipchandler_id,
                                 uint8_t           write_length,
                                 uint16_t          read_length,
                                 ipc::Queue::Item& item);
    // Vritual GPIO request from MCU tasks (leak detection, etc.)
    static bool send_vrgpio_request(uint8_t                  address,
                                    Operation                operation,
                                    std::span<const uint8_t> pins,
                                    std::span<const uint8_t> vals);

    // GPIO spoofing state update from MCTP task
    static bool send_gpio_spoofing_update(bool                           spoofingActive,
                                          nv::mctp::GPIOSpoofingPayload& updateData);

private:
    std::array<Iox, IoxNum> ioexp;
    nv::i2c::I2cHidBuffer   read_buffer{};
    void                    handle_i2c_request(std::span<uint8_t> buffer);
    void                    handle_vrgpio_request(std::span<uint8_t> buffer);
    void                    handle_gpio_spoofing_update(std::span<uint8_t> buffer);
};

}  // namespace nv::iox
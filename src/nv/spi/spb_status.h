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
#pragma once

namespace nv::spi {

enum class SpbStatus : uint32_t
{
    Begin = 0,
    Ok    = Begin,

    // Packet rewrite error
    SendInvalidMctpPkt,
    RecvInvalidSpiPkt,
    // Spi event error
    SetRxEventFail,
    EventClearFail,
    InterruptHandleError,
    PollCmdTimeout,
    MbxEventTimeout,
    PostedReadError,
    PostedWriteError,
    WaitAckError,
    WaitTxAck0Error,
    WaitTxAck1Error,
    WaitRxAckError,
    WaitResetAckError,
    WaitLenError,
    WriteLenError,
    GetEcMsgAvailNotAck,
    GetEcMsgAvailNotLen,
    GetAckNotEcMsgAvail,
    // mailbox_write error
    RequestWriteError,
    ReadyToReadError,
    FinishedReadError,
    RequestResetError,
    MbxReadFailInTask,
    // Likely not happen
    InvalidParameter,

    End
};

}  // namespace nv::spi

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

// MCU Power Controller Framework

namespace nv::mpf {

// Conceptual base is the Function Block (FB) which is a class that represents
// a function with N inputs and M outputs such that N + M >= 1. FBs may contain
// state. All derived classes must implement the below functionality:
class FuncBlock
{
public:
    /*

    // The ports structure contains the FB's inputs and outputs. Inputs are
    // immutable references and outputs are mutable references (see type
    // aliases in the `port` namespace).
    struct Ports {
        // ...
    };

    // Runtime configuration structure, define if the FB needs runtime
    // configurability
    struct RtCfg {
        // ...
    };

    // (If RtCfg is defined)
    FuncBlock(const RtCfg& cfg) : _cfg(cfg) {}
    FuncBlock(RtCfg&&) = delete;

    // Mark `static` or `const` if possible
    // Pass `ports` via immutable reference if it won't be passed using
    // registers
    void evaluate(Ports ports) {
        // ...
    }

    */

protected:
    /*

    // (If RtCfg is defined)
    const RtCfg& _cfg;

    // Optional state
    // ...

    */
};

// Type aliases for use in a FB's `Ports` structure
namespace port {

template<typename T>
using In = const T&;

template<typename T>
using Out = T&;

}  // namespace port

}  // namespace nv::mpf

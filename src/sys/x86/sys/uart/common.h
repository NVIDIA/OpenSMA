#pragma once

#include <cstdint>

namespace sys::uart {

enum class Instance : uint8_t
{
    Begin,
    _0 = Begin,
    _1,
    _2,
    _3,
    _4,
    _5,
    _6,
    _7,
    _8,
    _9,
    End,
};

enum class Status : uint32_t
{
    Ok,
    NotInited,
    InvalidInstance,
    InvalidParam,
    LpuartEdmaTxBusy,
    LpuartEdmaRxBusy,
};

enum class State : uint8_t
{
    Begin,
    Reset,
    Idle,
    Running,
    Error,
    End
};

}  // namespace sys::uart

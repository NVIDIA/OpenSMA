#pragma once

#include <span>
#include <array>
#include <cstdint>

#include "sys/uart/common.h"
#include "nv/vruart/common.h"

namespace sys::uart {

constexpr static size_t edmaXferBufSize = 512U;

class Bridge
{
public:
    Bridge() = default;

    Status init(nv::vruart::Instance      uartInstance,
                const nv::vruart::Signal& tx,
                const nv::vruart::Signal& rx,
                nv::vruart::Baudrate      baudrate,
                nv::vruart::EdmaInst      edmaInstance,
                nv::vruart::EdmaChn       edmaTxChn,
                nv::vruart::EdmaChn       edmaRxChn);
    Status tx(std::span<uint8_t> data);

    bool ready() const;
    bool txongoing() const { return false; }
};

}  // namespace sys::uart

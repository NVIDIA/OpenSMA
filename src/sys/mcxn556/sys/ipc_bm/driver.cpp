/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Bare-Metal IPC Driver Implementation for Core1
 *
 * Compatible with Core0's ipc::task::Task using MCMGR and FreeRTOS MessageBuffer
 */

#include "driver.h"

#include "nv/ipc/c2c_stream_buffer.h"
#include "nv/ipc/bm_core1_cfg_data.h"

extern "C" {
#include <string.h>  // Use C header directly for bare-metal (avoid cstring override issues)
#include "mcmgr.h"
#include "fsl_common.h"
}

namespace nv::ipc_bm {

// Alias the shared struct for local use
using C2CStreamBufferCtrl = nv::ipc::C2CStreamBufferCtrl;

/*******************************************************************************
 * Private Variables
 ******************************************************************************/
namespace {
volatile bool g_initialized   = false;
volatile bool g_dataAvailable = false;  // Set when Core0 sends data
uint32_t      g_sharedMemAddr = 0;

// Pointers to the StreamBuffer control structures
C2CStreamBufferCtrl* g_rxCtrl = nullptr;  // Core0ToCore1 (we read)
C2CStreamBufferCtrl* g_txCtrl = nullptr;  // Core1ToCore0 (we write)

// Pointer to actual data buffers (from buffer_ptr field)
uint8_t* g_rxDataBuffer = nullptr;
uint8_t* g_txDataBuffer = nullptr;

nv::ipc::Core1CfgData* g_core1CfgData = nullptr;

// Read-only block for Core1 config data (set from startup info; 0 if unused)
static uint32_t g_core1CfgDataAddr = 0;

// Timeout for waiting startup data (in loop iterations)
constexpr uint32_t StartupTimeoutLoops = 1000000U;

// Command codes (must match nv::ipc::task::CmdCode)
constexpr uint16_t kCmdInterCoreSendWriteDone = 0;
constexpr uint16_t kCmdCore1Ready             = 1;
constexpr uint16_t kCmdInterCoreAcmTxDone     = 2;

}  // namespace

/*******************************************************************************
 * MessageBuffer Helper Functions
 ******************************************************************************/
namespace {

/*******************************************************************************
 * Optimized Ring Buffer Copy Functions
 *
 * These functions use memcpy for block transfers instead of byte-by-byte copy.
 * For a 512-byte MCTP packet, this reduces loop iterations from 512 to at most 2.
 ******************************************************************************/

// Fast copy from ring buffer to linear buffer
// Handles wrap-around with at most 2 memcpy calls
void ringBufCopyOut(
    uint8_t* dst, const uint8_t* ringBuf, size_t start, size_t len, size_t ringSize)
{
    if (len == 0) {
        return;
    }

    size_t firstPart = ringSize - start;

    if (firstPart >= len) {
        // No wrap - single memcpy (common case)
        memcpy(dst, ringBuf + start, len);
    }
    else {
        // Wrap around - two memcpy calls
        memcpy(dst, ringBuf + start, firstPart);
        memcpy(dst + firstPart, ringBuf, len - firstPart);
    }
}

void ringBufCopyIn(
    uint8_t* ringBuf, size_t start, const uint8_t* src, size_t len, size_t ringSize)
{
    if (len == 0) {
        return;
    }

    size_t firstPart = ringSize - start;

    if (firstPart >= len) {
        // No wrap - single memcpy (common case)
        memcpy(ringBuf + start, src, len);
    }
    else {
        // Wrap around - two memcpy calls
        memcpy(ringBuf + start, src, firstPart);
        memcpy(ringBuf, src + firstPart, len - firstPart);
    }
}

void ringBufMemset(uint8_t* ringBuf, size_t start, uint8_t value, size_t len, size_t ringSize)
{
    if (len == 0) {
        return;
    }

    size_t firstPart = ringSize - start;

    if (firstPart >= len) {
        memset(ringBuf + start, value, len);
    }
    else {
        memset(ringBuf + start, value, firstPart);
        memset(ringBuf, value, len - firstPart);
    }
}

// Get number of bytes available to read from a MessageBuffer
size_t msgBufBytesAvailable(C2CStreamBufferCtrl* ctrl)
{
    // Memory barrier to ensure we read the latest values from other core
    __DSB();

    size_t head   = ctrl->head;
    size_t tail   = ctrl->tail;
    size_t length = ctrl->length;

    if (head >= tail) {
        return head - tail;
    }
    else {
        return length - tail + head;
    }
}

// Get number of bytes available for writing
size_t msgBufSpaceAvailable(C2CStreamBufferCtrl* ctrl)
{
    // Memory barrier to ensure we read the latest values from other core
    __DSB();

    size_t head   = ctrl->head;
    size_t tail   = ctrl->tail;
    size_t length = ctrl->length;

    size_t used;
    if (head >= tail) {
        used = head - tail;
    }
    else {
        used = length - tail + head;
    }

    // MessageBuffer reserves 1 byte to distinguish full from empty
    return (length - 1) - used;
}

// Read a message from MessageBuffer (includes size prefix)
// Returns number of bytes read (excluding the size prefix)
//
// NOTE: Core0's StreamBuffer::send() passes ALIGNED size to xMessageBufferSend.
// The length prefix in the buffer IS the aligned size (rounded up to 4 bytes).
// We return the aligned size, caller should be aware that 0-3 bytes of padding
// may be included at the end.
//
// For QueueRequest messages, the actual data length is in QueueRequest.length field.
size_t
msgBufReceive(C2CStreamBufferCtrl* ctrl, uint8_t* dataBuffer, uint8_t* outBuffer, size_t maxLen)
{
    size_t available = msgBufBytesAvailable(ctrl);
    if (available < sizeof(size_t)) {
        return 0;  // No complete message
    }

    size_t tail   = ctrl->tail;
    size_t length = ctrl->length;

    // Read message size (size_t at tail position)
    // This is the ALIGNED size written by Core0
    size_t alignedMsgSize;
    if (tail + sizeof(size_t) <= length) {
        // Common case: no wrap - direct aligned read
        alignedMsgSize = *reinterpret_cast<const size_t*>(dataBuffer + tail);
    }
    else {
        // Rare case: size_t wraps around - byte-by-byte read
        alignedMsgSize = 0;
        for (size_t i = 0; i < sizeof(size_t); i++) {
            size_t idx                                     = (tail + i) % length;
            reinterpret_cast<uint8_t*>(&alignedMsgSize)[i] = dataBuffer[idx];
        }
    }

    // Sanity check: reject obviously corrupted sizes.
    // Must CONSUME (advance tail) rather than silently return 0, otherwise the
    // reader blocks forever: bytesAvailable() returns >0 but tail never moves.
    if (alignedMsgSize == 0) {
        // Zero-length message: skip the size prefix (4 bytes) to make progress
        ctrl->tail = (tail + sizeof(size_t)) % length;
        __DSB();
        return 0;
    }
    if (alignedMsgSize > length) {
        // Corrupted size prefix (would overflow totalSize calculation).
        // Flush entire buffer to recover - better than blocking forever.
        ctrl->tail = ctrl->head;
        __DSB();
        return 0;
    }

    // Check if full message is available
    size_t totalSize = sizeof(size_t) + alignedMsgSize;

    if (available < totalSize) {
        return 0;  // Incomplete message
    }

    // Check output buffer size
    if (alignedMsgSize > maxLen) {
        // Message too large for buffer - skip it
        ctrl->tail = (tail + totalSize) % length;
        __DSB();
        return 0;
    }

    size_t dataStart = (tail + sizeof(size_t)) % length;
    ringBufCopyOut(outBuffer, dataBuffer, dataStart, alignedMsgSize, length);

    // Update tail pointer
    ctrl->tail = (tail + totalSize) % length;
    __DSB();

    // Return aligned size - caller should use embedded length field for actual size
    return alignedMsgSize;
}

// Write a message to MessageBuffer (includes size prefix)
// Returns true on success
//
// NOTE: Core0's StreamBuffer::send() passes ALIGNED size to xMessageBufferSend.
// To maintain compatibility, we must also write aligned size as the length prefix.
// The alignment is 4 bytes (StreamBufferSendAlignment).
bool msgBufSend(C2CStreamBufferCtrl* ctrl,
                uint8_t*             dataBuffer,
                const uint8_t*       data,
                size_t               dataLen)
{
    // Align data size to 4 bytes to match Core0's behavior
    constexpr size_t Alignment      = 4;
    size_t           alignedDataLen = (dataLen + Alignment - 1) & ~(Alignment - 1);
    size_t           totalSize      = sizeof(size_t) + alignedDataLen;

    if (msgBufSpaceAvailable(ctrl) < totalSize) {
        return false;  // Not enough space
    }

    size_t head   = ctrl->head;
    size_t length = ctrl->length;

    // Write message size
    if (head + sizeof(size_t) <= length) {
        // Common case: no wrap - direct aligned write
        *reinterpret_cast<size_t*>(dataBuffer + head) = alignedDataLen;
    }
    else {
        // Rare case: size_t wraps around - byte-by-byte write
        for (size_t i = 0; i < sizeof(size_t); i++) {
            size_t idx      = (head + i) % length;
            dataBuffer[idx] = reinterpret_cast<const uint8_t*>(&alignedDataLen)[i];
        }
    }

    size_t dataStart = (head + sizeof(size_t)) % length;
    ringBufCopyIn(dataBuffer, dataStart, data, dataLen, length);

    // Write padding zeros if needed (usually 0-3 bytes)
    size_t padLen = alignedDataLen - dataLen;
    if (padLen > 0) {
        size_t padStart = (dataStart + dataLen) % length;
        ringBufMemset(dataBuffer, padStart, 0, padLen, length);
    }

    // Update head pointer
    ctrl->head = (head + totalSize) % length;
    __DSB();

    return true;
}

}  // anonymous namespace

/*******************************************************************************
 * MCMGR Callbacks
 ******************************************************************************/
extern "C" {

#if USB_DEVICE_CONFIG_CDC_ACM
extern void ncsi_signal_acm_rx_rearm(void);
#endif

// Communication event callback (called when Core0 sends data)
static void IpcBm_CommunicationCallback(mcmgr_core_t coreNum, uint16_t eventData, void* context)
{
    (void)coreNum;
    (void)context;
    if (eventData == kCmdInterCoreSendWriteDone) {
        // Core0 has written data to shared memory
        g_dataAvailable = true;
    }
#if USB_DEVICE_CONFIG_CDC_ACM
    else if (eventData == kCmdInterCoreAcmTxDone) {
        // Core0 UART TX done -> re-arm ACM receive so host can send next packet
        ncsi_signal_acm_rx_rearm();
    }
#endif
}

}  // extern "C"

/*******************************************************************************
 * Driver Implementation
 ******************************************************************************/

Status Driver::init()
{
    if (g_initialized) {
        return Status::Ok;
    }

    // Initialize MCMGR (should already be done, but safe to call again)
    auto status = initMcmgr();
    if (status != Status::Ok) {
        return status;
    }

    // Get startup data from Core0: pointer to nv::ipc::StartupInfo in shared_bss
    uint32_t startupDataWord = 0;
    status                   = getStartupData(&startupDataWord);
    if (status != Status::Ok) {
        return status;
    }

    const nv::ipc::StartupInfo* info = reinterpret_cast<const nv::ipc::StartupInfo*>(
        startupDataWord);
    g_sharedMemAddr    = info->c2c_buffers_base;
    g_core1CfgDataAddr = info->core1_cfg_data_base;

    // Setup MessageBuffer pointers
    // g_sharedMemAddr is _c2c_buffers[0] (nv::ipc::StreamBuffer object on Core0)
    //
    // We need to access both buffers:
    //   - _c2c_buffers[0]: Core0ToCore1 (we read from this)
    //   - _c2c_buffers[1]: Core1ToCore0 (we write to this)
    //
    // nv::ipc::StreamBuffer layout on Core0 (total 60 bytes):
    //   - Object base class: obj_is_allocated (1 byte) + padding (3 bytes) = 4 bytes
    //   - sys::ipc::StreamBuffer._stream_buffer (pointer): 4 bytes
    //   - sys::ipc::StreamBuffer.static_stream_buffer (StaticStreamBuffer_t): 36 bytes
    //   - nv::ipc::StreamBuffer.Info (tuple<StreamBufferId, size_t>): 8 bytes
    //   - nv::ipc::StreamBuffer._is_stream_buffer (bool + padding): 4 bytes
    //   - Additional padding: 4 bytes
    //
    // Core0 has static_assert(sizeof(StreamBuffer) == 60) to verify this.
    // If Core0 build fails on that assert, update Core0StreamBufferSize below.

    constexpr size_t Core0StreamBufferSize    = 60;  // sizeof(nv::ipc::StreamBuffer) on Core0
    constexpr size_t StaticStreamBufferOffset = 8;   // Offset of static_stream_buffer in the
                                                     // object

    // Core1 reads from Core0ToCore1 (buffer index 0)
    g_rxCtrl = reinterpret_cast<C2CStreamBufferCtrl*>(g_sharedMemAddr
                                                      + StaticStreamBufferOffset);

    // Core1 writes to Core1ToCore0 (buffer index 1)
    g_txCtrl = reinterpret_cast<C2CStreamBufferCtrl*>(g_sharedMemAddr + Core0StreamBufferSize
                                                      + StaticStreamBufferOffset);

    g_core1CfgData = reinterpret_cast<nv::ipc::Core1CfgData*>(g_core1CfgDataAddr);

    // Get data buffer pointers from the control structures
    // These point to the actual ring buffer data
    g_rxDataBuffer = g_rxCtrl->buffer_ptr;
    g_txDataBuffer = g_txCtrl->buffer_ptr;

    // Register communication event callback
    mcmgr_status_t mcmgrStatus = MCMGR_RegisterEvent(
        kMCMGR_RemoteApplicationEvent, IpcBm_CommunicationCallback, nullptr);

    if (mcmgrStatus != kStatus_MCMGR_Success) {
        return Status::McmgrError;
    }

    // Notify Core0 that Core1 is ready
    // Use kMCMGR_RemoteRPMsgEvent for Core1Ready (matches Core0's event_type_to_kMCMGR_event
    // mapping) Use TriggerEventForce to ensure delivery
    mcmgrStatus = MCMGR_TriggerEventForce(
        kMCMGR_Core0, kMCMGR_RemoteRPMsgEvent, kCmdCore1Ready);

    if (mcmgrStatus != kStatus_MCMGR_Success) {
        return Status::McmgrError;
    }

    g_initialized = true;

    return Status::Ok;
}

bool Driver::isInitialized()
{
    return g_initialized;
}

uint32_t Driver::getSharedMemoryAddress()
{
    return g_sharedMemAddr;
}

uint32_t Driver::getCore1CfgDataAddress()
{
    return g_core1CfgDataAddr;
}

Status Driver::initMcmgr()
{
    mcmgr_status_t status = MCMGR_Init();
    if (status != kStatus_MCMGR_Success && status != kStatus_MCMGR_NotImplemented) {
        return Status::McmgrError;
    }
    return Status::Ok;
}

Status Driver::getStartupData(uint32_t* data)
{
    if (!data) {
        return Status::InvalidParam;
    }

    mcmgr_status_t status;
    uint32_t       loopCount = 0;

    while (loopCount < StartupTimeoutLoops) {
        status = MCMGR_GetStartupData(kMCMGR_Core0, data);

        if (status == kStatus_MCMGR_Success && *data != 0) {
            return Status::Ok;
        }

        for (int i = 0; i < 100; i++) {
            __NOP();
        }
        loopCount++;
    }

    return Status::Timeout;
}

Status Driver::sendToQueue(QueueId queue_id, const uint8_t* data, size_t length, bool is_front)
{
    if (!g_initialized) {
        return Status::NotInitialized;
    }

    if (!data || length == 0) {
        return Status::InvalidParam;
    }

    if (!g_txCtrl || !g_txDataBuffer) {
        return Status::NotInitialized;
    }

    (void)is_front;  // Front insertion not supported in bare-metal

    // Build request using wire format defined in driver.h
    QueueRequestWire request = {};
    request.variant_index    = 0;  // 0 = QueueRequest
    request.length           = static_cast<uint16_t>(length);
    request.is_front         = is_front ? 1 : 0;
    request.queue_id         = static_cast<uint32_t>(queue_id);

    // Pre-check space for BOTH header and data to prevent partial writes.
    // If header is written but data fails, Core0 reads the header and waits
    // for data that never comes, causing permanent IPC state machine desync.
    constexpr size_t Alignment       = 4;
    size_t           alignedReqSize  = (sizeof(request) + Alignment - 1) & ~(Alignment - 1);
    size_t           alignedDataSize = (length + Alignment - 1) & ~(Alignment - 1);
    size_t totalNeeded = sizeof(size_t) + alignedReqSize + sizeof(size_t) + alignedDataSize;

    // Disable interrupts during write
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if (msgBufSpaceAvailable(g_txCtrl) < totalNeeded) {
        __set_PRIMASK(primask);
        return Status::SendFailed;
    }

    // Both writes are guaranteed to succeed now
    bool ok = msgBufSend(
        g_txCtrl, g_txDataBuffer, reinterpret_cast<uint8_t*>(&request), sizeof(request));

    if (ok) {
        ok = msgBufSend(g_txCtrl, g_txDataBuffer, data, length);
    }

    __set_PRIMASK(primask);

    if (!ok) {
        return Status::SendFailed;
    }

    // Notify Core0 that data is available
    notifyCore0();

    return Status::Ok;
}

Status Driver::sendEvent(uint8_t event_id, uint32_t bits, bool is_set)
{
    if (!g_initialized) {
        return Status::NotInitialized;
    }

    if (!g_txCtrl || !g_txDataBuffer) {
        return Status::NotInitialized;
    }

    // Build request using wire format defined in driver.h
    EventRequestWire request = {};
    request.variant_index    = 1;  // 1 = EventRequest
    request.is_set           = is_set ? 1 : 0;
    request.bits             = bits;
    request.event_id         = static_cast<uint32_t>(event_id);

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    bool ok = msgBufSend(
        g_txCtrl, g_txDataBuffer, reinterpret_cast<uint8_t*>(&request), sizeof(request));

    __set_PRIMASK(primask);

    if (!ok) {
        return Status::SendFailed;
    }

    notifyCore0();

    return Status::Ok;
}

size_t Driver::bytesAvailable()
{
    if (!g_initialized || !g_rxCtrl) {
        return 0;
    }

    return msgBufBytesAvailable(g_rxCtrl);
}

size_t Driver::read(uint8_t* buffer, size_t max_length)
{
    if (!g_initialized || !buffer || max_length == 0) {
        return 0;
    }

    if (!g_rxCtrl || !g_rxDataBuffer) {
        return 0;
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    size_t bytesRead = msgBufReceive(g_rxCtrl, g_rxDataBuffer, buffer, max_length);

    __set_PRIMASK(primask);

    return bytesRead;
}

bool Driver::hasDataAvailable()
{
    return g_dataAvailable;
}

void Driver::clearDataAvailable()
{
    g_dataAvailable = false;
}

void Driver::notifyCore0()
{
    if (!g_initialized) {
        return;
    }

    // Use MCMGR_TriggerEventForce instead of MCMGR_TriggerEvent
    // TriggerEvent may fail if target hasn't registered, TriggerEventForce always sends
    (void)MCMGR_TriggerEventForce(
        kMCMGR_Core0, kMCMGR_RemoteApplicationEvent, kCmdInterCoreSendWriteDone);
}

uint8_t* Driver::getCore1CfgNcsiMac()
{
    if (!g_core1CfgData) {
        return nullptr;
    }
    return g_core1CfgData->ncsi_mac;
}

bool Driver::hasValidNcsiMac()
{
    return (g_core1CfgData != nullptr) && (g_core1CfgData->magic == nv::ipc::kCore1CfgMagic)
        && (g_core1CfgData->ncsi_mac_valid != 0U);
}

}  // namespace nv::ipc_bm

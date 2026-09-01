/**
 * @file llquicconnection.h
 * @brief Single QUIC client connection to an OpenSim simulator.
 *
 * Owns the HQUIC connection handle, the single bidirectional stream used
 * for LL packet framing, and (later) the QUIC datagram path used once the
 * peer advertises max_datagram_frame_size > 0.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Second Life Viewer Source Code
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * $/LicenseInfo$
 */

#pragma once

#include "stdtypes.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct QUIC_CONNECTION_EVENT;
struct QUIC_HANDLE;

class LLQuicConfiguration;
class LLQuicStream;

class LLQuicConnection
{
public:
    enum class State
    {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        FAILED,
        CLOSED,
    };

    explicit LLQuicConnection(std::shared_ptr<LLQuicConfiguration> configuration);
    ~LLQuicConnection();

    LLQuicConnection(const LLQuicConnection&)            = delete;
    LLQuicConnection& operator=(const LLQuicConnection&) = delete;
    LLQuicConnection(LLQuicConnection&&)                 = delete;
    LLQuicConnection& operator=(LLQuicConnection&&)      = delete;

    bool connect(const std::string& host, U16 port);
    void close();

    bool sendPacket(const U8* data, S32 size);
    bool sendPacket(const U8* data, S32 size, bool reliable);
    bool receivePacket(std::vector<uint8_t>& out);

    bool sendDatagram(const U8* data, S32 size);
    bool receiveDatagram(std::vector<uint8_t>& out);

    State    getState() const                { return mState.load(std::memory_order_acquire); }
    U16      getMaxDatagramSize() const      { return mDatagramMaxSendLength.load(std::memory_order_acquire); }
    bool     hasDatagramSupport() const      { return getMaxDatagramSize() > 0; }
    uint64_t getLastTransportStatus() const  { return mLastTransportStatus.load(std::memory_order_acquire); }
    uint64_t getLastAppErrorCode() const     { return mLastAppErrorCode.load(std::memory_order_acquire); }
    bool     wasClosedByPeer() const         { return mClosedByPeer.load(std::memory_order_acquire); }
    const std::string& getHost() const       { return mHost; }

    std::string describeFailure() const;

    // Public solely so the C-ABI trampoline in the .cpp can dispatch into us.
    // Not intended for external callers.
    void onConnectionEvent(QUIC_CONNECTION_EVENT* event);

    std::shared_ptr<LLQuicConfiguration> configuration() const noexcept { return mHandle.get_deleter().config; }

private:
    // The deleter pins the parent configuration (which transitively pins the
    // registration and api). Because the lifetime pin lives inside the
    // unique_ptr's deleter, ConnectionClose is guaranteed safe regardless of
    // member declaration order or future refactors of this class.
    struct Deleter
    {
        std::shared_ptr<LLQuicConfiguration> config;
        void operator()(QUIC_HANDLE* h) const noexcept;
    };
    using HandlePtr = std::unique_ptr<QUIC_HANDLE, Deleter>;

    std::atomic<State>                                                mState                 { State::DISCONNECTED };
    std::atomic<U16>                                                  mDatagramMaxSendLength { 0 };
    std::atomic<uint64_t>                                             mLastTransportStatus   { 0 };
    std::atomic<uint64_t>                                             mLastAppErrorCode      { 0 };
    std::atomic<bool>                                                 mClosedByPeer          { false };
    std::atomic<bool>                                                 mReachedConnected      { false };
    std::string                                                       mHost;
    std::mutex                                                        mRxMutex;
    std::deque<std::vector<uint8_t>>                                  mRxQueue;
    std::mutex                                                        mDatagramRxMutex;
    std::deque<std::vector<uint8_t>>                                  mDatagramRxQueue;
    HandlePtr                                                         mHandle;
    std::unique_ptr<LLQuicStream>                                     mStream;
    std::mutex                                                        mPeerStreamsMutex;
    std::vector<std::unique_ptr<LLQuicStream>>                        mPeerStreams;
};

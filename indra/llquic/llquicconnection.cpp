/**
 * @file llquicconnection.cpp
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

#include "linden_common.h"

#include "llquicconnection.h"

#include "llquicconfiguration.h"
#include "llquicregistration.h"
#include "llquicstream.h"

#include <msquic.h>

#include <cstring>
#include <new>
#include <sstream>

namespace
{

struct PendingDatagram
{
    QUIC_BUFFER qb;

    static PendingDatagram* allocate(uint32_t payload_size)
    {
        void* mem = ::operator new(sizeof(PendingDatagram) + payload_size);
        auto* p = new (mem) PendingDatagram();
        p->qb.Length = payload_size;
        p->qb.Buffer = reinterpret_cast<uint8_t*>(p) + sizeof(PendingDatagram);
        return p;
    }

    static void destroy(PendingDatagram* p) noexcept
    {
        if (!p) return;
        p->~PendingDatagram();
        ::operator delete(p);
    }
};

struct PendingDatagramDeleter
{
    void operator()(PendingDatagram* p) const noexcept { PendingDatagram::destroy(p); }
};

using PendingDatagramPtr = std::unique_ptr<PendingDatagram, PendingDatagramDeleter>;

} // namespace

static QUIC_STATUS QUIC_API ll_quic_connection_callback(
    HQUIC                  /*connection*/,
    void*                  context,
    QUIC_CONNECTION_EVENT* event)
{
    if (context && event)
    {
        static_cast<LLQuicConnection*>(context)->onConnectionEvent(event);
    }
    return QUIC_STATUS_SUCCESS;
}

void LLQuicConnection::Deleter::operator()(QUIC_HANDLE* h) const noexcept
{
    if (h && config && config->apiTable())
    {
        config->apiTable()->ConnectionClose(h);
    }
}

LLQuicConnection::LLQuicConnection(std::shared_ptr<LLQuicConfiguration> configuration)
:   mHandle(nullptr, Deleter{std::move(configuration)})
{
}

LLQuicConnection::~LLQuicConnection()
{
    if (mHandle)
    {
        const State s = mState.load(std::memory_order_acquire);
        if (s == State::CONNECTING || s == State::CONNECTED)
        {
            close();
        }
    }
}

bool LLQuicConnection::connect(const std::string& host, U16 port)
{
    if (mState.load(std::memory_order_acquire) != State::DISCONNECTED)
    {
        LL_WARNS("Quic") << "connect(): already in state "
                        << static_cast<int>(mState.load(std::memory_order_acquire))
                        << LL_ENDL;
        return false;
    }

    const auto& config = mHandle.get_deleter().config;
    if (!config || !config->handle() || !config->apiTable() || !config->registration())
    {
        LL_WARNS("Quic") << "connect(): missing configuration" << LL_ENDL;
        mState.store(State::FAILED, std::memory_order_release);
        return false;
    }

    mHost = host;
    const QUIC_API_TABLE* api = config->apiTable();

    QUIC_HANDLE* raw = nullptr;
    QUIC_STATUS  status = api->ConnectionOpen(
        config->registration()->handle(),
        ll_quic_connection_callback,
        this,
        &raw);
    if (QUIC_FAILED(status))
    {
        LL_WARNS("Quic") << "ConnectionOpen failed: 0x"
                        << std::hex << status << std::dec << LL_ENDL;
        mState.store(State::FAILED, std::memory_order_release);
        return false;
    }

    mHandle.reset(raw);
    mState.store(State::CONNECTING, std::memory_order_release);

    mStream = LLQuicStream::create(
        config,
        mHandle.get(),
        [this](std::vector<uint8_t> frame)
        {
            std::lock_guard<std::mutex> lock(mRxMutex);
            mRxQueue.push_back(std::move(frame));
        });
    if (!mStream)
    {
        mHandle.reset();
        mState.store(State::FAILED, std::memory_order_release);
        return false;
    }

    status = api->ConnectionStart(
        mHandle.get(),
        config->handle(),
        QUIC_ADDRESS_FAMILY_UNSPEC,
        mHost.c_str(),
        port);
    if (QUIC_FAILED(status))
    {
        LL_WARNS("Quic") << "ConnectionStart to " << host << ":" << port
                        << " failed: 0x" << std::hex << status << std::dec << LL_ENDL;
        mStream.reset();
        mHandle.reset();
        mState.store(State::FAILED, std::memory_order_release);
        return false;
    }

    LL_INFOS("Quic") << "QUIC connecting to " << host << ":" << port << LL_ENDL;
    return true;
}

void LLQuicConnection::close()
{
    const auto& config = mHandle.get_deleter().config;
    if (!mHandle || !config || !config->apiTable())
    {
        mState.store(State::CLOSED, std::memory_order_release);
        return;
    }

    config->apiTable()->ConnectionShutdown(
        mHandle.get(),
        QUIC_CONNECTION_SHUTDOWN_FLAG_NONE,
        /*ErrorCode=*/0);
}

bool LLQuicConnection::sendPacket(const U8* data, S32 size)
{
    const State s = mState.load(std::memory_order_acquire);
    if (s != State::CONNECTED && s != State::CONNECTING)
    {
        return false;
    }
    if (!mStream)
    {
        return false;
    }
    return mStream->send(data, size);
}

bool LLQuicConnection::sendPacket(const U8* data, S32 size, bool reliable)
{
    if (!reliable && size >= 0)
    {
        const U16 max = mDatagramMaxSendLength.load(std::memory_order_acquire);
        if (max > 0 && static_cast<U32>(size) <= max)
        {
            return sendDatagram(data, size);
        }
    }
    return sendPacket(data, size);
}

bool LLQuicConnection::receivePacket(std::vector<uint8_t>& out)
{
    std::lock_guard<std::mutex> lock(mRxMutex);
    if (mRxQueue.empty())
    {
        return false;
    }
    out = std::move(mRxQueue.front());
    mRxQueue.pop_front();
    return true;
}

bool LLQuicConnection::sendDatagram(const U8* data, S32 size)
{
    if (mState.load(std::memory_order_acquire) != State::CONNECTED)
    {
        return false;
    }
    if (size < 0 || size > 0xFFFF)
    {
        return false;
    }

    const U16 max_send = mDatagramMaxSendLength.load(std::memory_order_acquire);
    if (max_send == 0 || static_cast<U16>(size) > max_send)
    {
        return false;
    }

    const auto& config = mHandle.get_deleter().config;
    if (!mHandle || !config || !config->apiTable())
    {
        return false;
    }

    const uint32_t n = static_cast<uint32_t>(size);
    PendingDatagramPtr pending(PendingDatagram::allocate(n));
    if (n > 0)
    {
        std::memcpy(pending->qb.Buffer, data, n);
    }

    QUIC_STATUS status = config->apiTable()->DatagramSend(
        mHandle.get(),
        &pending->qb,
        1,
        QUIC_SEND_FLAG_NONE,
        pending.get());
    if (QUIC_FAILED(status))
    {
        LL_WARNS("Quic") << "DatagramSend failed: 0x"
                        << std::hex << status << std::dec << LL_ENDL;
        return false;
    }

    pending.release();
    return true;
}

bool LLQuicConnection::receiveDatagram(std::vector<uint8_t>& out)
{
    std::lock_guard<std::mutex> lock(mDatagramRxMutex);
    if (mDatagramRxQueue.empty())
    {
        return false;
    }
    out = std::move(mDatagramRxQueue.front());
    mDatagramRxQueue.pop_front();
    return true;
}

void LLQuicConnection::onConnectionEvent(QUIC_CONNECTION_EVENT* event)
{
    switch (event->Type)
    {
        case QUIC_CONNECTION_EVENT_CONNECTED:
            mState.store(State::CONNECTED, std::memory_order_release);
            mReachedConnected.store(true, std::memory_order_release);
            LL_INFOS("Quic") << "QUIC connected to " << mHost << LL_ENDL;
            break;

        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
            mLastTransportStatus.store(
                event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status,
                std::memory_order_release);
            mLastAppErrorCode.store(
                event->SHUTDOWN_INITIATED_BY_TRANSPORT.ErrorCode,
                std::memory_order_release);
            LL_WARNS("Quic")
                << "QUIC transport-initiated shutdown to " << mHost
                << " status=0x" << std::hex << event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status
                << std::dec
                << " errorcode=" << event->SHUTDOWN_INITIATED_BY_TRANSPORT.ErrorCode
                << LL_ENDL;
            break;

        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
            mClosedByPeer.store(true, std::memory_order_release);
            mLastAppErrorCode.store(
                event->SHUTDOWN_INITIATED_BY_PEER.ErrorCode,
                std::memory_order_release);
            LL_WARNS("Quic")
                << "QUIC peer-initiated shutdown from " << mHost
                << " errorcode=" << event->SHUTDOWN_INITIATED_BY_PEER.ErrorCode
                << LL_ENDL;
            break;

        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
        {
            const State prior = mState.load(std::memory_order_acquire);
            const State next  = (prior == State::CONNECTED) ? State::CLOSED : State::FAILED;
            mState.store(next, std::memory_order_release);
            LL_INFOS("Quic") << "QUIC shutdown complete to " << mHost
                            << " (final state=" << static_cast<int>(next) << ")"
                            << LL_ENDL;
            break;
        }

        case QUIC_CONNECTION_EVENT_DATAGRAM_STATE_CHANGED:
        {
            const auto& ds  = event->DATAGRAM_STATE_CHANGED;
            const U16   max = ds.SendEnabled ? ds.MaxSendLength : 0;
            mDatagramMaxSendLength.store(max, std::memory_order_release);
            LL_DEBUGS("Quic")
                << "QUIC datagram state to " << mHost
                << " send_enabled=" << static_cast<int>(ds.SendEnabled)
                << " max_send_len=" << ds.MaxSendLength
                << LL_ENDL;
            break;
        }

        case QUIC_CONNECTION_EVENT_DATAGRAM_RECEIVED:
        {
            const auto& rcv = event->DATAGRAM_RECEIVED;
            if (rcv.Buffer && rcv.Buffer->Length > 0)
            {
                std::vector<uint8_t> bytes(
                    rcv.Buffer->Buffer,
                    rcv.Buffer->Buffer + rcv.Buffer->Length);
                std::lock_guard<std::mutex> lock(mDatagramRxMutex);
                mDatagramRxQueue.push_back(std::move(bytes));
            }
            break;
        }

        case QUIC_CONNECTION_EVENT_DATAGRAM_SEND_STATE_CHANGED:
        {
            const auto& ss    = event->DATAGRAM_SEND_STATE_CHANGED;
            const auto  state = ss.State;
            const bool  can_free =
                state == QUIC_DATAGRAM_SEND_SENT                  ||
                state == QUIC_DATAGRAM_SEND_LOST_DISCARDED        ||
                state == QUIC_DATAGRAM_SEND_ACKNOWLEDGED          ||
                state == QUIC_DATAGRAM_SEND_ACKNOWLEDGED_SPURIOUS ||
                state == QUIC_DATAGRAM_SEND_CANCELED;
            if (can_free && ss.ClientContext)
            {
                PendingDatagramPtr pending(
                    static_cast<PendingDatagram*>(ss.ClientContext));
                (void)pending;
            }
            break;
        }

        case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
        {
            const auto& config = mHandle.get_deleter().config;
            auto*       stream = event->PEER_STREAM_STARTED.Stream;
            LL_DEBUGS("Quic")
                << "Adopting peer-initiated stream from " << mHost
                << " flags=0x" << std::hex
                << static_cast<unsigned>(event->PEER_STREAM_STARTED.Flags)
                << std::dec
                << LL_ENDL;
            if (stream && config && config->apiTable())
            {
                auto adopted = LLQuicStream::adopt(
                    config,
                    stream,
                    [this](std::vector<uint8_t> frame)
                    {
                        std::lock_guard<std::mutex> lock(mRxMutex);
                        mRxQueue.push_back(std::move(frame));
                    });
                if (adopted)
                {
                    std::lock_guard<std::mutex> lock(mPeerStreamsMutex);
                    mPeerStreams.push_back(std::move(adopted));
                }
                else
                {
                    LL_WARNS("Quic") << "LLQuicStream::adopt failed; closing peer stream" << LL_ENDL;
                    config->apiTable()->StreamClose(stream);
                }
            }
            break;
        }

        default:
            break;
    }
}

namespace
{

const char* tls_alert_name(uint8_t alert)
{
    switch (alert)
    {
        case 0:   return "close_notify";
        case 10:  return "unexpected_message";
        case 20:  return "bad_record_mac";
        case 22:  return "record_overflow";
        case 40:  return "handshake_failure";
        case 41:  return "no_certificate";
        case 42:  return "bad_certificate";
        case 43:  return "unsupported_certificate";
        case 44:  return "certificate_revoked";
        case 45:  return "certificate_expired";
        case 46:  return "certificate_unknown";
        case 47:  return "illegal_parameter";
        case 48:  return "unknown_ca";
        case 49:  return "access_denied";
        case 50:  return "decode_error";
        case 51:  return "decrypt_error";
        case 70:  return "protocol_version";
        case 71:  return "insufficient_security";
        case 80:  return "internal_error";
        case 86:  return "inappropriate_fallback";
        case 90:  return "user_canceled";
        case 109: return "missing_extension";
        case 110: return "unsupported_extension";
        case 112: return "unrecognized_name";
        case 113: return "bad_certificate_status_response";
        case 115: return "unknown_psk_identity";
        case 116: return "certificate_required";
        case 120: return "no_application_protocol";
        default:  return nullptr;
    }
}

bool extract_tls_alert_from_status(uint64_t status, uint8_t* out_alert)
{
    const uint8_t low = static_cast<uint8_t>(status & 0xFFu);
    if (static_cast<uint64_t>(static_cast<QUIC_STATUS>(QUIC_STATUS_TLS_ALERT(low))) == status)
    {
        if (out_alert) *out_alert = low;
        return true;
    }
    return false;
}

std::string format_tls_alert(uint8_t alert)
{
    std::ostringstream o;
    if (const char* name = tls_alert_name(alert))
    {
        o << "TLS alert '" << name << "'";
    }
    else
    {
        o << "TLS alert " << static_cast<unsigned>(alert);
    }
    return o.str();
}

std::string format_quic_status(uint64_t status)
{
    if (status == 0) return std::string();

    uint8_t alert = 0;
    if (extract_tls_alert_from_status(status, &alert))
    {
        return format_tls_alert(alert);
    }

    const QUIC_STATUS s = static_cast<QUIC_STATUS>(status);
    if (s == QUIC_STATUS_HANDSHAKE_FAILURE)    return "Handshake failure";
    if (s == QUIC_STATUS_CONNECTION_TIMEOUT)   return "Connection timeout";
    if (s == QUIC_STATUS_CONNECTION_IDLE)      return "Connection idle timeout";
    if (s == QUIC_STATUS_CONNECTION_REFUSED)   return "Connection refused";
    if (s == QUIC_STATUS_UNREACHABLE)          return "Host unreachable";
    if (s == QUIC_STATUS_VER_NEG_ERROR)        return "QUIC version negotiation failed";
    if (s == QUIC_STATUS_PROTOCOL_ERROR)       return "QUIC protocol error";
    if (s == QUIC_STATUS_INTERNAL_ERROR)       return "QUIC internal error";
    if (s == QUIC_STATUS_USER_CANCELED)        return "Cancelled by peer";
    if (s == QUIC_STATUS_ALPN_NEG_FAILURE)     return "ALPN negotiation failed";
    if (s == QUIC_STATUS_INVALID_PARAMETER)    return "Invalid parameter";
    if (s == QUIC_STATUS_INVALID_STATE)        return "Invalid QUIC state";
    if (s == QUIC_STATUS_NOT_SUPPORTED)        return "Not supported";
    if (s == QUIC_STATUS_NOT_FOUND)            return "Not found";
    if (s == QUIC_STATUS_OUT_OF_MEMORY)        return "Out of memory";
    if (s == QUIC_STATUS_ABORTED)              return "Aborted";
    if (s == QUIC_STATUS_ADDRESS_IN_USE)       return "Address already in use";
    if (s == QUIC_STATUS_INVALID_ADDRESS)      return "Invalid address";
    if (s == QUIC_STATUS_BUFFER_TOO_SMALL)     return "Buffer too small";
    if (s == QUIC_STATUS_STREAM_LIMIT_REACHED) return "Stream limit reached";
    if (s == QUIC_STATUS_ALPN_IN_USE)          return "ALPN in use";
    if (s == QUIC_STATUS_TLS_ERROR)            return "TLS error";
    if (s == QUIC_STATUS_CERT_EXPIRED)         return "Server certificate expired";
    if (s == QUIC_STATUS_CERT_UNTRUSTED_ROOT)  return "Server certificate has untrusted root";
    if (s == QUIC_STATUS_CERT_NO_CERT)         return "Server presented no certificate";

    std::ostringstream o;
    o << "QUIC status 0x" << std::hex << status << std::dec;
    return o.str();
}

std::string format_app_error(uint64_t app_err)
{
    if (app_err == 0) return std::string();

    if (app_err >= 0x100u && app_err < 0x200u)
    {
        return format_tls_alert(static_cast<uint8_t>(app_err & 0xFFu));
    }

    std::ostringstream o;
    o << "application error 0x" << std::hex << app_err << std::dec;
    return o.str();
}

} // namespace

std::string LLQuicConnection::describeFailure() const
{
    const State    s         = getState();
    const uint64_t tx_st     = getLastTransportStatus();
    const uint64_t app_err   = getLastAppErrorCode();
    const bool     by_peer   = wasClosedByPeer();
    const bool     was_conn  = mReachedConnected.load(std::memory_order_acquire);

    std::ostringstream o;
    const std::string& host = mHost.empty() ? std::string("simulator") : mHost;

    const std::string tx_text  = format_quic_status(tx_st);
    const std::string app_text = format_app_error(app_err);

    auto append_reasons = [&]()
    {
        if (!tx_text.empty() || !app_text.empty())
        {
            o << ": ";
            if (!tx_text.empty())
            {
                o << tx_text;
            }
            if (!app_text.empty())
            {
                if (!tx_text.empty()) o << "; ";
                o << app_text;
            }
        }
    };

    switch (s)
    {
        case State::DISCONNECTED:
            o << "QUIC client could not start a connection to " << host
              << " (local configuration error)";
            break;

        case State::CONNECTING:
            o << "QUIC handshake with " << host << " did not complete";
            append_reasons();
            break;

        case State::CONNECTED:
            o << "QUIC connection to " << host
              << " was established but the simulator did not confirm the session";
            break;

        case State::FAILED:
            if (by_peer)
            {
                o << "Simulator " << host << " refused or aborted the QUIC connection";
            }
            else if (was_conn)
            {
                o << "QUIC connection to " << host
                  << " established but the simulator did not confirm the session";
            }
            else
            {
                o << "Could not establish QUIC connection to " << host;
            }
            append_reasons();
            break;

        case State::CLOSED:
            if (by_peer)
            {
                o << "Simulator " << host << " closed the QUIC connection";
            }
            else if (was_conn && static_cast<QUIC_STATUS>(tx_st) == QUIC_STATUS_CONNECTION_IDLE)
            {
                o << "QUIC connection to " << host
                  << " timed out waiting for the simulator to confirm the session";
            }
            else
            {
                o << "QUIC connection to " << host << " was lost";
            }
            append_reasons();
            break;
    }

    return o.str();
}

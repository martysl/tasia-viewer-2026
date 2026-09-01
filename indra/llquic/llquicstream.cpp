#include "linden_common.h"

#include "llquicstream.h"

#include "llquicconfiguration.h"

#include <msquic.h>

#include <algorithm>
#include <cstring>
#include <new>
#include <utility>

namespace
{

struct PendingSend
{
    QUIC_BUFFER qb;

    static PendingSend* allocate(uint32_t framed_size)
    {
        void* mem = ::operator new(sizeof(PendingSend) + framed_size);
        auto* p = new (mem) PendingSend();
        p->qb.Length = framed_size;
        p->qb.Buffer = reinterpret_cast<uint8_t*>(p) + sizeof(PendingSend);
        return p;
    }

    static void destroy(PendingSend* p) noexcept
    {
        if (!p) return;
        p->~PendingSend();
        ::operator delete(p);
    }
};

struct PendingSendDeleter
{
    void operator()(PendingSend* p) const noexcept { PendingSend::destroy(p); }
};

using PendingSendPtr = std::unique_ptr<PendingSend, PendingSendDeleter>;

} // namespace

static QUIC_STATUS QUIC_API ll_quic_stream_callback(
    HQUIC              /*stream*/,
    void*              context,
    QUIC_STREAM_EVENT* event)
{
    if (context && event)
    {
        static_cast<LLQuicStream*>(context)->onStreamEvent(event);
    }
    return QUIC_STATUS_SUCCESS;
}

void LLQuicStream::Deleter::operator()(QUIC_HANDLE* h) const noexcept
{
    if (h && config && config->apiTable())
    {
        config->apiTable()->StreamClose(h);
    }
}

std::unique_ptr<LLQuicStream> LLQuicStream::create(
    std::shared_ptr<LLQuicConfiguration> config,
    QUIC_HANDLE*                         connection_handle,
    FrameCallback                        on_frame)
{
    if (!config || !config->apiTable() || !connection_handle)
    {
        LL_WARNS("Quic") << "LLQuicStream::create: missing arguments" << LL_ENDL;
        return nullptr;
    }

    const QUIC_API_TABLE* api = config->apiTable();

    auto stream = std::unique_ptr<LLQuicStream>(
        new LLQuicStream(config, std::move(on_frame)));

    QUIC_HANDLE* raw = nullptr;
    QUIC_STATUS  status = api->StreamOpen(
        connection_handle,
        QUIC_STREAM_OPEN_FLAG_NONE,
        ll_quic_stream_callback,
        stream.get(),
        &raw);
    if (QUIC_FAILED(status))
    {
        LL_WARNS("Quic") << "StreamOpen failed: 0x"
                        << std::hex << status << std::dec << LL_ENDL;
        return nullptr;
    }

    stream->mHandle.reset(raw);

    status = api->StreamStart(raw, QUIC_STREAM_START_FLAG_IMMEDIATE);
    if (QUIC_FAILED(status))
    {
        LL_WARNS("Quic") << "StreamStart failed: 0x"
                        << std::hex << status << std::dec << LL_ENDL;
        return nullptr;
    }

    return stream;
}

std::unique_ptr<LLQuicStream> LLQuicStream::adopt(
    std::shared_ptr<LLQuicConfiguration> config,
    QUIC_HANDLE*                         peer_stream_handle,
    FrameCallback                        on_frame)
{
    if (!config || !config->apiTable() || !peer_stream_handle)
    {
        LL_WARNS("Quic") << "LLQuicStream::adopt: missing arguments" << LL_ENDL;
        return nullptr;
    }

    auto stream = std::unique_ptr<LLQuicStream>(
        new LLQuicStream(config, std::move(on_frame)));

    config->apiTable()->SetCallbackHandler(
        peer_stream_handle,
        reinterpret_cast<void*>(&ll_quic_stream_callback),
        stream.get());

    stream->mHandle.reset(peer_stream_handle);
    return stream;
}

LLQuicStream::LLQuicStream(std::shared_ptr<LLQuicConfiguration> config, FrameCallback on_frame)
:   mHandle(nullptr, Deleter{std::move(config)})
,   mOnFrame(std::move(on_frame))
{
}

LLQuicStream::~LLQuicStream() = default;

bool LLQuicStream::send(const U8* data, S32 size)
{
    if (!mHandle || size < 0)
    {
        return false;
    }

    const auto& config = mHandle.get_deleter().config;
    if (!config || !config->apiTable())
    {
        return false;
    }

    const uint32_t n      = static_cast<uint32_t>(size);
    const uint32_t framed = 4u + n;
    PendingSendPtr pending(PendingSend::allocate(framed));
    uint8_t* p = pending->qb.Buffer;
    p[0] = static_cast<uint8_t>((n >> 24) & 0xFF);
    p[1] = static_cast<uint8_t>((n >> 16) & 0xFF);
    p[2] = static_cast<uint8_t>((n >>  8) & 0xFF);
    p[3] = static_cast<uint8_t>( n        & 0xFF);
    if (size > 0)
    {
        std::memcpy(p + 4, data, static_cast<size_t>(size));
    }

    QUIC_STATUS status = config->apiTable()->StreamSend(
        mHandle.get(),
        &pending->qb,
        1,
        QUIC_SEND_FLAG_NONE,
        pending.get());
    if (QUIC_FAILED(status))
    {
        LL_WARNS("Quic") << "StreamSend failed: 0x"
                        << std::hex << status << std::dec << LL_ENDL;
        return false;
    }

    pending.release();
    return true;
}

void LLQuicStream::feedRx(const uint8_t* data, uint32_t size)
{
    uint32_t offset = 0;
    while (offset < size)
    {
        if (mRxPhase == RxPhase::LENGTH)
        {
            const uint32_t need = 4u - mRxLengthBytesRead;
            const uint32_t take = std::min(need, size - offset);
            std::memcpy(mRxLengthBytes + mRxLengthBytesRead, data + offset, take);
            mRxLengthBytesRead += take;
            offset             += take;

            if (mRxLengthBytesRead == 4u)
            {
                mRxExpectedLength =
                    (static_cast<uint32_t>(mRxLengthBytes[0]) << 24) |
                    (static_cast<uint32_t>(mRxLengthBytes[1]) << 16) |
                    (static_cast<uint32_t>(mRxLengthBytes[2]) <<  8) |
                     static_cast<uint32_t>(mRxLengthBytes[3]);
                mRxPayload.clear();
                mRxPayload.reserve(mRxExpectedLength);
                mRxPhase = RxPhase::PAYLOAD;
            }
        }
        else
        {
            const uint32_t need = mRxExpectedLength - static_cast<uint32_t>(mRxPayload.size());
            const uint32_t take = std::min(need, size - offset);
            mRxPayload.insert(mRxPayload.end(), data + offset, data + offset + take);
            offset += take;

            if (mRxPayload.size() == mRxExpectedLength)
            {
                if (mOnFrame)
                {
                    mOnFrame(std::move(mRxPayload));
                }
                mRxPayload.clear();
                mRxLengthBytesRead = 0;
                mRxExpectedLength  = 0;
                mRxPhase           = RxPhase::LENGTH;
            }
        }
    }
}

void LLQuicStream::onStreamEvent(QUIC_STREAM_EVENT* event)
{
    switch (event->Type)
    {
        case QUIC_STREAM_EVENT_START_COMPLETE:
            if (QUIC_FAILED(event->START_COMPLETE.Status))
            {
                LL_WARNS("Quic") << "Stream START_COMPLETE failed: 0x"
                                << std::hex << event->START_COMPLETE.Status
                                << std::dec << LL_ENDL;
            }
            break;

        case QUIC_STREAM_EVENT_RECEIVE:
        {
            const auto& rcv = event->RECEIVE;
            for (uint32_t i = 0; i < rcv.BufferCount; ++i)
            {
                feedRx(rcv.Buffers[i].Buffer, rcv.Buffers[i].Length);
            }
            break;
        }

        case QUIC_STREAM_EVENT_SEND_COMPLETE:
        {
            PendingSendPtr pending(
                static_cast<PendingSend*>(event->SEND_COMPLETE.ClientContext));
            (void)pending;
            break;
        }

        case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
        case QUIC_STREAM_EVENT_PEER_RECEIVE_ABORTED:
            LL_WARNS("Quic") << "Stream aborted by peer" << LL_ENDL;
            break;

        default:
            break;
    }
}

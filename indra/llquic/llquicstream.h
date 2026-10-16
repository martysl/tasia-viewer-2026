#pragma once

#include "stdtypes.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

struct QUIC_HANDLE;
struct QUIC_STREAM_EVENT;

class LLQuicConfiguration;

class LLQuicStream
{
public:
    using FrameCallback = std::function<void(std::vector<uint8_t>)>;

    static std::unique_ptr<LLQuicStream> create(
        std::shared_ptr<LLQuicConfiguration> config,
        QUIC_HANDLE*                         connection_handle,
        FrameCallback                        on_frame);

    static std::unique_ptr<LLQuicStream> adopt(
        std::shared_ptr<LLQuicConfiguration> config,
        QUIC_HANDLE*                         peer_stream_handle,
        FrameCallback                        on_frame);

    ~LLQuicStream();

    LLQuicStream(const LLQuicStream&)            = delete;
    LLQuicStream& operator=(const LLQuicStream&) = delete;
    LLQuicStream(LLQuicStream&&)                 = delete;
    LLQuicStream& operator=(LLQuicStream&&)      = delete;

    bool send(const U8* data, S32 size);

    void onStreamEvent(QUIC_STREAM_EVENT* event);

private:
    struct Deleter
    {
        std::shared_ptr<LLQuicConfiguration> config;
        void operator()(QUIC_HANDLE* h) const noexcept;
    };
    using HandlePtr = std::unique_ptr<QUIC_HANDLE, Deleter>;

    enum class RxPhase { LENGTH, PAYLOAD };

    LLQuicStream(std::shared_ptr<LLQuicConfiguration> config, FrameCallback on_frame);

    void feedRx(const uint8_t* data, uint32_t size);

    HandlePtr            mHandle;
    FrameCallback        mOnFrame;
    RxPhase              mRxPhase           { RxPhase::LENGTH };
    uint32_t             mRxLengthBytesRead { 0 };
    uint32_t             mRxExpectedLength  { 0 };
    uint8_t              mRxLengthBytes[4]  { };
    std::vector<uint8_t> mRxPayload;
};

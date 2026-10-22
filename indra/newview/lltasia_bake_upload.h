/** @file lltasia_bake_upload.h
 * Local-bake upload sequencing, independent of network and graphics APIs.
 * Copyright (C) 2026 Tasia. LGPL-2.1-or-later.
 */
#ifndef LL_LLTASIA_BAKE_UPLOAD_H
#define LL_LLTASIA_BAKE_UPLOAD_H
#include <cstdint>

namespace TasiaBake
{
class UploadPolicy
{
public:
    enum class Result { Ignored, Obsolete, Accepted, Retry, Exhausted };
    static constexpr unsigned MAX_ATTEMPTS = 7;

    void request()
    {
        ++mRevision;
        mNeeded = true;
        mFailures = 0;
    }

    void cancel()
    {
        ++mRevision;
        ++mTicket;
        mNeeded = false;
        mInFlight = false;
        mFailures = 0;
    }

    std::uint64_t begin()
    {
        if (!mNeeded || mInFlight) return 0;
        mActiveRevision = mRevision;
        mNeeded = false;
        mInFlight = true;
        return ++mTicket;
    }

    Result complete(std::uint64_t ticket, bool success)
    {
        if (!mInFlight || ticket != mTicket) return Result::Ignored;
        mInFlight = false;
        if (mActiveRevision != mRevision)
        {
            mNeeded = true;
            return Result::Obsolete;
        }
        if (success)
        {
            mFailures = 0;
            return Result::Accepted;
        }
        ++mFailures;
        mNeeded = mFailures < MAX_ATTEMPTS;
        return mNeeded ? Result::Retry : Result::Exhausted;
    }

    bool needed() const { return mNeeded; }
    bool inFlight() const { return mInFlight; }
    bool pending() const { return mNeeded || mInFlight; }
    unsigned failures() const { return mFailures; }
    bool useHttp(bool has_capability) const { return has_capability && mFailures == 0; }
    unsigned retryDelay() const { return mFailures ? 2u << (mFailures - 1) : 0; }

private:
    std::uint64_t mRevision = 0;
    std::uint64_t mActiveRevision = 0;
    std::uint64_t mTicket = 0;
    unsigned mFailures = 0;
    bool mNeeded = false;
    bool mInFlight = false;
};
}
#endif

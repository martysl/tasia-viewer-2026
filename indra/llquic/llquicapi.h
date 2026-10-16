/**
 * @file llquicapi.h
 * @brief RAII wrapper around the MsQuic process-wide API table.
 *
 * Owns the const QUIC_API_TABLE* returned by MsQuicOpen2 and ensures
 * MsQuicClose is invoked exactly once when the last shared_ptr goes away.
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

#include <memory>

struct QUIC_API_TABLE;

class LLQuicApi
{
public:
    static std::shared_ptr<LLQuicApi> create();

    ~LLQuicApi();

    LLQuicApi(const LLQuicApi&)            = delete;
    LLQuicApi& operator=(const LLQuicApi&) = delete;
    LLQuicApi(LLQuicApi&&)                 = delete;
    LLQuicApi& operator=(LLQuicApi&&)      = delete;

    const QUIC_API_TABLE* table() const noexcept { return mTable.get(); }

private:
    struct Deleter
    {
        void operator()(const QUIC_API_TABLE* api) const noexcept;
    };

    explicit LLQuicApi(const QUIC_API_TABLE* api);

    std::unique_ptr<const QUIC_API_TABLE, Deleter> mTable;
};

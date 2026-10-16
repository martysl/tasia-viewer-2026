/**
 * @file llquicapi.cpp
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

#include "llquicapi.h"

#include <msquic.h>

void LLQuicApi::Deleter::operator()(const QUIC_API_TABLE* api) const noexcept
{
    if (api)
    {
        MsQuicClose(api);
    }
}

std::shared_ptr<LLQuicApi> LLQuicApi::create()
{
    const QUIC_API_TABLE* api = nullptr;
    QUIC_STATUS status = MsQuicOpen2(&api);
    if (QUIC_FAILED(status))
    {
        LL_WARNS("Quic") << "MsQuicOpen2 failed: 0x"
                        << std::hex << status << std::dec << LL_ENDL;
        return nullptr;
    }
    return std::shared_ptr<LLQuicApi>(new LLQuicApi(api));
}

LLQuicApi::LLQuicApi(const QUIC_API_TABLE* api)
:   mTable(api)
{
}

LLQuicApi::~LLQuicApi() = default;

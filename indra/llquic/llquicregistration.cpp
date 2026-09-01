/**
 * @file llquicregistration.cpp
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

#include "llquicregistration.h"

#include "llquicapi.h"

#include <msquic.h>

void LLQuicRegistration::Deleter::operator()(QUIC_HANDLE* h) const noexcept
{
    if (h && api && api->table())
    {
        api->table()->RegistrationClose(h);
    }
}

std::shared_ptr<LLQuicRegistration> LLQuicRegistration::create(
    std::shared_ptr<LLQuicApi> api,
    const std::string&         app_name)
{
    if (!api || !api->table())
    {
        LL_WARNS("Quic") << "LLQuicRegistration::create: missing api" << LL_ENDL;
        return nullptr;
    }

    QUIC_REGISTRATION_CONFIG cfg = {};
    cfg.AppName          = app_name.c_str();
    cfg.ExecutionProfile = QUIC_EXECUTION_PROFILE_LOW_LATENCY;

    QUIC_HANDLE* raw = nullptr;
    QUIC_STATUS  status = api->table()->RegistrationOpen(&cfg, &raw);
    if (QUIC_FAILED(status))
    {
        LL_WARNS("Quic") << "RegistrationOpen failed: 0x"
                        << std::hex << status << std::dec << LL_ENDL;
        return nullptr;
    }

    HandlePtr handle(raw, Deleter{std::move(api)});
    return std::shared_ptr<LLQuicRegistration>(
        new LLQuicRegistration(std::move(handle)));
}

LLQuicRegistration::LLQuicRegistration(HandlePtr handle)
:   mHandle(std::move(handle))
{
}

LLQuicRegistration::~LLQuicRegistration() = default;

const QUIC_API_TABLE* LLQuicRegistration::apiTable() const noexcept
{
    const auto& api = mHandle.get_deleter().api;
    return api ? api->table() : nullptr;
}

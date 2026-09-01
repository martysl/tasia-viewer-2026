/**
 * @file llquicconfiguration.cpp
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

#include "llquicconfiguration.h"

#include "llquicregistration.h"

#include <msquic.h>

void LLQuicConfiguration::Deleter::operator()(QUIC_HANDLE* h) const noexcept
{
    if (h && registration && registration->apiTable())
    {
        registration->apiTable()->ConfigurationClose(h);
    }
}

std::shared_ptr<LLQuicConfiguration> LLQuicConfiguration::create(
    std::shared_ptr<LLQuicRegistration> registration,
    const QUIC_BUFFER*                  alpn,
    unsigned int                        alpn_count,
    const QUIC_SETTINGS&                settings,
    const QUIC_CREDENTIAL_CONFIG&       credentials)
{
    if (!registration || !registration->handle() || !registration->apiTable())
    {
        LL_WARNS("Quic") << "LLQuicConfiguration::create: missing registration" << LL_ENDL;
        return nullptr;
    }

    const QUIC_API_TABLE* api = registration->apiTable();

    QUIC_HANDLE* raw = nullptr;
    QUIC_STATUS  status = api->ConfigurationOpen(
        registration->handle(),
        alpn,
        alpn_count,
        &settings,
        sizeof(settings),
        nullptr,
        &raw);
    if (QUIC_FAILED(status))
    {
        LL_WARNS("Quic") << "ConfigurationOpen failed: 0x"
                        << std::hex << status << std::dec << LL_ENDL;
        return nullptr;
    }

    HandlePtr handle(raw, Deleter{std::move(registration)});

    status = api->ConfigurationLoadCredential(handle.get(), &credentials);
    if (QUIC_FAILED(status))
    {
        LL_WARNS("Quic") << "ConfigurationLoadCredential failed: 0x"
                        << std::hex << status << std::dec << LL_ENDL;
        return nullptr;
    }

    return std::shared_ptr<LLQuicConfiguration>(
        new LLQuicConfiguration(std::move(handle)));
}

LLQuicConfiguration::LLQuicConfiguration(HandlePtr handle)
:   mHandle(std::move(handle))
{
}

LLQuicConfiguration::~LLQuicConfiguration() = default;

const QUIC_API_TABLE* LLQuicConfiguration::apiTable() const noexcept
{
    const auto& reg = mHandle.get_deleter().registration;
    return reg ? reg->apiTable() : nullptr;
}

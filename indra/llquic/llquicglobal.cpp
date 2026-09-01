/**
 * @file llquicglobal.cpp
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

#include "llquicglobal.h"

#include "llquicapi.h"
#include "llquicconfiguration.h"
#include "llquicregistration.h"

#include <msquic.h>

namespace
{
    constexpr const char     QUIC_ALPN[]                 = "opensim-ll/1";
    constexpr uint32_t       QUIC_IDLE_TIMEOUT_MS        = 60 * 1000;
    constexpr uint32_t       QUIC_KEEPALIVE_MS           = 30 * 1000;
    constexpr uint32_t       QUIC_HANDSHAKE_IDLE_TIMEOUT_MS = 10 * 1000;
    constexpr const char     QUIC_REGISTRATION_NAME[]    = "FirestormQuic";
    constexpr uint16_t       QUIC_PEER_BIDI_STREAMS      = 1024;
    constexpr uint16_t       QUIC_PEER_UNIDI_STREAMS     = 1024;
}

LLQuicGlobal& LLQuicGlobal::instance()
{
    static LLQuicGlobal sInstance;
    return sInstance;
}

LLQuicGlobal::LLQuicGlobal()  = default;
LLQuicGlobal::~LLQuicGlobal() = default;

bool LLQuicGlobal::initialize(const std::string& ca_bundle_path)
{
    if (mClientConfig)
    {
        return true;
    }

    mCaBundlePath = ca_bundle_path;

    auto api = LLQuicApi::create();
    if (!api)
    {
        return false;
    }

    auto registration = LLQuicRegistration::create(std::move(api), QUIC_REGISTRATION_NAME);
    if (!registration)
    {
        return false;
    }

    const QUIC_BUFFER alpn = {
        static_cast<uint32_t>(sizeof(QUIC_ALPN) - 1),
        reinterpret_cast<uint8_t*>(const_cast<char*>(QUIC_ALPN))
    };

    QUIC_SETTINGS settings = {};
    settings.IdleTimeoutMs                  = QUIC_IDLE_TIMEOUT_MS;
    settings.IsSet.IdleTimeoutMs            = TRUE;
    settings.HandshakeIdleTimeoutMs         = QUIC_HANDSHAKE_IDLE_TIMEOUT_MS;
    settings.IsSet.HandshakeIdleTimeoutMs   = TRUE;
    settings.KeepAliveIntervalMs            = QUIC_KEEPALIVE_MS;
    settings.IsSet.KeepAliveIntervalMs      = TRUE;
    settings.PeerBidiStreamCount            = QUIC_PEER_BIDI_STREAMS;
    settings.IsSet.PeerBidiStreamCount      = TRUE;
    settings.PeerUnidiStreamCount           = QUIC_PEER_UNIDI_STREAMS;
    settings.IsSet.PeerUnidiStreamCount     = TRUE;
    settings.DatagramReceiveEnabled         = TRUE;
    settings.IsSet.DatagramReceiveEnabled   = TRUE;

    QUIC_CREDENTIAL_CONFIG credentials = {};
    credentials.Type  = QUIC_CREDENTIAL_TYPE_NONE;
    credentials.Flags = QUIC_CREDENTIAL_FLAG_CLIENT;
#if !LL_WINDOWS
    credentials.Flags |= QUIC_CREDENTIAL_FLAG_USE_TLS_BUILTIN_CERTIFICATE_VALIDATION;
    if (!mCaBundlePath.empty())
    {
        credentials.CaCertificateFile = mCaBundlePath.c_str();
        credentials.Flags |= QUIC_CREDENTIAL_FLAG_SET_CA_CERTIFICATE_FILE;
        LL_INFOS("Quic") << "MsQuic using CA bundle: " << mCaBundlePath << LL_ENDL;
    }
    else
    {
        LL_WARNS("Quic") << "MsQuic CA bundle path not provided; "
                            "relying on QUICTLS built-in trust paths "
                            "(may fail on packaged builds)." << LL_ENDL;
    }
#endif

    auto config = LLQuicConfiguration::create(
        std::move(registration),
        &alpn,
        1,
        settings,
        credentials);
    if (!config)
    {
        return false;
    }

    mClientConfig = std::move(config);
    LL_INFOS("Quic") << "MsQuic initialized (ALPN=" << QUIC_ALPN << ")" << LL_ENDL;
    return true;
}

void LLQuicGlobal::shutdown()
{
    if (mClientConfig)
    {
        mClientConfig.reset();
        mCaBundlePath.clear();
        LL_INFOS("Quic") << "MsQuic shut down" << LL_ENDL;
    }
}

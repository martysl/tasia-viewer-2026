/**
 * @file llquicconfiguration.h
 * @brief RAII wrapper around an MsQuic configuration handle.
 *
 * Holds a shared_ptr to the LLQuicRegistration that produced it, which
 * transitively pins the api table needed to dispatch ConfigurationClose.
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
struct QUIC_BUFFER;
struct QUIC_CREDENTIAL_CONFIG;
struct QUIC_HANDLE;
struct QUIC_SETTINGS;

class LLQuicRegistration;

class LLQuicConfiguration
{
public:
    static std::shared_ptr<LLQuicConfiguration> create(
        std::shared_ptr<LLQuicRegistration> registration,
        const QUIC_BUFFER*                  alpn,
        unsigned int                        alpn_count,
        const QUIC_SETTINGS&                settings,
        const QUIC_CREDENTIAL_CONFIG&       credentials);

    ~LLQuicConfiguration();

    LLQuicConfiguration(const LLQuicConfiguration&)            = delete;
    LLQuicConfiguration& operator=(const LLQuicConfiguration&) = delete;
    LLQuicConfiguration(LLQuicConfiguration&&)                 = delete;
    LLQuicConfiguration& operator=(LLQuicConfiguration&&)      = delete;

    QUIC_HANDLE*                        handle()       const noexcept { return mHandle.get(); }
    const QUIC_API_TABLE*               apiTable()     const noexcept;
    std::shared_ptr<LLQuicRegistration> registration() const noexcept { return mHandle.get_deleter().registration; }

private:
    // The deleter pins the parent registration (which transitively pins the
    // api). The unique_ptr is therefore self-contained: no sibling member
    // needs to outlive it for ConfigurationClose to be safe.
    struct Deleter
    {
        std::shared_ptr<LLQuicRegistration> registration;
        void operator()(QUIC_HANDLE* h) const noexcept;
    };
    using HandlePtr = std::unique_ptr<QUIC_HANDLE, Deleter>;

    explicit LLQuicConfiguration(HandlePtr handle);

    HandlePtr mHandle;
};

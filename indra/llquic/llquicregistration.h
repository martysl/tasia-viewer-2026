/**
 * @file llquicregistration.h
 * @brief RAII wrapper around an MsQuic registration handle.
 *
 * Holds a shared_ptr to the LLQuicApi that produced the handle so the
 * api table is guaranteed alive for the duration of the registration's
 * lifetime, including its destruction (RegistrationClose dispatches
 * through the api table).
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
#include <string>

struct QUIC_API_TABLE;
struct QUIC_HANDLE;

class LLQuicApi;

class LLQuicRegistration
{
public:
    static std::shared_ptr<LLQuicRegistration> create(
        std::shared_ptr<LLQuicApi> api,
        const std::string&         app_name);

    ~LLQuicRegistration();

    LLQuicRegistration(const LLQuicRegistration&)            = delete;
    LLQuicRegistration& operator=(const LLQuicRegistration&) = delete;
    LLQuicRegistration(LLQuicRegistration&&)                 = delete;
    LLQuicRegistration& operator=(LLQuicRegistration&&)      = delete;

    QUIC_HANDLE*               handle()   const noexcept { return mHandle.get(); }
    const QUIC_API_TABLE*      apiTable() const noexcept;
    std::shared_ptr<LLQuicApi> api()      const noexcept { return mHandle.get_deleter().api; }

private:
    // The deleter owns a shared_ptr to the parent api so the api table is
    // guaranteed alive for the duration of RegistrationClose. There is no
    // sibling member to outlive: the unique_ptr is self-contained.
    struct Deleter
    {
        std::shared_ptr<LLQuicApi> api;
        void operator()(QUIC_HANDLE* h) const noexcept;
    };
    using HandlePtr = std::unique_ptr<QUIC_HANDLE, Deleter>;

    explicit LLQuicRegistration(HandlePtr handle);

    HandlePtr mHandle;
};

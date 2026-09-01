/**
 * @file llquicglobal.h
 * @brief Process-wide MsQuic registration and configuration.
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

#include "stdtypes.h"

#include <memory>
#include <string>

class LLQuicConfiguration;

class LLQuicGlobal
{
public:
    static LLQuicGlobal& instance();

    bool initialize(const std::string& ca_bundle_path = std::string());
    void shutdown();

    bool isInitialized() const { return static_cast<bool>(mClientConfig); }

    std::shared_ptr<LLQuicConfiguration> getClientConfiguration() const { return mClientConfig; }

private:
    LLQuicGlobal();
    ~LLQuicGlobal();

    LLQuicGlobal(const LLQuicGlobal&)            = delete;
    LLQuicGlobal& operator=(const LLQuicGlobal&) = delete;

    std::shared_ptr<LLQuicConfiguration> mClientConfig;
    std::string                          mCaBundlePath;
};

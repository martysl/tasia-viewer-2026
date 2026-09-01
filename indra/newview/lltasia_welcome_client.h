/**
 * @file lltasia_welcome_client.h
 * @brief Async client for loading Tasia welcome text.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Tasia Viewer Project
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
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * $/LicenseInfo$
 */

#ifndef LL_TASIA_WELCOME_CLIENT_H
#define LL_TASIA_WELCOME_CLIENT_H

#include "stdtypes.h"

#include <boost/function.hpp>
#include <string>

class LLTasiaWelcomeClient
{
public:
    typedef boost::function<void(const std::string&)> response_callback_t;

    static void requestLine(response_callback_t callback);

private:
    static void fetchCoro(std::string url, U32 timeout_seconds, U32 max_bytes, response_callback_t callback);
};

#endif // LL_TASIA_WELCOME_CLIENT_H

/**
 * @file lltasia_welcome_client.cpp
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

#include "llviewerprecompiledheaders.h"

#include "lltasia_welcome_client.h"

#include "llcorehttputil.h"
#include "llcoros.h"
#include "llhttpconstants.h"
#include "llmath.h"
#include "llsd.h"
#include "llstring.h"
#include "llviewercontrol.h"

#include <boost/bind.hpp>
#include <sstream>

namespace
{
const U32 MIN_WELCOME_BYTES = 1024;
const U32 MAX_WELCOME_BYTES = 1024 * 1024;
const U32 MAX_WELCOME_LINE_CHARS = 512;

std::string chooseFirstUsableLine(const LLSD::Binary& data, U32 max_bytes)
{
    if (data.empty())
    {
        return std::string();
    }

    const std::size_t bytes_to_read = llmin<std::size_t>(data.size(), max_bytes);
    std::string body(data.begin(), data.begin() + bytes_to_read);

    std::istringstream stream(body);
    std::string line;
    bool first_line = true;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
        {
            line.erase(line.size() - 1);
        }

        if (first_line && line.size() >= 3 &&
            static_cast<U8>(line[0]) == 0xef &&
            static_cast<U8>(line[1]) == 0xbb &&
            static_cast<U8>(line[2]) == 0xbf)
        {
            line.erase(0, 3);
        }
        first_line = false;

        LLStringUtil::trim(line);
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        if (line.size() > MAX_WELCOME_LINE_CHARS)
        {
            continue;
        }

        return line;
    }

    return std::string();
}

void sendResponse(const LLTasiaWelcomeClient::response_callback_t& callback, const std::string& line)
{
    if (callback)
    {
        callback(line);
    }
}
}

// static
void LLTasiaWelcomeClient::requestLine(response_callback_t callback)
{
    std::string url = gSavedSettings.getString("TasiaWelcomeURL");
    LLStringUtil::trim(url);
    if (url == "https://i.let-us.cyou/welcome.txt")
    {
        url = "https://i.let-us.cyou/welcome.php";
    }
    if (url.empty())
    {
        sendResponse(callback, std::string());
        return;
    }

    const F32 timeout_setting = gSavedSettings.getF32("TasiaWelcomeURLTimeout");
    const U32 timeout_seconds = static_cast<U32>(llclamp(static_cast<S32>(timeout_setting + 0.999f), 1, 30));
    const U32 max_bytes = llclamp(gSavedSettings.getU32("TasiaWelcomeMaxBytes"), MIN_WELCOME_BYTES, MAX_WELCOME_BYTES);

    LLCoros::instance().launch("LLTasiaWelcomeClient::fetchCoro",
        boost::bind(&LLTasiaWelcomeClient::fetchCoro, url, timeout_seconds, max_bytes, callback));
}

// static
void LLTasiaWelcomeClient::fetchCoro(std::string url, U32 timeout_seconds, U32 max_bytes, response_callback_t callback)
{
    LLCore::HttpRequest::ptr_t http_request(new LLCore::HttpRequest);
    LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t http_adapter(
        new LLCoreHttpUtil::HttpCoroutineAdapter("TasiaWelcomeFetch", LLCore::HttpRequest::DEFAULT_POLICY_ID));
    LLCore::HttpHeaders::ptr_t http_headers(new LLCore::HttpHeaders);
    LLCore::HttpOptions::ptr_t http_options(new LLCore::HttpOptions);

    http_options->setTimeout(timeout_seconds);
    http_options->setTransferTimeout(timeout_seconds);
    http_options->setRetries(0);

    std::ostringstream range;
    range << "bytes=0-" << (max_bytes - 1);
    http_headers->append(HTTP_OUT_HEADER_ACCEPT, "text/plain, text/*;q=0.9, */*;q=0.1");
    http_headers->append(HTTP_OUT_HEADER_RANGE, range.str());

    LLSD result = http_adapter->getRawAndSuspend(http_request, url, http_options, http_headers);
    LLSD http_results = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
    LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(http_results);
    if (!status)
    {
        LL_WARNS("TasiaWelcome") << "Welcome text fetch failed: " << status.toString() << LL_ENDL;
        sendResponse(callback, std::string());
        return;
    }

    if (!result.has(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_RAW))
    {
        LL_WARNS("TasiaWelcome") << "Welcome text fetch returned no body" << LL_ENDL;
        sendResponse(callback, std::string());
        return;
    }

    const LLSD::Binary& raw = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_RAW].asBinary();
    sendResponse(callback, chooseFirstUsableLine(raw, max_bytes));
}

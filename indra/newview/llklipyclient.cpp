/**
 * @file llklipyclient.cpp
 * @brief KLIPY API client for Tasia Viewer chat GIF/Clip features.
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

#include "llklipyclient.h"
#include "llcorehttputil.h"
#include "llcoros.h"
#include "llhttpconstants.h"
#include "llmath.h"
#include "llsd.h"
#include "llstring.h"
#include "lltasia_klipy_key.h"
#include "lluri.h"
#include "llviewercontrol.h"

#include <boost/bind.hpp>
#include <sstream>

namespace
{
const std::string KLIPY_API_BASE_URL = "https://api.klipy.com/v1/";
const std::string KLIPY_NOT_CONFIGURED = "KLIPY is not configured.";
const S32 KLIPY_MIN_LIMIT = 1;
const S32 KLIPY_MAX_LIMIT = 50;
const S32 KLIPY_MAX_OFFSET = 4999;

std::string getString(const LLSD& data, const std::string& key)
{
    return data.has(key) ? data[key].asString() : std::string();
}

S32 getS32(const LLSD& data, const std::string& key)
{
    return data.has(key) ? data[key].asInteger() : 0;
}

std::string contentTypeToEndpoint(LLKlipyClient::ContentType type)
{
    switch (type)
    {
    case LLKlipyClient::CONTENT_GIF:     return "gifs";
    case LLKlipyClient::CONTENT_CLIP:    return "clips";
    case LLKlipyClient::CONTENT_STICKER: return "stickers";
    case LLKlipyClient::CONTENT_MEME:    return "memes";
    default:                             return "gifs";
    }
}

std::string mediaVariantKey(LLKlipyClient::ContentType type, bool tiny)
{
    if (type == LLKlipyClient::CONTENT_CLIP)
    {
        return tiny ? "tinymp4" : "mp4";
    }
    return tiny ? "tinygif" : "gif";
}

void fillMedia(const LLSD& media, const std::string& key, std::string& url, S32* width = NULL, S32* height = NULL)
{
    if (!media.has(key))
    {
        return;
    }

    const LLSD& variant = media[key];
    url = getString(variant, "url");
    if (width)
    {
        *width = getS32(variant, "width");
    }
    if (height)
    {
        *height = getS32(variant, "height");
    }
}

LLKlipyClient::results_t parseResults(const LLSD& response, LLKlipyClient::ContentType type)
{
    LLKlipyClient::results_t results;

    // KLIPY response: { "data": { "items": [...] } }
    if (!response.has("data") || !response["data"].has("items"))
    {
        return results;
    }

    const LLSD& items = response["data"]["items"];
    if (!items.isArray())
    {
        return results;
    }

    for (LLSD::array_const_iterator it = items.beginArray(); it != items.endArray(); ++it)
    {
        const LLSD& item = *it;
        LLKlipyClient::Result result;
        result.id = getString(item, "id");
        result.description = getString(item, "description");
        result.page_url = getString(item, "url");

        if (result.id.empty() || !item.has("media"))
        {
            continue;
        }

        const LLSD& media = item["media"];

        // Fill GIF variants
        std::string tiny_key = mediaVariantKey(type, true);
        std::string full_key = mediaVariantKey(type, false);

        fillMedia(media, tiny_key, result.tiny_gif_url, &result.preview_width, &result.preview_height);
        fillMedia(media, full_key, result.original_gif_url, &result.original_width, &result.original_height);

        // For clips, also try to get mp4 variants
        if (type == LLKlipyClient::CONTENT_CLIP)
        {
            fillMedia(media, "tinymp4", result.tiny_mp4_url);
            fillMedia(media, "mp4", result.mp4_url);
            result.has_video = true;
        }
        else
        {
            // GIFs can also have mp4 for web playback
            fillMedia(media, "tinymp4", result.tiny_mp4_url);
            fillMedia(media, "mp4", result.mp4_url);
        }

        // Preview URL: prefer tiny variant for grid, fall back to full
        result.preview_gif_url = result.tiny_gif_url;
        if (result.preview_gif_url.empty())
        {
            result.preview_gif_url = !result.original_gif_url.empty() ? result.original_gif_url : result.mp4_url;
        }
        if (result.preview_gif_url.empty())
        {
            result.preview_gif_url = result.tiny_mp4_url;
        }

        // Build fallback page URL
        if (result.page_url.empty())
        {
            // Strip "clips:", "gifs:", etc. prefix for URL
            std::string clean_id = result.id;
            std::size_t colon_pos = clean_id.find(':');
            if (colon_pos != std::string::npos)
            {
                clean_id = clean_id.substr(colon_pos + 1);
            }
            result.page_url = "https://klipy.com/" + contentTypeToEndpoint(type) + "/" + clean_id;
        }

        results.push_back(result);
    }

    return results;
}

void sendResponse(const LLKlipyClient::response_callback_t& callback,
                  bool success,
                  const std::string& message,
                  const LLKlipyClient::results_t& results = LLKlipyClient::results_t())
{
    if (callback)
    {
        callback(success, message, results);
    }
}

std::string buildRequestURL(const std::string& endpoint,
                            const std::string& api_key,
                            const std::string& query,
                            S32 limit,
                            S32 offset)
{
    std::ostringstream url;
    url << KLIPY_API_BASE_URL << endpoint
        << "/search"
        << "?apikey=" << LLURI::escape(api_key)
        << "&limit=" << llclamp(limit, KLIPY_MIN_LIMIT, KLIPY_MAX_LIMIT)
        << "&page=" << (llclamp(offset, 0, KLIPY_MAX_OFFSET) / llclamp(limit, KLIPY_MIN_LIMIT, KLIPY_MAX_LIMIT) + 1);

    if (!query.empty())
    {
        url << "&q=" << LLURI::escape(query);
    }

    return url.str();
}

std::string buildTrendingURL(const std::string& endpoint,
                             const std::string& api_key,
                             S32 limit,
                             S32 offset)
{
    std::ostringstream url;
    url << KLIPY_API_BASE_URL << endpoint
        << "/trending"
        << "?apikey=" << LLURI::escape(api_key)
        << "&limit=" << llclamp(limit, KLIPY_MIN_LIMIT, KLIPY_MAX_LIMIT)
        << "&page=" << (llclamp(offset, 0, KLIPY_MAX_OFFSET) / llclamp(limit, KLIPY_MIN_LIMIT, KLIPY_MAX_LIMIT) + 1);

    return url.str();
}
} // anonymous namespace

// static
const std::string& LLKlipyClient::notConfiguredMessage()
{
    return KLIPY_NOT_CONFIGURED;
}

// static
bool LLKlipyClient::isConfigured()
{
    return gSavedSettings.getBOOL("TasiaKlipyEnabled") && LLTasiaKlipyKey::hasConfiguredAPIKey();
}

// static
void LLKlipyClient::search(const std::string& query, ContentType type,
                           response_callback_t callback, S32 limit, S32 offset)
{
    if (!gSavedSettings.getBOOL("TasiaKlipyEnabled"))
    {
        sendResponse(callback, false, "KLIPY is disabled.");
        return;
    }

    std::string trimmed_query = query;
    LLStringUtil::trim(trimmed_query);
    if (trimmed_query.empty())
    {
        sendResponse(callback, false, "Enter a search term.");
        return;
    }

    const std::string api_key = LLTasiaKlipyKey::getConfiguredAPIKey();
    if (api_key.empty())
    {
        sendResponse(callback, false, notConfiguredMessage());
        return;
    }

    std::string endpoint = contentTypeToEndpoint(type);

    LLCoros::instance().launch("LLKlipyClient::searchCoro",
        boost::bind(&LLKlipyClient::requestCoro,
                    buildRequestURL(endpoint, api_key, trimmed_query, limit, offset),
                    type,
                    callback));
}

// static
void LLKlipyClient::trending(ContentType type,
                             response_callback_t callback, S32 limit, S32 offset)
{
    if (!gSavedSettings.getBOOL("TasiaKlipyEnabled"))
    {
        sendResponse(callback, false, "KLIPY is disabled.");
        return;
    }

    const std::string api_key = LLTasiaKlipyKey::getConfiguredAPIKey();
    if (api_key.empty())
    {
        sendResponse(callback, false, notConfiguredMessage());
        return;
    }

    std::string endpoint = contentTypeToEndpoint(type);

    LLCoros::instance().launch("LLKlipyClient::trendingCoro",
        boost::bind(&LLKlipyClient::requestCoro,
                    buildTrendingURL(endpoint, api_key, limit, offset),
                    type,
                    callback));
}

// static
void LLKlipyClient::requestCoro(std::string url, ContentType type, response_callback_t callback)
{
    LLCore::HttpRequest::ptr_t http_request(new LLCore::HttpRequest);
    LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t http_adapter(
        new LLCoreHttpUtil::HttpCoroutineAdapter("KlipyFetch", LLCore::HttpRequest::DEFAULT_POLICY_ID));
    LLCore::HttpHeaders::ptr_t http_headers(new LLCore::HttpHeaders);
    LLCore::HttpOptions::ptr_t http_options(new LLCore::HttpOptions);

    http_options->setTimeout(15);
    http_options->setTransferTimeout(15);
    http_options->setRetries(1);
    http_headers->append(HTTP_OUT_HEADER_ACCEPT, "application/json");

    LLSD response = http_adapter->getJsonAndSuspend(http_request, url, http_options, http_headers);
    LLSD http_results = response[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
    LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(http_results);
    if (!status)
    {
        LL_WARNS("KLIPY") << "KLIPY request failed: " << status.toString() << LL_ENDL;
        sendResponse(callback, false, "KLIPY request failed.");
        return;
    }

    // Check for KLIPY error response
    if (response.has("error") && response["error"].asBoolean())
    {
        std::string message = response.has("message") ? response["message"].asString() : "KLIPY request failed.";
        LL_WARNS("KLIPY") << "KLIPY API returned an error: " << message << LL_ENDL;
        sendResponse(callback, false, message);
        return;
    }

    LLKlipyClient::results_t results = parseResults(response, type);
    sendResponse(callback, true, results.empty() ? "No results found." : std::string(), results);
}

// static
const std::string LLKlipyClient::contentTypeString(ContentType type)
{
    switch (type)
    {
    case CONTENT_GIF:     return "gifs";
    case CONTENT_CLIP:    return "clips";
    case CONTENT_STICKER: return "stickers";
    case CONTENT_MEME:    return "memes";
    default:              return "gifs";
    }
}

// static
const std::string& LLKlipyClient::contentTypeDisplayName(ContentType type)
{
    static const std::string names[] = { "GIFs", "Clips", "Stickers", "Memes" };
    if (type >= 0 && type < CONTENT_COUNT)
    {
        return names[type];
    }
    return names[0];
}

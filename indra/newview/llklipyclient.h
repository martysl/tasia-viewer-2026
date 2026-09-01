/**
 * @file llklipyclient.h
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

#ifndef LL_KLIPY_CLIENT_H
#define LL_KLIPY_CLIENT_H

#include "stdtypes.h"

#include <boost/function.hpp>
#include <string>
#include <vector>

class LLKlipyClient
{
public:
    // Content type for KLIPY API (gif, clip, sticker, meme)
    enum ContentType
    {
        CONTENT_GIF,
        CONTENT_CLIP,
        CONTENT_STICKER,
        CONTENT_MEME,
        CONTENT_COUNT
    };

    struct Result
    {
        std::string id;
        std::string description;
        std::string page_url;
        // GIF media variants
        std::string tiny_gif_url;
        std::string preview_gif_url;
        std::string original_gif_url;
        // Video media variants (clips, mp4)
        std::string tiny_mp4_url;
        std::string mp4_url;
        // Dimensions
        S32 preview_width = 0;
        S32 preview_height = 0;
        S32 original_width = 0;
        S32 original_height = 0;
        // Whether this result has video
        bool has_video = false;
    };

    typedef std::vector<Result> results_t;
    typedef boost::function<void(bool success, const std::string& message, const results_t& results)> response_callback_t;

    static const std::string& notConfiguredMessage();
    static bool isConfigured();
    static void search(const std::string& query, ContentType type,
                       response_callback_t callback, S32 limit = 24, S32 offset = 0);
    static void trending(ContentType type,
                         response_callback_t callback, S32 limit = 24, S32 offset = 0);

    static const std::string contentTypeString(ContentType type);
    static const std::string& contentTypeDisplayName(ContentType type);

private:
    static void requestCoro(std::string url, ContentType type, response_callback_t callback);
};

#endif // LL_KLIPY_CLIENT_H

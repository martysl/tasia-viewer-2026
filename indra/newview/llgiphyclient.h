/**
 * @file llgiphyclient.h
 * @brief GIPHY API client for Tasia Viewer chat GIF features.
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

#ifndef LL_GIPHY_CLIENT_H
#define LL_GIPHY_CLIENT_H

#include "stdtypes.h"

#include <boost/function.hpp>
#include <string>
#include <vector>

class LLGiphyClient
{
public:
    struct Result
    {
        std::string id;
        std::string title;
        std::string page_url;
        std::string preview_gif_url;
        std::string fixed_width_gif_url;
        std::string downsized_gif_url;
        std::string original_gif_url;
        S32 preview_width = 0;
        S32 preview_height = 0;
    };

    struct Category
    {
        std::string name;
        std::string name_encoded;
        std::string preview_gif_url;
        std::string preview_width;
        std::string preview_height;
    };

    struct Suggestion
    {
        std::string term;
    };

    typedef std::vector<Result> results_t;
    typedef std::vector<Category> categories_t;
    typedef std::vector<Suggestion> suggestions_t;
    typedef boost::function<void(bool success, const std::string& message, const results_t& results)> response_callback_t;
    typedef boost::function<void(bool success, const std::string& message, const categories_t& categories)> categories_callback_t;
    typedef boost::function<void(bool success, const std::string& message, const suggestions_t& suggestions)> suggestions_callback_t;

    static const std::string& notConfiguredMessage();
    static bool isConfigured();

    // GIFs
    static void search(const std::string& query, response_callback_t callback, S32 limit = 24, S32 offset = 0);
    static void trending(response_callback_t callback, S32 limit = 24, S32 offset = 0);
    static void random(const std::string& tag, response_callback_t callback);
    static void translate(const std::string& text, response_callback_t callback);

    // Stickers
    static void stickerSearch(const std::string& query, response_callback_t callback, S32 limit = 24, S32 offset = 0);
    static void stickerTrending(response_callback_t callback, S32 limit = 24, S32 offset = 0);
    static void stickerRandom(const std::string& tag, response_callback_t callback);

    // Emoji
    static void emoji(response_callback_t callback, S32 limit = 24, S32 offset = 0);

    // Browsing
    static void categories(categories_callback_t callback);
    static void categoryGifs(const std::string& category, response_callback_t callback, S32 limit = 24, S32 offset = 0);

    // Suggestions
    static void searchSuggestions(const std::string& query, suggestions_callback_t callback, S32 limit = 8);

private:
    static void requestCoro(std::string url, response_callback_t callback);
    static void requestCoroCategories(std::string url, categories_callback_t callback);
    static void requestCoroSuggestions(std::string url, suggestions_callback_t callback);
};

#endif // LL_GIPHY_CLIENT_H

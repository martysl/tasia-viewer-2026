/**
 * @file lltasia_chat_preview.cpp
 * @brief Shared chat link previews (image, YouTube, GIPHY) for nearby chat and IM/group chat.
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

#include "lltasia_chat_preview.h"

#include "llbutton.h"
#include "llfloaterreg.h"
#include "llfloaterwebcontent.h"
#include "llmediactrl.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "lluri.h"
#include "llweb.h"

#include <algorithm>

namespace
{
bool tasiaEndsWith(const std::string& value, const std::string& suffix)
{
    if (suffix.size() > value.size())
    {
        return false;
    }
    return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool tasiaIsYouTubeHost(std::string host)
{
    return host == "youtube.com" ||
        host == "www.youtube.com" ||
        host == "m.youtube.com" ||
        host == "youtu.be" ||
        host == "www.youtube-nocookie.com" ||
        host == "youtube-nocookie.com" ||
        tasiaEndsWith(host, ".youtube.com");
}

bool tasiaIsGiphyHost(std::string host)
{
    return host == "giphy.com" ||
        host == "www.giphy.com" ||
        tasiaEndsWith(host, ".giphy.com");
}

bool tasiaIsImageURL(std::string path)
{
    return tasiaEndsWith(path, ".png") ||
        tasiaEndsWith(path, ".jpg") ||
        tasiaEndsWith(path, ".jpeg") ||
        tasiaEndsWith(path, ".gif") ||
        tasiaEndsWith(path, ".webp") ||
        tasiaEndsWith(path, ".bmp") ||
        tasiaEndsWith(path, ".apng");
}

std::vector<std::string> tasiaSplitPath(const std::string& path)
{
    std::vector<std::string> segments;
    std::string::size_type start = 0;
    while (start < path.size())
    {
        std::string::size_type slash = path.find('/', start);
        if (slash == std::string::npos)
        {
            segments.push_back(path.substr(start));
            break;
        }
        if (slash > start)
        {
            segments.push_back(path.substr(start, slash - start));
        }
        start = slash + 1;
    }
    return segments;
}

void tasiaStripTrailingUrlPunctuation(std::string& url)
{
    while (!url.empty())
    {
        const char last = url[url.size() - 1];
        if (last == '.' || last == ',' || last == ';' || last == ':' ||
            last == '!' || last == '?' || last == ')' || last == ']' || last == '}')
        {
            url.erase(url.size() - 1);
        }
        else
        {
            break;
        }
    }
}

std::string tasiaMakeHostedYouTubePlayerURL(const std::string& video_id)
{
    return "https://api.tasiaviewer.work/api/v1/yt?v=" + LLURI::escape(video_id);
}

bool tasiaIsGiphyId(const std::string& value)
{
    if (value.size() < 4 || value.size() > 80)
    {
        return false;
    }

    for (std::string::const_iterator it = value.begin(); it != value.end(); ++it)
    {
        if (!isalnum(static_cast<unsigned char>(*it)))
        {
            return false;
        }
    }
    return true;
}

std::string tasiaGiphyIdFromSlug(const std::string& slug)
{
    std::string candidate = slug;
    std::string::size_type dash_pos = slug.rfind('-');
    if (dash_pos != std::string::npos && dash_pos + 1 < slug.size())
    {
        candidate = slug.substr(dash_pos + 1);
    }

    if (tasiaIsGiphyId(candidate))
    {
        return candidate;
    }
    return std::string();
}

bool tasiaExtractGiphyPreviewFromURL(std::string url, TasiaGiphyPreview& preview)
{
    tasiaStripTrailingUrlPunctuation(url);
    LLURI uri(url);
    std::string scheme = uri.scheme();
    LLStringUtil::toLower(scheme);
    if (scheme != "http" && scheme != "https")
    {
        return false;
    }

    std::string host = uri.hostName();
    LLStringUtil::toLower(host);
    if (!tasiaIsGiphyHost(host))
    {
        return false;
    }

    const std::vector<std::string> segments = tasiaSplitPath(uri.path());
    if (segments.size() >= 2 && segments[0] == "media")
    {
        preview.id = segments[1];
    }
    else if (segments.size() >= 2 && segments[0] == "gifs")
    {
        // Giphy URLs look like https://giphy.com/gifs/raz-razvan-razvanflore-Mv1QDJzeB9eaDHzvvf
        // — the real ID is the alphanumeric token AFTER the LAST dash, not the first segment.
        preview.id = tasiaGiphyIdFromSlug(segments[segments.size() - 1]);
    }

    if (preview.id.empty())
    {
        return false;
    }

    preview.page_url = "https://giphy.com/gifs/" + preview.id;
    preview.media_url = "https://i.giphy.com/media/" + preview.id + "/giphy.gif";
    return true;
}

bool tasiaExtractImagePreviewFromURL(std::string url, TasiaImagePreview& preview)
{
    tasiaStripTrailingUrlPunctuation(url);
    LLURI uri(url);
    std::string scheme = uri.scheme();
    LLStringUtil::toLower(scheme);
    if (scheme != "http" && scheme != "https")
    {
        return false;
    }

    std::string path = uri.path();
    LLStringUtil::toLower(path);
    if (!tasiaIsImageURL(path))
    {
        return false;
    }

    preview.url = url;
    return true;
}

bool tasiaExtractYouTubePreviewFromURL(std::string url, TasiaYouTubePreview& preview)
{
    tasiaStripTrailingUrlPunctuation(url);
    LLURI uri(url);
    std::string scheme = uri.scheme();
    LLStringUtil::toLower(scheme);
    if (scheme != "http" && scheme != "https")
    {
        return false;
    }

    std::string host = uri.hostName();
    LLStringUtil::toLower(host);
    if (!tasiaIsYouTubeHost(host))
    {
        return false;
    }

    std::string video_id;
    const std::vector<std::string> segments = tasiaSplitPath(uri.path());
    if (host == "youtu.be")
    {
        if (!segments.empty())
        {
            video_id = segments[0];
        }
    }
    else if (!segments.empty() && segments[0] == "watch")
    {
        LLSD query = uri.queryMap();
        if (query.has("v"))
        {
            video_id = query["v"].asString();
        }
    }
    else
    {
        for (std::vector<std::string>::const_iterator it = segments.begin(); it != segments.end(); ++it)
        {
            if ((*it == "embed" || *it == "shorts" || *it == "live") && (it + 1) != segments.end())
            {
                video_id = *(it + 1);
                break;
            }
        }
        if (video_id.empty() && !segments.empty())
        {
            video_id = segments[segments.size() - 1];
        }
    }

    if (video_id.empty())
    {
        return false;
    }

    preview.video_id = video_id;
    preview.page_url = "https://www.youtube.com/watch?v=" + video_id;
    preview.player_url = tasiaMakeHostedYouTubePlayerURL(video_id);
    return true;
}
}

bool tasiaFindFirstGiphyPreview(const std::string& text, TasiaGiphyPreview& preview)
{
    std::string::size_type search_pos = 0;
    while (search_pos < text.size())
    {
        std::string::size_type http_pos = text.find("http://", search_pos);
        std::string::size_type https_pos = text.find("https://", search_pos);
        std::string::size_type url_pos = std::min(http_pos, https_pos);
        if (http_pos == std::string::npos)
        {
            url_pos = https_pos;
        }
        else if (https_pos == std::string::npos)
        {
            url_pos = http_pos;
        }

        if (url_pos == std::string::npos)
        {
            return false;
        }

        std::string::size_type url_end = text.find_first_of(" \n\r\t<>\"'", url_pos);
        if (url_end == std::string::npos)
        {
            url_end = text.size();
        }

        if (tasiaExtractGiphyPreviewFromURL(text.substr(url_pos, url_end - url_pos), preview))
        {
            return true;
        }
        search_pos = url_end;
    }
    return false;
}

bool tasiaFindFirstImagePreview(const std::string& text, TasiaImagePreview& preview)
{
    std::string::size_type search_pos = 0;
    while (search_pos < text.size())
    {
        std::string::size_type http_pos = text.find("http://", search_pos);
        std::string::size_type https_pos = text.find("https://", search_pos);
        std::string::size_type url_pos = std::min(http_pos, https_pos);
        if (http_pos == std::string::npos)
        {
            url_pos = https_pos;
        }
        else if (https_pos == std::string::npos)
        {
            url_pos = http_pos;
        }

        if (url_pos == std::string::npos)
        {
            return false;
        }

        std::string::size_type url_end = text.find_first_of(" \n\r\t<>\"'", url_pos);
        if (url_end == std::string::npos)
        {
            url_end = text.size();
        }

        if (tasiaExtractImagePreviewFromURL(text.substr(url_pos, url_end - url_pos), preview))
        {
            return true;
        }
        search_pos = url_end;
    }
    return false;
}

bool tasiaFindFirstYouTubePreview(const std::string& text, TasiaYouTubePreview& preview)
{
    std::string::size_type search_pos = 0;
    while (search_pos < text.size())
    {
        static const char* YOUTUBE_CANDIDATES[] =
        {
            "https://",
            "http://",
            "www.youtube.com/",
            "youtube.com/",
            "m.youtube.com/",
            "youtu.be/",
            "www.youtube-nocookie.com/",
            "youtube-nocookie.com/"
        };

        std::string::size_type candidate_pos = std::string::npos;
        for (const char* candidate : YOUTUBE_CANDIDATES)
        {
            std::string::size_type pos = text.find(candidate, search_pos);
            if (pos != std::string::npos && (candidate_pos == std::string::npos || pos < candidate_pos))
            {
                candidate_pos = pos;
            }
        }

        if (candidate_pos == std::string::npos)
        {
            return false;
        }

        std::string::size_type url_end = text.find_first_of(" \n\r\t<>\"'", candidate_pos);
        if (url_end == std::string::npos)
        {
            url_end = text.size();
        }

        if (tasiaExtractYouTubePreviewFromURL(text.substr(candidate_pos, url_end - candidate_pos), preview))
        {
            return true;
        }
        search_pos = url_end;
    }
    return false;
}

TasiaImagePreviewPanel::TasiaImagePreviewPanel(const TasiaImagePreview& preview)
    : LLPanel(makeParams())
    , mURL(preview.url)
{
    LLMediaCtrl::Params media_params;
    media_params.name = "tasia_image_preview_media";
    media_params.rect = LLRect(10, 178, 330, 28);
    media_params.start_url = mURL;
    media_params.border_visible = true;
    media_params.focus_on_click = false;
    media_params.trusted_content = false;
    mMedia = LLUICtrlFactory::create<LLMediaCtrl>(media_params);
    mMedia->setTakeFocusOnClick(false);
    addChild(mMedia);

    LLTextBox::Params url_params;
    url_params.name = "tasia_image_preview_url";
    url_params.rect = LLRect(10, 24, 330, 6);
    url_params.initial_value = LLSD(mURL);
    url_params.use_ellipses = true;
    mURLText = LLUICtrlFactory::create<LLTextBox>(url_params);
    addChild(mURLText);

    LLButton::Params open_params;
    open_params.name = "tasia_image_preview_open";
    open_params.label = "Open";
    open_params.rect = LLRect(340, 24, 420, 6);
    mOpenButton = LLUICtrlFactory::create<LLButton>(open_params);
    mOpenButton->setClickedCallback([this](LLUICtrl*, const LLSD&) { openInViewer(); });
    addChild(mOpenButton);
}

void TasiaImagePreviewPanel::reshape(S32 width, S32 height, bool called_from_parent)
{
    LLPanel::reshape(width, height, called_from_parent);
    if (mMedia)
    {
        mMedia->setRect(LLRect(10, height - 10, llmax(120, width - 130), 28));
    }
    if (mURLText)
    {
        mURLText->setRect(LLRect(10, 24, llmax(120, width - 130), 6));
    }
    if (mOpenButton)
    {
        mOpenButton->setRect(LLRect(width - 110, height - 34, width - 10, height - 58));
    }
}

void TasiaImagePreviewPanel::openInViewer()
{
    LLFloaterWebContent::Params params;
    params.url = mURL;
    params.show_chrome = false;
    params.trusted_content = true;
    params.allow_address_entry = false;
    params.clean_browser = true;
    LLFloaterReg::showInstance("web_content", params);
}

void TasiaImagePreviewPanel::openURL()
{
    LLWeb::loadURLExternal(mURL);
}

TasiaYouTubePreviewPanel::TasiaYouTubePreviewPanel(const TasiaYouTubePreview& preview)
    : LLPanel(makeParams())
    , mPlayerURL(preview.player_url)
    , mPageURL(preview.page_url)
{
    LLMediaCtrl::Params media_params;
    media_params.name = "tasia_youtube_preview_media";
    media_params.rect = LLRect(10, 178, 330, 28);
    media_params.start_url = mPlayerURL;
    media_params.border_visible = true;
    media_params.focus_on_click = false;
    media_params.trusted_content = false;
    mMedia = LLUICtrlFactory::create<LLMediaCtrl>(media_params);
    mMedia->setTakeFocusOnClick(false);
    addChild(mMedia);

    LLTextBox::Params title_params;
    title_params.name = "tasia_youtube_preview_title";
    title_params.rect = LLRect(10, 24, 330, 6);
    title_params.initial_value = LLSD("YouTube");
    title_params.use_ellipses = true;
    mTitle = LLUICtrlFactory::create<LLTextBox>(title_params);
    addChild(mTitle);

    LLButton::Params open_params;
    open_params.name = "tasia_youtube_preview_open";
    open_params.label = "Open in YouTube";
    open_params.rect = LLRect(340, 44, 420, 26);
    mOpenButton = LLUICtrlFactory::create<LLButton>(open_params);
    mOpenButton->setClickedCallback([this](LLUICtrl*, const LLSD&) { openExternal(); });
    addChild(mOpenButton);

    LLButton::Params play_params;
    play_params.name = "tasia_youtube_preview_play";
    play_params.label = "Play In-World";
    play_params.rect = LLRect(340, 24, 420, 6);
    mPlayButton = LLUICtrlFactory::create<LLButton>(play_params);
    mPlayButton->setClickedCallback([this](LLUICtrl*, const LLSD&) { openInViewer(); });
    addChild(mPlayButton);
}

void TasiaYouTubePreviewPanel::reshape(S32 width, S32 height, bool called_from_parent)
{
    LLPanel::reshape(width, height, called_from_parent);
    if (mMedia)
    {
        mMedia->setRect(LLRect(10, height - 10, llmax(120, width - 130), 28));
    }
    if (mTitle)
    {
        mTitle->setRect(LLRect(10, 24, llmax(120, width - 130), 6));
    }
    if (mOpenButton)
    {
        mOpenButton->setRect(LLRect(width - 110, height - 54, width - 10, height - 78));
    }
    if (mPlayButton)
    {
        mPlayButton->setRect(LLRect(width - 110, height - 34, width - 10, height - 58));
    }
}

void TasiaYouTubePreviewPanel::openInViewer()
{
    LLFloaterWebContent::Params params;
    params.url = mPlayerURL;
    params.show_chrome = false;
    params.trusted_content = true;
    params.allow_address_entry = false;
    params.clean_browser = true;
    LLFloaterReg::showInstance("web_content", params);
}

void TasiaYouTubePreviewPanel::openExternal()
{
    LLWeb::loadURLExternal(mPageURL);
}

TasiaGiphyPreviewPanel::TasiaGiphyPreviewPanel(const TasiaGiphyPreview& preview)
    : LLPanel(makeParams())
    , mMediaURL(preview.media_url)
    , mPageURL(preview.page_url)
{
    LLMediaCtrl::Params media_params;
    media_params.name = "tasia_giphy_preview_media";
    media_params.rect = LLRect(10, 178, 330, 28);
    media_params.start_url = mMediaURL;
    media_params.border_visible = true;
    media_params.focus_on_click = false;
    media_params.trusted_content = false;
    mMedia = LLUICtrlFactory::create<LLMediaCtrl>(media_params);
    mMedia->setTakeFocusOnClick(false);
    addChild(mMedia);

    LLTextBox::Params title_params;
    title_params.name = "tasia_giphy_preview_title";
    title_params.rect = LLRect(10, 24, 330, 6);
    title_params.initial_value = LLSD("GIPHY");
    title_params.use_ellipses = true;
    mTitle = LLUICtrlFactory::create<LLTextBox>(title_params);
    addChild(mTitle);

    LLTextBox::Params url_params;
    url_params.name = "tasia_giphy_preview_url";
    url_params.rect = LLRect(10, 44, 330, 26);
    url_params.initial_value = LLSD(mPageURL);
    url_params.use_ellipses = true;
    mURLText = LLUICtrlFactory::create<LLTextBox>(url_params);
    addChild(mURLText);

    LLTextBox::Params powered_params;
    powered_params.name = "tasia_giphy_preview_powered";
    powered_params.rect = LLRect(10, 64, 330, 46);
    powered_params.initial_value = LLSD("Powered by GIPHY");
    mPoweredBy = LLUICtrlFactory::create<LLTextBox>(powered_params);
    addChild(mPoweredBy);

    LLButton::Params open_params;
    open_params.name = "tasia_giphy_preview_open";
    open_params.label = "Open";
    open_params.rect = LLRect(340, 24, 420, 6);
    mOpenButton = LLUICtrlFactory::create<LLButton>(open_params);
    mOpenButton->setClickedCallback([this](LLUICtrl*, const LLSD&) { openInViewer(); });
    addChild(mOpenButton);
}

void TasiaGiphyPreviewPanel::reshape(S32 width, S32 height, bool called_from_parent)
{
    LLPanel::reshape(width, height, called_from_parent);
    if (mMedia)
    {
        mMedia->setRect(LLRect(10, height - 10, llmax(120, width - 130), 28));
    }
    if (mTitle)
    {
        mTitle->setRect(LLRect(10, 24, llmax(120, width - 130), 6));
    }
    if (mURLText)
    {
        mURLText->setRect(LLRect(10, 44, llmax(120, width - 130), 26));
    }
    if (mPoweredBy)
    {
        mPoweredBy->setRect(LLRect(10, 64, llmax(120, width - 130), 46));
    }
    if (mOpenButton)
    {
        mOpenButton->setRect(LLRect(width - 110, height - 34, width - 10, height - 58));
    }
}

void TasiaGiphyPreviewPanel::openInViewer()
{
    LLFloaterWebContent::Params params;
    params.url = mPageURL;
    params.show_chrome = false;
    params.trusted_content = true;
    params.allow_address_entry = false;
    params.clean_browser = true;
    LLFloaterReg::showInstance("web_content", params);
}

void TasiaGiphyPreviewPanel::openURL()
{
    LLWeb::loadURLExternal(mPageURL);
}

// static
LLPanel::Params TasiaImagePreviewPanel::makeParams()
{
    LLPanel::Params params;
    params.name = "tasia_image_preview";
    params.rect = LLRect(0, 140, 460, 0);
    params.mouse_opaque = true;
    params.background_visible = true;
    params.has_border = true;
    return params;
}

// static
LLPanel::Params TasiaYouTubePreviewPanel::makeParams()
{
    LLPanel::Params params;
    params.name = "tasia_youtube_preview";
    params.rect = LLRect(0, 66, 530, 0);
    params.mouse_opaque = true;
    params.background_visible = true;
    params.has_border = true;
    return params;
}

// static
LLPanel::Params TasiaGiphyPreviewPanel::makeParams()
{
    LLPanel::Params params;
    params.name = "tasia_giphy_preview";
    params.rect = LLRect(0, 188, 440, 0);
    params.mouse_opaque = true;
    params.background_visible = true;
    params.has_border = true;
    return params;
}

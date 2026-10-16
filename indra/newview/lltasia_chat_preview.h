/**
 * @file lltasia_chat_preview.h
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

#ifndef LL_TASIA_CHAT_PREVIEW_H
#define LL_TASIA_CHAT_PREVIEW_H

#include "llpanel.h"
#include "llstring.h"

#include <string>

class LLButton;
class LLMediaCtrl;
class LLTextBox;

struct TasiaGiphyPreview
{
    std::string id;
    std::string page_url;
    std::string media_url;
};

struct TasiaImagePreview
{
    std::string url;
};

struct TasiaYouTubePreview
{
    std::string video_id;
    std::string page_url;
    std::string player_url;
};

bool tasiaFindFirstGiphyPreview(const std::string& text, TasiaGiphyPreview& preview);
bool tasiaFindFirstImagePreview(const std::string& text, TasiaImagePreview& preview);
bool tasiaFindFirstYouTubePreview(const std::string& text, TasiaYouTubePreview& preview);

class TasiaImagePreviewPanel : public LLPanel
{
public:
    TasiaImagePreviewPanel(const TasiaImagePreview& preview);
    void reshape(S32 width, S32 height, bool called_from_parent = true) override;
    void openInViewer();
    void openURL();

private:
    static LLPanel::Params makeParams();
    std::string mURL;
    LLMediaCtrl* mMedia = nullptr;
    LLTextBox* mURLText = nullptr;
    LLButton* mOpenButton = nullptr;
};

class TasiaYouTubePreviewPanel : public LLPanel
{
public:
    TasiaYouTubePreviewPanel(const TasiaYouTubePreview& preview);
    void reshape(S32 width, S32 height, bool called_from_parent = true) override;
    void openInViewer();
    void openExternal();

private:
    static LLPanel::Params makeParams();
    std::string mPlayerURL;
    std::string mPageURL;
    LLMediaCtrl* mMedia = nullptr;
    LLTextBox* mTitle = nullptr;
    LLButton* mOpenButton = nullptr;
    LLButton* mPlayButton = nullptr;
};

class TasiaGiphyPreviewPanel : public LLPanel
{
public:
    TasiaGiphyPreviewPanel(const TasiaGiphyPreview& preview);
    void reshape(S32 width, S32 height, bool called_from_parent = true) override;
    void openInViewer();
    void openURL();

private:
    static LLPanel::Params makeParams();
    std::string mMediaURL;
    std::string mPageURL;
    LLMediaCtrl* mMedia = nullptr;
    LLTextBox* mTitle = nullptr;
    LLTextBox* mURLText = nullptr;
    LLTextBox* mPoweredBy = nullptr;
    LLButton* mOpenButton = nullptr;
};

#endif // LL_TASIA_CHAT_PREVIEW_H

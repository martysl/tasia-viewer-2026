/**
 * @file llfloatertasiafeed.h
 * @brief TasiaFeed snapshot upload floater
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

#ifndef LL_FLOATER_TASIAFEED_H
#define LL_FLOATER_TASIAFEED_H

#include "llfloater.h"
#include "llsnapshotlivepreview.h"

class LLLineEditor;
class LLComboBox;
class LLCheckBoxCtrl;

class LLFloaterTasiaFeed : public LLFloater
{
public:
    LLFloaterTasiaFeed(const LLSD& key);
    ~LLFloaterTasiaFeed();

    bool postBuild() override;
    void onOpen(const LLSD& key) override;
    void draw() override;

    void refreshControls();

private:
    void onSend();
    void onCancel();
    void uploadCallback(bool success, const LLSD& response);

    LLSnapshotLivePreview* getPreviewView();

    LLLineEditor* mTitleEditor;
    LLLineEditor* mDescriptionEditor;
    LLComboBox*   mVisibilityCombo;
    LLComboBox*   mMaturityCombo;
    bool          mUploading;
    std::string   mResultURL;
    std::string   mResultMessage;
    LLSD          mResultData;
};

class LLTasiaFeedPanel : public LLPanel
{
public:
    LLTasiaFeedPanel();
    bool postBuild() override;

private:
    void onSend();
    void uploadCallback(bool success, const LLSD& response);

    LLLineEditor* mTitleEditor;
    LLLineEditor* mDescriptionEditor;
    LLComboBox*   mVisibilityCombo;
    LLComboBox*   mMaturityCombo;
    bool          mUploading;
    std::string   mResultURL;
    std::string   mResultMessage;
};

#endif

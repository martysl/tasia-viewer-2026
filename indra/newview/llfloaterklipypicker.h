/**
 * @file llfloaterklipypicker.h
 * @brief KLIPY picker floater for Tasia Viewer chat GIF/Clip features.
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

#ifndef LL_FLOATER_KLIPY_PICKER_H
#define LL_FLOATER_KLIPY_PICKER_H

#include "llfloater.h"
#include "llklipyclient.h"
#include "llhandle.h"

#include <boost/function.hpp>
#include <string>

class LLButton;
class LLLineEditor;
class LLScrollListCtrl;
class LLTextBox;
class LLComboBox;

class LLFloaterKlipyPicker : public LLFloater
{
public:
    typedef boost::function<void(const std::string& url)> selection_callback_t;

    LLFloaterKlipyPicker(const LLSD& key);
    bool postBuild() override;
    void onOpen(const LLSD& key) override;
    void onClose(bool app_quitting) override;

    static LLFloaterKlipyPicker* show(selection_callback_t callback = selection_callback_t());

private:
    void setSelectionCallback(selection_callback_t callback);
    void requestSearch();
    void requestTrending();
    void populateResults(const LLKlipyClient::results_t& results);
    void setStatus(const std::string& status);
    void setLoading(bool loading);
    void refreshControls();
    std::string getSelectedURL() const;
    LLKlipyClient::ContentType getSelectedContentType() const;

    void onSearchClicked();
    void onTrendingClicked();
    void onUseClicked();
    void onResultSelected();
    void onContentTypeChanged();

    static void onKlipyResults(LLHandle<LLFloater> handle,
                               S32 request_id,
                               bool success,
                               const std::string& message,
                               const LLKlipyClient::results_t& results);

    LLLineEditor* mSearchEditor = nullptr;
    LLScrollListCtrl* mResultsList = nullptr;
    LLTextBox* mStatusText = nullptr;
    LLButton* mSearchButton = nullptr;
    LLButton* mTrendingButton = nullptr;
    LLButton* mUseButton = nullptr;
    LLComboBox* mContentTypeCombo = nullptr;
    selection_callback_t mSelectionCallback;
    LLKlipyClient::ContentType mContentType = LLKlipyClient::CONTENT_GIF;
    bool mLoading = false;
    S32 mRequestId = 0;
};

#endif // LL_FLOATER_KLIPY_PICKER_H

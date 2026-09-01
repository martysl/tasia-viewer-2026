/**
 * @file llfloaterklipypicker.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llfloaterklipypicker.h"

#include "llbutton.h"
#include "llclipboard.h"
#include "llcombobox.h"
#include "llfloaterreg.h"
#include "lllineeditor.h"
#include "llscrolllistctrl.h"
#include "llstring.h"
#include "lltextbox.h"

#include <boost/bind.hpp>

LLFloaterKlipyPicker::LLFloaterKlipyPicker(const LLSD& key)
    : LLFloater(key)
{
}

bool LLFloaterKlipyPicker::postBuild()
{
    mSearchEditor = getChild<LLLineEditor>("search_edit");
    mResultsList = getChild<LLScrollListCtrl>("results_list");
    mStatusText = getChild<LLTextBox>("status_text");
    mSearchButton = getChild<LLButton>("search_btn");
    mTrendingButton = getChild<LLButton>("trending_btn");
    mUseButton = getChild<LLButton>("use_btn");
    mContentTypeCombo = getChild<LLComboBox>("content_type_combo");

    mSearchButton->setClickedCallback(boost::bind(&LLFloaterKlipyPicker::onSearchClicked, this));
    mTrendingButton->setClickedCallback(boost::bind(&LLFloaterKlipyPicker::onTrendingClicked, this));
    mUseButton->setClickedCallback(boost::bind(&LLFloaterKlipyPicker::onUseClicked, this));
    mResultsList->setCommitCallback(boost::bind(&LLFloaterKlipyPicker::onResultSelected, this));
    if (mContentTypeCombo)
    {
        mContentTypeCombo->setCommitCallback(boost::bind(&LLFloaterKlipyPicker::onContentTypeChanged, this));
    }

    setStatus("Search KLIPY or load trending content.");
    refreshControls();
    return true;
}

void LLFloaterKlipyPicker::onOpen(const LLSD& key)
{
    if (mResultsList && mResultsList->isEmpty())
    {
        requestTrending();
    }
}

void LLFloaterKlipyPicker::onClose(bool app_quitting)
{
    mSelectionCallback = selection_callback_t();
}

// static
LLFloaterKlipyPicker* LLFloaterKlipyPicker::show(selection_callback_t callback)
{
    LLFloaterKlipyPicker* floater = LLFloaterReg::showTypedInstance<LLFloaterKlipyPicker>("klipy_picker");
    if (floater)
    {
        floater->setSelectionCallback(callback);
    }
    return floater;
}

void LLFloaterKlipyPicker::setSelectionCallback(selection_callback_t callback)
{
    mSelectionCallback = callback;
}

LLKlipyClient::ContentType LLFloaterKlipyPicker::getSelectedContentType() const
{
    if (!mContentTypeCombo)
    {
        return LLKlipyClient::CONTENT_GIF;
    }

    const std::string value = mContentTypeCombo->getSelectedValue().asString();
    if (value == "clips")
    {
        return LLKlipyClient::CONTENT_CLIP;
    }
    if (value == "stickers")
    {
        return LLKlipyClient::CONTENT_STICKER;
    }
    if (value == "memes")
    {
        return LLKlipyClient::CONTENT_MEME;
    }
    return LLKlipyClient::CONTENT_GIF;
}

void LLFloaterKlipyPicker::requestSearch()
{
    std::string query = mSearchEditor ? mSearchEditor->getValue().asString() : std::string();
    LLStringUtil::trim(query);
    if (query.empty())
    {
        setStatus("Enter a search term.");
        refreshControls();
        return;
    }

    mContentType = getSelectedContentType();
    setLoading(true);
    setStatus("Searching KLIPY...");
    const S32 request_id = ++mRequestId;
    LLKlipyClient::search(query, mContentType,
        boost::bind(&LLFloaterKlipyPicker::onKlipyResults, getHandle(), request_id, _1, _2, _3));
}

void LLFloaterKlipyPicker::requestTrending()
{
    mContentType = getSelectedContentType();
    setLoading(true);
    setStatus("Loading trending content...");
    const S32 request_id = ++mRequestId;
    LLKlipyClient::trending(mContentType,
        boost::bind(&LLFloaterKlipyPicker::onKlipyResults, getHandle(), request_id, _1, _2, _3));
}

void LLFloaterKlipyPicker::populateResults(const LLKlipyClient::results_t& results)
{
    if (!mResultsList)
    {
        return;
    }

    mResultsList->deleteAllItems();
    for (LLKlipyClient::results_t::const_iterator it = results.begin(); it != results.end(); ++it)
    {
        LLScrollListItem::Params item;
        item.value(it->page_url);
        item.columns.add().column("title").value(it->description.empty() ? it->id : it->description);
        item.columns.add().column("url").value(it->page_url);
        mResultsList->addRow(item);
    }
}

void LLFloaterKlipyPicker::setStatus(const std::string& status)
{
    if (mStatusText)
    {
        mStatusText->setText(status);
    }
}

void LLFloaterKlipyPicker::setLoading(bool loading)
{
    mLoading = loading;
    refreshControls();
}

void LLFloaterKlipyPicker::refreshControls()
{
    if (mSearchButton)
    {
        mSearchButton->setEnabled(!mLoading);
    }
    if (mTrendingButton)
    {
        mTrendingButton->setEnabled(!mLoading);
    }
    if (mUseButton)
    {
        mUseButton->setEnabled(!mLoading && !getSelectedURL().empty());
    }
    if (mSearchEditor)
    {
        mSearchEditor->setEnabled(!mLoading);
    }
    if (mContentTypeCombo)
    {
        mContentTypeCombo->setEnabled(!mLoading);
    }
}

std::string LLFloaterKlipyPicker::getSelectedURL() const
{
    if (!mResultsList || !mResultsList->getFirstSelected())
    {
        return std::string();
    }
    return mResultsList->getSelectedValue().asString();
}

void LLFloaterKlipyPicker::onSearchClicked()
{
    requestSearch();
}

void LLFloaterKlipyPicker::onTrendingClicked()
{
    requestTrending();
}

void LLFloaterKlipyPicker::onContentTypeChanged()
{
    if (mResultsList && !mResultsList->isEmpty())
    {
        mResultsList->deleteAllItems();
    }
    setStatus("Select a content type, then search or load trending.");
    refreshControls();
}

void LLFloaterKlipyPicker::onUseClicked()
{
    const std::string url = getSelectedURL();
    if (url.empty())
    {
        return;
    }

    if (mSelectionCallback)
    {
        mSelectionCallback(url);
        closeFloater();
        return;
    }

    LLWString wide_url = utf8str_to_wstring(url);
    LLClipboard::instance().copyToClipboard(wide_url, 0, static_cast<S32>(wide_url.size()));
    setStatus("KLIPY URL copied to clipboard.");
}

void LLFloaterKlipyPicker::onResultSelected()
{
    refreshControls();
}

// static
void LLFloaterKlipyPicker::onKlipyResults(LLHandle<LLFloater> handle,
                                          S32 request_id,
                                          bool success,
                                          const std::string& message,
                                          const LLKlipyClient::results_t& results)
{
    LLFloaterKlipyPicker* floater = dynamic_cast<LLFloaterKlipyPicker*>(handle.get());
    if (!floater || floater->mRequestId != request_id)
    {
        return;
    }

    floater->setLoading(false);
    if (success)
    {
        floater->populateResults(results);
        floater->setStatus(message.empty() ? "Select a result. Powered by KLIPY." : message);
    }
    else
    {
        floater->populateResults(LLKlipyClient::results_t());
        floater->setStatus(message.empty() ? "KLIPY request failed." : message);
    }
    floater->refreshControls();
}

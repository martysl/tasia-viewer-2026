/**
 * @file llfloatergiphypicker.cpp
 * @brief GIPHY picker floater for Tasia Viewer chat GIF features.
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

#include "llfloatergiphypicker.h"

#include "llbutton.h"
#include "llclipboard.h"
#include "llcombobox.h"
#include "llfloaterreg.h"
#include "lllineeditor.h"
#include "llscrolllistctrl.h"
#include "llstring.h"
#include "lltextbox.h"

#include <boost/bind.hpp>

LLFloaterGiphyPicker::LLFloaterGiphyPicker(const LLSD& key)
    : LLFloater(key)
{
}

bool LLFloaterGiphyPicker::postBuild()
{
    mSearchEditor = getChild<LLLineEditor>("search_edit");
    mResultsList = getChild<LLScrollListCtrl>("results_list");
    mCategoryList = getChild<LLScrollListCtrl>("category_list");
    mSuggestionList = getChild<LLScrollListCtrl>("suggestion_list");
    mStatusText = getChild<LLTextBox>("status_text");
    mSearchButton = getChild<LLButton>("search_btn");
    mTrendingButton = getChild<LLButton>("trending_btn");
    mStickersButton = getChild<LLButton>("stickers_btn");
    mEmojiButton = getChild<LLButton>("emoji_btn");
    mRandomButton = getChild<LLButton>("random_btn");
    mTranslateButton = getChild<LLButton>("translate_btn");
    mUseButton = getChild<LLButton>("use_btn");

    mSearchButton->setClickedCallback(boost::bind(&LLFloaterGiphyPicker::onSearchClicked, this));
    mTrendingButton->setClickedCallback(boost::bind(&LLFloaterGiphyPicker::onTrendingClicked, this));
    mStickersButton->setClickedCallback(boost::bind(&LLFloaterGiphyPicker::onStickersClicked, this));
    mEmojiButton->setClickedCallback(boost::bind(&LLFloaterGiphyPicker::onEmojiClicked, this));
    mRandomButton->setClickedCallback(boost::bind(&LLFloaterGiphyPicker::onRandomClicked, this));
    mTranslateButton->setClickedCallback(boost::bind(&LLFloaterGiphyPicker::onTranslateClicked, this));
    mUseButton->setClickedCallback(boost::bind(&LLFloaterGiphyPicker::onUseClicked, this));
    mResultsList->setCommitCallback(boost::bind(&LLFloaterGiphyPicker::onResultSelected, this));
    mCategoryList->setCommitCallback(boost::bind(&LLFloaterGiphyPicker::onCategorySelected, this));
    mSuggestionList->setCommitCallback(boost::bind(&LLFloaterGiphyPicker::onSuggestionSelected, this));
    mSearchEditor->setKeystrokeCallback([this](LLLineEditor*, void*) { onSearchKeystroke(); }, NULL);

    setStatus("Search GIPHY or load trending GIFs.");
    refreshControls();
    return true;
}

void LLFloaterGiphyPicker::onOpen(const LLSD& key)
{
    if (mResultsList && mResultsList->isEmpty())
    {
        requestTrending();
    }
    loadCategories();
}

void LLFloaterGiphyPicker::onClose(bool app_quitting)
{
    mSelectionCallback = selection_callback_t();
}

// static
LLFloaterGiphyPicker* LLFloaterGiphyPicker::show(selection_callback_t callback)
{
    LLFloaterGiphyPicker* floater = LLFloaterReg::showTypedInstance<LLFloaterGiphyPicker>("giphy_picker");
    if (floater)
    {
        floater->setSelectionCallback(callback);
    }
    return floater;
}

void LLFloaterGiphyPicker::setSelectionCallback(selection_callback_t callback)
{
    mSelectionCallback = callback;
}

void LLFloaterGiphyPicker::requestSearch()
{
    std::string query = mSearchEditor ? mSearchEditor->getValue().asString() : std::string();
    LLStringUtil::trim(query);
    if (query.empty())
    {
        setStatus("Enter a search term.");
        refreshControls();
        return;
    }

    mMode = MODE_SEARCH;
    setLoading(true);
    setStatus("Searching GIPHY...");
    const S32 request_id = ++mRequestId;
    LLGiphyClient::search(query,
        boost::bind(&LLFloaterGiphyPicker::onGiphyResults, getHandle(), request_id, _1, _2, _3));
}

void LLFloaterGiphyPicker::requestTrending()
{
    mMode = MODE_TRENDING;
    setLoading(true);
    setStatus("Loading trending GIFs...");
    const S32 request_id = ++mRequestId;
    LLGiphyClient::trending(
        boost::bind(&LLFloaterGiphyPicker::onGiphyResults, getHandle(), request_id, _1, _2, _3));
}

void LLFloaterGiphyPicker::requestStickers()
{
    mMode = MODE_STICKERS;
    setLoading(true);
    setStatus("Loading stickers...");
    const S32 request_id = ++mRequestId;

    std::string query = mSearchEditor ? mSearchEditor->getValue().asString() : std::string();
    LLStringUtil::trim(query);
    if (!query.empty())
    {
        LLGiphyClient::stickerSearch(query,
            boost::bind(&LLFloaterGiphyPicker::onGiphyResults, getHandle(), request_id, _1, _2, _3));
    }
    else
    {
        LLGiphyClient::stickerTrending(
            boost::bind(&LLFloaterGiphyPicker::onGiphyResults, getHandle(), request_id, _1, _2, _3));
    }
}

void LLFloaterGiphyPicker::requestEmoji()
{
    mMode = MODE_EMOJI;
    setLoading(true);
    setStatus("Loading emoji...");
    const S32 request_id = ++mRequestId;
    LLGiphyClient::emoji(
        boost::bind(&LLFloaterGiphyPicker::onGiphyResults, getHandle(), request_id, _1, _2, _3));
}

void LLFloaterGiphyPicker::requestRandom()
{
    mMode = MODE_RANDOM;
    setLoading(true);
    setStatus("Rolling a random GIF...");
    const S32 request_id = ++mRequestId;

    std::string query = mSearchEditor ? mSearchEditor->getValue().asString() : std::string();
    LLStringUtil::trim(query);
    LLGiphyClient::random(query,
        boost::bind(&LLFloaterGiphyPicker::onGiphyResults, getHandle(), request_id, _1, _2, _3));
}

void LLFloaterGiphyPicker::requestTranslate()
{
    mMode = MODE_TRANSLATE;
    setLoading(true);
    setStatus("Translating text to GIF...");
    const S32 request_id = ++mRequestId;

    std::string query = mSearchEditor ? mSearchEditor->getValue().asString() : std::string();
    LLStringUtil::trim(query);
    LLGiphyClient::translate(query,
        boost::bind(&LLFloaterGiphyPicker::onGiphyResults, getHandle(), request_id, _1, _2, _3));
}

void LLFloaterGiphyPicker::requestCategory(const std::string& category)
{
    mMode = MODE_CATEGORY;
    setLoading(true);
    setStatus("Loading category: " + category);
    const S32 request_id = ++mRequestId;
    LLGiphyClient::categoryGifs(category,
        boost::bind(&LLFloaterGiphyPicker::onGiphyResults, getHandle(), request_id, _1, _2, _3));
}

void LLFloaterGiphyPicker::loadCategories()
{
    if (mCategoriesLoaded)
    {
        return;
    }
    LLGiphyClient::categories(
        boost::bind(&LLFloaterGiphyPicker::onCategoriesLoaded, getHandle(), _1, _2, _3));
}

void LLFloaterGiphyPicker::loadSuggestions(const std::string& query)
{
    std::string trimmed = query;
    LLStringUtil::trim(trimmed);
    if (trimmed.empty())
    {
        if (mSuggestionList)
        {
            mSuggestionList->deleteAllItems();
        }
        return;
    }

    const S32 request_id = ++mSuggestionRequestId;
    LLGiphyClient::searchSuggestions(trimmed,
        boost::bind(&LLFloaterGiphyPicker::onSuggestionsLoaded, getHandle(), request_id, _1, _2, _3));
}

void LLFloaterGiphyPicker::populateResults(const LLGiphyClient::results_t& results)
{
    if (!mResultsList)
    {
        return;
    }

    mResultsList->deleteAllItems();
    for (LLGiphyClient::results_t::const_iterator it = results.begin(); it != results.end(); ++it)
    {
        LLScrollListItem::Params item;
        item.value(it->page_url);
        item.columns.add().column("title").value(it->title.empty() ? it->id : it->title);
        item.columns.add().column("url").value(it->page_url);
        mResultsList->addRow(item);
    }
}

void LLFloaterGiphyPicker::populateCategories(const LLGiphyClient::categories_t& categories)
{
    if (!mCategoryList)
    {
        return;
    }

    mCategoryList->deleteAllItems();
    for (LLGiphyClient::categories_t::const_iterator it = categories.begin(); it != categories.end(); ++it)
    {
        LLScrollListItem::Params item;
        item.value(it->name_encoded);
        item.columns.add().column("category").value(it->name);
        mCategoryList->addRow(item);
    }
    mCategoriesLoaded = true;
}

void LLFloaterGiphyPicker::populateSuggestions(const LLGiphyClient::suggestions_t& suggestions)
{
    if (!mSuggestionList)
    {
        return;
    }

    mSuggestionList->deleteAllItems();
    for (LLGiphyClient::suggestions_t::const_iterator it = suggestions.begin(); it != suggestions.end(); ++it)
    {
        LLScrollListItem::Params item;
        item.value(it->term);
        item.columns.add().column("term").value(it->term);
        mSuggestionList->addRow(item);
    }
}

void LLFloaterGiphyPicker::setStatus(const std::string& status)
{
    if (mStatusText)
    {
        mStatusText->setText(status);
    }
}

void LLFloaterGiphyPicker::setLoading(bool loading)
{
    mLoading = loading;
    refreshControls();
}

void LLFloaterGiphyPicker::refreshControls()
{
    if (mSearchButton)
    {
        mSearchButton->setEnabled(!mLoading);
    }
    if (mTrendingButton)
    {
        mTrendingButton->setEnabled(!mLoading);
    }
    if (mStickersButton)
    {
        mStickersButton->setEnabled(!mLoading);
    }
    if (mEmojiButton)
    {
        mEmojiButton->setEnabled(!mLoading);
    }
    if (mRandomButton)
    {
        mRandomButton->setEnabled(!mLoading);
    }
    if (mTranslateButton)
    {
        mTranslateButton->setEnabled(!mLoading);
    }
    if (mUseButton)
    {
        mUseButton->setEnabled(!mLoading && !getSelectedURL().empty());
    }
    if (mSearchEditor)
    {
        mSearchEditor->setEnabled(!mLoading);
    }
    if (mCategoryList)
    {
        mCategoryList->setEnabled(!mLoading);
    }
    if (mSuggestionList)
    {
        mSuggestionList->setEnabled(!mLoading);
    }
}

std::string LLFloaterGiphyPicker::getSelectedURL() const
{
    if (!mResultsList || !mResultsList->getFirstSelected())
    {
        return std::string();
    }
    return mResultsList->getSelectedValue().asString();
}

void LLFloaterGiphyPicker::onSearchClicked()
{
    requestSearch();
}

void LLFloaterGiphyPicker::onTrendingClicked()
{
    requestTrending();
}

void LLFloaterGiphyPicker::onStickersClicked()
{
    requestStickers();
}

void LLFloaterGiphyPicker::onEmojiClicked()
{
    requestEmoji();
}

void LLFloaterGiphyPicker::onRandomClicked()
{
    requestRandom();
}

void LLFloaterGiphyPicker::onTranslateClicked()
{
    requestTranslate();
}

void LLFloaterGiphyPicker::onUseClicked()
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
    setStatus("GIF URL copied to clipboard.");
}

void LLFloaterGiphyPicker::onResultSelected()
{
    refreshControls();
}

void LLFloaterGiphyPicker::onCategorySelected()
{
    if (!mCategoryList || !mCategoryList->getFirstSelected())
    {
        return;
    }
    requestCategory(mCategoryList->getSelectedValue().asString());
}

void LLFloaterGiphyPicker::onSuggestionSelected()
{
    if (!mSuggestionList || !mSuggestionList->getFirstSelected())
    {
        return;
    }
    const std::string term = mSuggestionList->getSelectedValue().asString();
    if (mSearchEditor)
    {
        mSearchEditor->setText(term);
    }
    requestSearch();
}

void LLFloaterGiphyPicker::onSearchKeystroke()
{
    const std::string query = mSearchEditor ? mSearchEditor->getValue().asString() : std::string();
    loadSuggestions(query);
}

// static
void LLFloaterGiphyPicker::onGiphyResults(LLHandle<LLFloater> handle,
                                          S32 request_id,
                                          bool success,
                                          const std::string& message,
                                          const LLGiphyClient::results_t& results)
{
    LLFloaterGiphyPicker* floater = dynamic_cast<LLFloaterGiphyPicker*>(handle.get());
    if (!floater || floater->mRequestId != request_id)
    {
        return;
    }

    floater->setLoading(false);
    if (success)
    {
        floater->populateResults(results);
        floater->setStatus(message.empty() ? "Select a GIF. Powered by GIPHY." : message);
    }
    else
    {
        floater->populateResults(LLGiphyClient::results_t());
        floater->setStatus(message.empty() ? "GIPHY request failed." : message);
    }
    floater->refreshControls();
}

// static
void LLFloaterGiphyPicker::onCategoriesLoaded(LLHandle<LLFloater> handle,
                                              bool success,
                                              const std::string& message,
                                              const LLGiphyClient::categories_t& categories)
{
    LLFloaterGiphyPicker* floater = dynamic_cast<LLFloaterGiphyPicker*>(handle.get());
    if (!floater)
    {
        return;
    }

    if (success)
    {
        floater->populateCategories(categories);
    }
    else
    {
        LL_WARNS("GIPHY") << "Failed to load categories: " << message << LL_ENDL;
    }
}

// static
void LLFloaterGiphyPicker::onSuggestionsLoaded(LLHandle<LLFloater> handle,
                                               S32 request_id,
                                               bool success,
                                               const std::string& message,
                                               const LLGiphyClient::suggestions_t& suggestions)
{
    LLFloaterGiphyPicker* floater = dynamic_cast<LLFloaterGiphyPicker*>(handle.get());
    if (!floater || floater->mSuggestionRequestId != request_id)
    {
        return;
    }

    if (success)
    {
        floater->populateSuggestions(suggestions);
    }
}

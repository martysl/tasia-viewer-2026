/**
 * @file llfloatergridstatus.cpp
 * @brief Grid status floater - uses an embedded web browser to show Grid status info
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2017, Linden Research, Inc.
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
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llfloatergridstatus.h"

#include "llcallbacklist.h"
#include "llcorehttputil.h"
#include "llfloaterreg.h"
#include "llhttpconstants.h"
#include "llmd5.h"
#include "llmediactrl.h"
#include "llagent.h"
#include "lldir.h"
#include "lluri.h"
#include "llviewernetwork.h"
#include "llviewerregion.h"
#include "llsdserialize.h"
#include "lltoolbarview.h"
#include "llviewercontrol.h"
#include "llxmltree.h"

#include <algorithm>

std::map<std::pair<std::string, std::string>, std::string> LLFloaterGridStatus::sItemsMap;
std::set<std::string> LLFloaterGridStatus::sInitializedGrids;
const std::string DEFAULT_GRID_STATUS_URL = "http://status.secondlifegrid.net/";

namespace
{
bool isUsableGridStatusURL(const std::string& value)
{
    if (value.empty() || value.size() > 2048 || value.find('#') != std::string::npos ||
        std::any_of(value.begin(), value.end(), [](unsigned char c) { return c <= 0x20 || c == 0x5c; }))
        return false;
    const LLURI uri(value);
    const auto authority = uri.authority();
    if ((uri.scheme() != "http" && uri.scheme() != "https") || uri.hostName().empty() ||
        authority.find('@') != std::string::npos) return false;
    std::string port;
    if (!authority.empty() && authority.front() == '[')
    {
        const auto close_bracket = authority.find(']');
        if (close_bracket == std::string::npos) return false;
        const auto suffix = authority.substr(close_bracket + 1);
        if (!suffix.empty())
        {
            if (suffix.front() != ':') return false;
            port = suffix.substr(1);
        }
    }
    else
    {
        const auto colon = authority.rfind(':');
        if (colon != std::string::npos) port = authority.substr(colon + 1);
    }
    if (!port.empty())
    {
        if (port.size() > 5 || !std::all_of(port.begin(), port.end(),
                [](unsigned char c) { return c >= '0' && c <= '9'; })) return false;
        const auto number = std::stoul(port);
        if (number == 0 || number > 65535) return false;
    }
    else if ((!authority.empty() && authority.back() == ':') ||
             (authority.size() > 1 && authority.front() == '[' && authority.back() != ']')) return false;
    return true;
}

std::string getGridStatusEndpoint(bool rss)
{
    LLGridManager* manager = LLGridManager::getInstance();
    if (manager->isInSecondLife())
    {
        const std::string configured = rss ? gSavedSettings.getString("GridStatusRSS") :
                                             DEFAULT_GRID_STATUS_URL;
        return isUsableGridStatusURL(configured) ? configured : std::string();
    }

    if (!manager->isInOpenSim()) return {};
    const char* feature_key = rss ? "GridStatusRSS" : "GridStatus";
    const char* metadata_key = rss ? "grid_status_rss" : "grid_status";
    if (LLViewerRegion* region = gAgent.getRegion())
    {
        LLSD features;
        region->getSimulatorFeatures(features);
        const LLSD& extras = features["OpenSimExtras"];
        if (extras[feature_key].isString() && isUsableGridStatusURL(extras[feature_key].asString()))
            return extras[feature_key].asString();
    }

    const LLSD info = manager->getGridInfo(manager->getGrid());
    const std::string configured = info[metadata_key].asString();
    return isUsableGridStatusURL(configured) ? configured : std::string();
}

std::string getGridStatusRSSFilename(const std::string& grid)
{
    LLMD5 digest;
    digest.update(grid);
    digest.finalize();
    char hex_digest[MD5HEX_STR_SIZE];
    digest.hex_digest(hex_digest);
    return "grid_status_" + std::string(hex_digest) + ".xml";
}
}

LLFloaterGridStatus::LLFloaterGridStatus(const Params& key) :
    LLFloaterWebContent(key),
    mIsFirstUpdate(true)
{
}

bool LLFloaterGridStatus::postBuild()
{
    LLFloaterWebContent::postBuild();
    mWebBrowser->addObserver(this);

    return true;
}

void LLFloaterGridStatus::onOpen(const LLSD& key)
{
    Params p(key);
    p.trusted_content = LLGridManager::getInstance()->isInSecondLife();
    p.allow_address_entry = false;

    LLFloaterWebContent::onOpen(p);
    applyPreferredRect();
    if (mWebBrowser)
    {
        const std::string url = getGridStatusEndpoint(false);
        mWebBrowser->navigateTo(url.empty() ? "about:blank" : url,
                                std::string{HTTP_CONTENT_TEXT_HTML});
    }
}

void LLFloaterGridStatus::startGridStatusTimer()
{
    checkGridStatusRSS();
    doPeriodically(boost::bind(&LLFloaterGridStatus::checkGridStatusRSS), gSavedSettings.getF32("GridStatusUpdateDelay"));
}

bool LLFloaterGridStatus::checkGridStatusRSS()
{
    const std::string grid = LLGridManager::getInstance()->getGrid();
    const std::string url = getGridStatusEndpoint(true);
    if (!url.empty() && gToolBarView->hasCommand(LLCommandId("gridstatus")))
    {
        LLCoros::instance().launch("LLFloaterGridStatus::getGridStatusRSSCoro",
                [grid, url]() { getGridStatusRSSCoro(grid, url); });
    }
    return false;
}

void LLFloaterGridStatus::getGridStatusRSSCoro(const std::string& grid, const std::string& url)
{

    LLCore::HttpRequest::policy_t httpPolicy(LLCore::HttpRequest::DEFAULT_POLICY_ID);
    LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t
    httpAdapter = std::make_shared<LLCoreHttpUtil::HttpCoroutineAdapter>("getGridStatusRSSCoro", httpPolicy);
    LLCore::HttpRequest::ptr_t httpRequest = std::make_shared<LLCore::HttpRequest>();
    LLCore::HttpOptions::ptr_t httpOpts = std::make_shared<LLCore::HttpOptions>();
    LLCore::HttpHeaders::ptr_t httpHeaders = std::make_shared<LLCore::HttpHeaders>();

    // Convert string_view constants to std::string for HttpHeaders::append()
    httpHeaders->append(std::string{HTTP_OUT_HEADER_CONTENT_TYPE}, std::string{HTTP_CONTENT_TEXT_XML});
    
    LLSD result = httpAdapter->getRawAndSuspend(httpRequest, url, httpOpts, httpHeaders);
    LLSD httpResults = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
    LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(httpResults);
    if (!status)
    {
        return;
    }

    // A request started on one grid must not update another grid's cache or UI.
    if (LLGridManager::getInstance()->getGrid() != grid) return;

    const LLSD::Binary &rawBody = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_RAW].asBinary();
    std::string body(rawBody.begin(), rawBody.end());

    const std::string filename = LLGridManager::getInstance()->isInSecondLife() ?
        "grid_status_rss.xml" : getGridStatusRSSFilename(grid);
    std::string fullpath = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, filename);
    if(!gSavedSettings.getBOOL("TestGridStatusRSSFromFile"))
    {
        llofstream custom_file_out(fullpath.c_str(), std::ios::trunc);
        if (custom_file_out.is_open())
        {
            custom_file_out << body;
            custom_file_out.close();
        }
    }
    LLXmlTree grid_status_xml;
    if (!grid_status_xml.parseFile(fullpath))
    {
        return ;
    }
    bool new_entries = false;
    LLXmlTreeNode* rootp = grid_status_xml.getRoot();
    for (LLXmlTreeNode* item = rootp->getChildByName( "entry" ); item; item = rootp->getNextNamedChild())
    {
        LLXmlTreeNode* id_node = item->getChildByName("id");
        LLXmlTreeNode* updated_node = item->getChildByName("updated");
        if (!id_node || !updated_node)
        {
            continue;
        }
        std::string guid = id_node->getContents();
        std::string date = updated_node->getContents();
        const auto item_key = std::make_pair(grid, guid);
        if(sItemsMap.find(item_key) == sItemsMap.end())
        {
            new_entries = true;
        }
        else
        {
            if(sItemsMap[item_key] != date)
            {
                new_entries = true;
            }
        }
        sItemsMap[item_key] = date;
    }
    const bool first_grid_update = sInitializedGrids.insert(grid).second;
    if(new_entries && !first_grid_update && !getInstance()->isFirstUpdate())
    {
        gToolBarView->flashCommand(LLCommandId("gridstatus"), true);
    }
    getInstance()->setFirstUpdate(false);
}

// virtual
void LLFloaterGridStatus::handleReshape(const LLRect& new_rect, bool by_user)
{
    if (by_user && !isMinimized())
    {
        gSavedSettings.setRect("GridStatusFloaterRect", new_rect);
    }

    LLFloaterWebContent::handleReshape(new_rect, by_user);
}

void LLFloaterGridStatus::applyPreferredRect()
{
    const LLRect preferred_rect = gSavedSettings.getRect("GridStatusFloaterRect");

    LLRect new_rect = getRect();
    new_rect.setLeftTopAndSize(
        new_rect.mLeft, new_rect.mTop,
        preferred_rect.getWidth(), preferred_rect.getHeight());
    setShape(new_rect);
}

LLFloaterGridStatus* LLFloaterGridStatus::getInstance()
{
    return LLFloaterReg::getTypedInstance<LLFloaterGridStatus>("grid_status");
}

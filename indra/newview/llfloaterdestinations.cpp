/**
 * @file llfloaterdestinations.h
 * @author Leyla Farazha
 * @brief floater for the destinations guide
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Linden Research, Inc.
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

#include "llfloaterdestinations.h"
#include "llagent.h"
#include "lllogininstance.h"
#include "llmediactrl.h"
#include "lluri.h"
#include "llviewernetwork.h"
#include "llviewerregion.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llweb.h"

namespace
{
bool isUsableDestinationGuideURL(const std::string& endpoint)
{
    if (endpoint.empty() || endpoint.size() > 2048 ||
        endpoint.find_first_of("\\\r\n\t ") != std::string::npos ||
        endpoint.find('#') != std::string::npos)
    {
        return false;
    }
    const LLURI uri(endpoint);
    return (uri.scheme() == "http" || uri.scheme() == "https") &&
           !uri.hostName().empty() && uri.authority().find('@') == std::string::npos;
}

std::string getDestinationGuideEndpoint()
{
    LLGridManager* grid_manager = LLGridManager::getInstance();
    if (grid_manager->isInSecondLife())
    {
        const std::string url = gSavedSettings.getString("DestinationGuideURL");
        return isUsableDestinationGuideURL(url) ? url : std::string();
    }
    if (!grid_manager->isInOpenSim()) return std::string();

    if (LLViewerRegion* region = gAgent.getRegion())
    {
        LLSD features;
        region->getSimulatorFeatures(features);
        const LLSD& extras = features["OpenSimExtras"];
        if (extras["destination-guide-url"].isString() &&
            isUsableDestinationGuideURL(extras["destination-guide-url"].asString()))
        {
            return extras["destination-guide-url"].asString();
        }
    }

    if (LLLoginInstance::instanceExists() && LLLoginInstance::getInstance()->authSuccess())
    {
        const LLSD url = LLLoginInstance::getInstance()->getResponse("destination_guide_url");
        if (url.isString() && isUsableDestinationGuideURL(url.asString())) return url.asString();
    }
    return std::string();
}
}

LLFloaterDestinations::LLFloaterDestinations(const LLSD& key)
    :   LLFloater(key)
{
}

LLFloaterDestinations::~LLFloaterDestinations()
{
}

bool LLFloaterDestinations::postBuild()
{
    LLMediaCtrl* destinations = getChild<LLMediaCtrl>("destination_guide_contents");
    destinations->setErrorPageURL(gSavedSettings.getString("GenericErrorPageURL"));
    navigateToDestinationGuide();

    // If cookie is there, will set it now. Otherwise will have to wait for login completion
    // which will also update destinations instance if it already exists.
    LLViewerMedia::getInstance()->getOpenIDCookie(destinations);
    return true;
}

void LLFloaterDestinations::onOpen(const LLSD& key)
{
    LLFloater::onOpen(key);
    navigateToDestinationGuide();
}

void LLFloaterDestinations::navigateToDestinationGuide()
{
    LLMediaCtrl* destinations = getChild<LLMediaCtrl>("destination_guide_contents");
    const std::string endpoint = getDestinationGuideEndpoint();
    if (endpoint.empty())
    {
        if (!mDestinationGuideURL.empty())
        {
            mDestinationGuideURL.clear();
            destinations->navigateTo("about:blank", std::string{HTTP_CONTENT_TEXT_HTML});
        }
        return;
    }

    const std::string url = LLWeb::expandURLSubstitutions(endpoint, LLSD());
    if (!isUsableDestinationGuideURL(url))
    {
        if (!mDestinationGuideURL.empty())
        {
            mDestinationGuideURL.clear();
            destinations->navigateTo("about:blank", std::string{HTTP_CONTENT_TEXT_HTML});
        }
        return;
    }
    if (url == mDestinationGuideURL) return;
    mDestinationGuideURL = url;
    destinations->navigateTo(url, std::string{HTTP_CONTENT_TEXT_HTML});
}

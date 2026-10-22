/**
 * @file lltasia_user_config.cpp
 * @brief Runtime Tasia user badge/title configuration.
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

#include "lltasia_user_config.h"
#include "lltasia_config_fetch.h"
#include "llfloaterprofile.h"
#include "llfloaterreg.h"
#include "llviewernetwork.h"
#include "llagent.h"
#include "llframetimer.h"
#include "lluri.h"
#include "llsdjson.h"
#include <boost/json.hpp>

#include "llcorehttputil.h"
#include "llcoros.h"
#include "llhttpconstants.h"
#include "llmath.h"
#include "llavatarname.h"
#include "llsd.h"
#include "llsecapi.h"
#include "llstring.h"
#include "llviewercontrol.h"
#include "llvoavatar.h"

#include <boost/bind.hpp>
#include <cstdlib>
#include <map>
#include <sstream>

namespace
{
const S32 MAX_USERS = 1024;
const std::size_t MAX_SHORT_TEXT = 128;
const std::size_t MAX_LONG_TEXT = 1024;
const std::size_t MAX_URL_TEXT = 1024;

typedef std::map<std::string, LLTasiaUserConfig::User> user_map_t;

bool sRequested = false;
bool sLoaded = false;
bool sPending = false;
F64 sNextRequest = 0;
U64 sGeneration = 0;
std::string sGrid;
user_map_t sUsers;

std::string gridKey(std::string grid)
{
    LLStringUtil::trim(grid);
    if (grid.empty() || grid.size() > 255 || grid.find_first_of("@?#\\\r\n\t ") != std::string::npos)
        return {};
    const LLURI uri(grid.find("://") == std::string::npos ? "https://" + grid : grid);
    if (uri.scheme() != "https" && uri.scheme() != "http") return {};
    return utf8str_tolower(uri.authority());
}

// Resolve an entry's grid to the viewer's internal grid key. Accepts the key
// itself, a bare host with or without a port, the login-list label, and the
// friendly name "secondlife.com" for the built-in Second Life grid.
std::string resolveEntryGrid(const std::string& value)
{
    const std::string resolved = LLGridManager::getInstance()->resolveGridKey(value);
    if (!resolved.empty()) return resolved;
    return gridKey(value);
}

// The catalog is served by one grid's own host. An entry without an explicit
// grid belongs to THAT grid only, so a shared file can never leak a cosmetic
// identity onto a different grid's residents.
std::string sourceGridForUrl(const std::string& url)
{
    const LLURI uri(url);
    const std::string host = utf8str_tolower(uri.hostName());
    if (host.empty()) return {};
    const auto grids = LLGridManager::getInstance()->getKnownGrids();
    for (const auto& grid : grids)
    {
        const std::string key = grid.first;
        const std::string key_host = key.substr(0, key.find(':'));
        if (key_host == host) return key;
    }
    return resolveEntryGrid(host);
}

std::string cleanText(std::string value, std::size_t max_len)
{
    LLStringUtil::trim(value);
    if (value.size() > max_len)
    {
        value.resize(max_len);
        LLStringUtil::trim(value);
    }

    for (std::string::iterator it = value.begin(); it != value.end(); ++it)
    {
        const unsigned char c = static_cast<unsigned char>(*it);
        if (c < 0x20 && c != '\n' && c != '\r' && c != '\t')
        {
            *it = ' ';
        }
    }

    return value;
}

std::string getCleanString(const LLSD& data, const std::string& key, std::size_t max_len)
{
    if (!data.has(key) || !data[key].isString())
    {
        return std::string();
    }

    return cleanText(data[key].asString(), max_len);
}

std::string getFirstString(const LLSD& data, const char* key1, const char* key2, const char* key3)
{
    std::string value = getCleanString(data, key1, MAX_SHORT_TEXT);
    if (!value.empty())
    {
        return value;
    }
    value = getCleanString(data, key2, MAX_SHORT_TEXT);
    if (!value.empty())
    {
        return value;
    }
    return getCleanString(data, key3, MAX_SHORT_TEXT);
}

bool startsWithHttpURL(const std::string& url)
{
    return LLStringUtil::startsWith(url, "https://") || LLStringUtil::startsWith(url, "http://");
}

std::string getCleanURL(const LLSD& data, const std::string& key)
{
    std::string url = getCleanString(data, key, MAX_URL_TEXT);
    if (!startsWithHttpURL(url))
    {
        return std::string();
    }
    return url;
}

bool parseHexByte(const std::string& value, std::size_t offset, F32& out)
{
    if (offset + 2 > value.size())
    {
        return false;
    }

    const std::string byte_str = value.substr(offset, 2);
    if (byte_str.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) return false;
    char* end = NULL;
    const unsigned long parsed = std::strtoul(byte_str.c_str(), &end, 16);
    if (!end || *end != '\0' || parsed > 255)
    {
        return false;
    }

    out = static_cast<F32>(parsed) / 255.f;
    return true;
}

bool parseColorString(std::string value, LLColor4& color)
{
    LLStringUtil::trim(value);
    if (!value.empty() && value[0] == '#')
    {
        value.erase(0, 1);
    }

    if (value.size() != 6 && value.size() != 8)
    {
        return false;
    }

    F32 r = 1.f;
    F32 g = 1.f;
    F32 b = 1.f;
    F32 a = 1.f;
    if (!parseHexByte(value, 0, r) ||
        !parseHexByte(value, 2, g) ||
        !parseHexByte(value, 4, b))
    {
        return false;
    }

    if (value.size() == 8 && !parseHexByte(value, 6, a))
    {
        return false;
    }

    color.set(r, g, b, a);
    return true;
}

bool parseColor(const LLSD& data, LLColor4& color)
{
    std::string value = getCleanString(data, "tag_color", MAX_SHORT_TEXT);
    if (value.empty())
    {
        value = getCleanString(data, "color", MAX_SHORT_TEXT);
    }
    if (value.empty())
    {
        return false;
    }

    return parseColorString(value, color);
}

LLTasiaUserConfig::User parseUser(const LLSD& item)
{
    LLTasiaUserConfig::User user;
    user.custom_title = getFirstString(item, "custom_title", "title", "rank_title");
    // Only explicitly populated name fields are UI overrides. page_name belongs
    // to the website and MUST NOT be used as a display-name fallback.
    user.cosmetic_display_name = getCleanString(item, "display_name", MAX_SHORT_TEXT);
    user.cosmetic_username = getCleanString(item, "username", MAX_SHORT_TEXT);
    user.badge_name = getCleanString(item, "badge_name", MAX_SHORT_TEXT);
    user.badge_icon = getCleanURL(item, "badge_icon");
    user.profile_text = getCleanString(item, "profile_text", MAX_LONG_TEXT);
    user.tooltip = getCleanString(item, "tooltip", MAX_LONG_TEXT);
    user.has_tag_color = parseColor(item, user.tag_color);
    return user;
}

void applyConfig(const LLSD& response, const std::string& source_grid)
{
    user_map_t parsed_users;
    if (!response.has("users") || !response["users"].isArray())
    {
        LL_WARNS("TasiaConfig") << "Remote user config has no users array" << LL_ENDL;
        return;
    }

    const LLSD& users = response["users"];
    if (users.size() > MAX_USERS) return; // reject whole oversized catalog, not a partial identity set
    S32 count = 0;
    for (LLSD::array_const_iterator it = users.beginArray(); it != users.endArray() && count < MAX_USERS; ++it)
    {
        const LLSD& item = *it;
        if (!item.isMap())
        {
            continue;
        }

        const std::string uuid_string = getCleanString(item, "uuid", MAX_SHORT_TEXT);
        // An explicit grid wins; otherwise the entry belongs to the grid that
        // publishes this catalog. Entries for an unknown grid are ignored.
        std::string grid = resolveEntryGrid(getCleanString(item, "grid", 255));
        if (grid.empty()) grid = source_grid;
        LLUUID agent_id(uuid_string);
        if (grid.empty() || agent_id.isNull())
        {
            continue;
        }

        LLTasiaUserConfig::User user = parseUser(item);
        if (!user.hasProfileBadge() && user.getNametagTitle().empty() &&
            user.cosmetic_display_name.empty() && user.cosmetic_username.empty() && !user.has_tag_color)
        {
            continue;
        }

        const std::string key = grid + "\n" + agent_id.asString();
        if (parsed_users.count(key)) return; // ambiguous duplicate identity: retain last good catalog
        parsed_users[key] = user;
        ++count;
    }

    sUsers.swap(parsed_users);
    sLoaded = true;
    LL_INFOS("TasiaConfig") << "Loaded " << sUsers.size() << " Tasia custom user entries" << LL_ENDL;
    LLVOAvatar::invalidateNameTags();
}
}

std::string LLTasiaUserConfig::User::getNametagTitle() const
{
    if (!custom_title.empty())
    {
        return custom_title;
    }
    return badge_name;
}

bool LLTasiaUserConfig::User::hasProfileBadge() const
{
    return !badge_name.empty() || !profile_text.empty() || !tooltip.empty() || !badge_icon.empty();
}

// static
void LLTasiaUserConfig::requestOnce()
{
    const std::string grid = gridKey(LLGridManager::getInstance()->getGrid());    if (sGrid != grid || (!gSavedSettings.getBOOL("TasiaRemoteUserConfigEnabled") && !sUsers.empty()))
    {
        sGrid = grid;
        ++sGeneration;
        sPending = false;
        sNextRequest = 0;
        sUsers.clear();
        sLoaded = false;
        LLVOAvatar::invalidateNameTags();
    }
    if (!gSecAPIHandler || gAgent.getSessionID().isNull() || grid.empty() ||
        !gSavedSettings.getBOOL("TasiaRemoteUserConfigEnabled") || sPending ||
        LLFrameTimer::getTotalSeconds() < sNextRequest)
    {
        return;
    }

    sRequested = true;
    sPending = true;
    sNextRequest = LLFrameTimer::getTotalSeconds() + 300.0;
    std::string url = gSavedSettings.getString("TasiaRemoteUserConfigURL");
    const auto generation = ++sGeneration;
    const auto session = gAgent.getSessionID();
    // A catalog hosted by the grid we are on applies to that grid. A catalog
    // hosted elsewhere only applies when it names the grid explicitly.
    const std::string source_grid = sourceGridForUrl(url);
    tasiaFetchConfig(url, 262144, [grid, generation, session, source_grid](bool ok, const std::string& text)
    {
        if (generation != sGeneration) return;
        sPending = false;
        if (gAgent.getSessionID() != session || gridKey(LLGridManager::getInstance()->getGrid()) != grid ||
            !gSavedSettings.getBOOL("TasiaRemoteUserConfigEnabled")) return;
        sLoaded = true;
        if (!ok) return; // fail softly; never alter simulator/account identity
        boost::system::error_code error;
        const auto json = boost::json::parse(text, error);
        if (!error)
        {
            applyConfig(LlsdFromJson(json), source_grid);

            // The catalog can finish after the user's profile has already
            // resolved its avatar-name callback. Refresh that open profile so
            // explicit display_name/username overrides appear immediately.
            LLFloaterProfile* self_profile = LLFloaterReg::findTypedInstance<LLFloaterProfile>(
                "profile", LLSD().with("id", gAgent.getID()));
            if (self_profile)
            {
                self_profile->refreshName();
            }
        }
    });
}

// static
bool LLTasiaUserConfig::getUser(const LLUUID& agent_id, User& user)
{
    if (agent_id.isNull() || gAgent.getSessionID().isNull() ||
        !gSavedSettings.getBOOL("TasiaRemoteUserConfigEnabled"))
    {
        return false;
    }

    user_map_t::const_iterator it = sUsers.find(
        gridKey(LLGridManager::getInstance()->getGrid()) + "\n" + agent_id.asString());
    if (it == sUsers.end())
    {
        return false;
    }
    user = it->second;
    return true;
}

// static
bool LLTasiaUserConfig::hasCosmeticAlias(const LLUUID& agent_id)
{
    User user;
    return getUser(agent_id, user) && (!user.cosmetic_display_name.empty() || !user.cosmetic_username.empty());
}

// static
std::string LLTasiaUserConfig::renderDisplayName(const LLUUID& agent_id, const LLAvatarName& real_name)
{
    User user;
    return (getUser(agent_id, user) && !user.cosmetic_display_name.empty())
        ? user.cosmetic_display_name : real_name.getDisplayName();
}

// static
std::string LLTasiaUserConfig::renderUsername(const LLUUID& agent_id, const LLAvatarName& real_name)
{
    User user;
    return (getUser(agent_id, user) && !user.cosmetic_username.empty())
        ? user.cosmetic_username : real_name.getUserName();
}

// static
std::string LLTasiaUserConfig::renderCompleteName(const LLUUID& agent_id, const LLAvatarName& real_name)
{
    if (!hasCosmeticAlias(agent_id)) return real_name.getCompleteName();
    const std::string display_name = renderDisplayName(agent_id, real_name);
    const std::string username = renderUsername(agent_id, real_name);
    return display_name == username ? display_name : display_name + " (" + username + ")";
}

// static
std::string LLTasiaUserConfig::renderAccountType(const LLUUID&, const std::string& canonical_account_type)
{
    return canonical_account_type;
}

// static
bool LLTasiaUserConfig::isRequested()
{
    return sRequested;
}

// static
bool LLTasiaUserConfig::isLoaded()
{
    return sLoaded;
}

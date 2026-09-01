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
const U32 MIN_CONFIG_BYTES = 1024;
const U32 MAX_CONFIG_BYTES = 1024 * 1024;
const S32 MAX_USERS = 1024;
const std::size_t MAX_SHORT_TEXT = 128;
const std::size_t MAX_LONG_TEXT = 1024;
const std::size_t MAX_URL_TEXT = 1024;

typedef std::map<std::string, LLTasiaUserConfig::User> user_map_t;

bool sRequested = false;
bool sLoaded = false;
user_map_t sUsers;

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
    // These aliases are local presentation only: they never alter account,
    // profile, IM, chat, or simulator identity.
    user.cosmetic_display_name = getFirstString(item, "display_name", "display_name_override", "nametag_display_name");
    user.cosmetic_username = getFirstString(item, "username", "username_override", "nametag_username");
    user.cosmetic_account_type = getFirstString(item, "account_type", "account_type_override", "profile_account_type");
    user.badge_name = getCleanString(item, "badge_name", MAX_SHORT_TEXT);
    user.badge_icon = getCleanURL(item, "badge_icon");
    user.profile_text = getCleanString(item, "profile_text", MAX_LONG_TEXT);
    user.tooltip = getCleanString(item, "tooltip", MAX_LONG_TEXT);
    user.has_tag_color = parseColor(item, user.tag_color);
    return user;
}

void applyConfig(const LLSD& response)
{
    user_map_t parsed_users;
    if (!response.has("users") || !response["users"].isArray())
    {
        LL_WARNS("TasiaConfig") << "Remote user config has no users array" << LL_ENDL;
        sUsers.clear();
        sLoaded = true;
        return;
    }

    const LLSD& users = response["users"];
    S32 count = 0;
    for (LLSD::array_const_iterator it = users.beginArray(); it != users.endArray() && count < MAX_USERS; ++it)
    {
        const LLSD& item = *it;
        if (!item.isMap())
        {
            continue;
        }

        const std::string uuid_string = getCleanString(item, "uuid", MAX_SHORT_TEXT);
        LLUUID agent_id(uuid_string);
        if (agent_id.isNull())
        {
            continue;
        }

        LLTasiaUserConfig::User user = parseUser(item);
        if (!user.hasProfileBadge() && user.getNametagTitle().empty()
            && user.cosmetic_display_name.empty() && user.cosmetic_username.empty()
            && user.cosmetic_account_type.empty()
            && !user.has_tag_color)
        {
            continue;
        }

        parsed_users[agent_id.asString()] = user;
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
    if (sRequested)
    {
        return;
    }

    if (!gSecAPIHandler)
    {
        LL_DEBUGS("TasiaConfig") << "Remote user config fetch deferred until security handler is initialized" << LL_ENDL;
        return;
    }

    sRequested = true;

    if (!gSavedSettings.getBOOL("TasiaRemoteUserConfigEnabled"))
    {
        sLoaded = true;
        return;
    }

    std::string url = gSavedSettings.getString("TasiaRemoteUserConfigURL");
    LLStringUtil::trim(url);
    if (url.empty())
    {
        sLoaded = true;
        return;
    }

    const F32 timeout_setting = gSavedSettings.getF32("TasiaRemoteUserConfigTimeout");
    const U32 timeout_seconds = static_cast<U32>(llclamp(static_cast<S32>(timeout_setting + 0.999f), 1, 30));
    const U32 max_bytes = llclamp(gSavedSettings.getU32("TasiaRemoteUserConfigMaxBytes"), MIN_CONFIG_BYTES, MAX_CONFIG_BYTES);

    LLCoros::instance().launch("LLTasiaUserConfig::fetchCoro",
        boost::bind(&LLTasiaUserConfig::fetchCoro, url, timeout_seconds, max_bytes));
}

// static
bool LLTasiaUserConfig::getUser(const LLUUID& agent_id, User& user)
{
    if (agent_id.isNull())
    {
        return false;
    }

    user_map_t::const_iterator it = sUsers.find(agent_id.asString());
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
        ? user.cosmetic_username : real_name.getUserNameForDisplay();
}

// static
std::string LLTasiaUserConfig::renderCompleteName(const LLUUID& agent_id, const LLAvatarName& real_name)
{
    if (!hasCosmeticAlias(agent_id))
    {
        return real_name.getCompleteName();
    }
    const std::string display_name = renderDisplayName(agent_id, real_name);
    const std::string username = renderUsername(agent_id, real_name);
    return (display_name == username) ? display_name : display_name + " (" + username + ")";
}

// static
std::string LLTasiaUserConfig::renderAccountType(const LLUUID& agent_id, const std::string& canonical_account_type)
{
    User user;
    return (getUser(agent_id, user) && !user.cosmetic_account_type.empty())
        ? user.cosmetic_account_type : canonical_account_type;
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

// static
void LLTasiaUserConfig::fetchCoro(std::string url, U32 timeout_seconds, U32 max_bytes)
{
    LLCore::HttpRequest::ptr_t http_request(new LLCore::HttpRequest);
    LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t http_adapter(
        new LLCoreHttpUtil::HttpCoroutineAdapter("TasiaUserConfigFetch", LLCore::HttpRequest::DEFAULT_POLICY_ID));
    LLCore::HttpHeaders::ptr_t http_headers(new LLCore::HttpHeaders);
    LLCore::HttpOptions::ptr_t http_options(new LLCore::HttpOptions);

    http_options->setTimeout(timeout_seconds);
    http_options->setTransferTimeout(timeout_seconds);
    http_options->setRetries(0);

    std::ostringstream range;
    range << "bytes=0-" << (max_bytes - 1);
    http_headers->append(HTTP_OUT_HEADER_ACCEPT, "application/json, */*;q=0.1");
    http_headers->append(HTTP_OUT_HEADER_RANGE, range.str());

    LLSD response = http_adapter->getJsonAndSuspend(http_request, url, http_options, http_headers);
    LLSD http_results = response[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
    LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(http_results);
    if (!status)
    {
        LL_WARNS("TasiaConfig") << "Remote user config fetch failed: " << status.toString() << LL_ENDL;
        sLoaded = true;
        return;
    }

    applyConfig(response);
}

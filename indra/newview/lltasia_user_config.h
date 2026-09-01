/**
 * @file lltasia_user_config.h
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

#ifndef LL_TASIA_USER_CONFIG_H
#define LL_TASIA_USER_CONFIG_H

#include "stdtypes.h"
#include "lluuid.h"
#include "v4color.h"

#include <string>

class LLAvatarName;

class LLTasiaUserConfig
{
public:
    struct User
    {
        std::string custom_title;
        std::string cosmetic_display_name;
        std::string cosmetic_username;
        std::string cosmetic_account_type;
        std::string badge_name;
        std::string badge_icon;
        std::string profile_text;
        std::string tooltip;
        LLColor4 tag_color;
        bool has_tag_color = false;

        std::string getNametagTitle() const;
        bool hasProfileBadge() const;
    };

    static void requestOnce();
    static bool getUser(const LLUUID& agent_id, User& user);
    // UI-only formatters. They never alter canonical name cache data or UUID identity.
    static bool hasCosmeticAlias(const LLUUID& agent_id);
    static std::string renderDisplayName(const LLUUID& agent_id, const LLAvatarName& real_name);
    static std::string renderUsername(const LLUUID& agent_id, const LLAvatarName& real_name);
    static std::string renderCompleteName(const LLUUID& agent_id, const LLAvatarName& real_name);
    static std::string renderAccountType(const LLUUID& agent_id, const std::string& canonical_account_type);
    static bool isRequested();
    static bool isLoaded();

private:
    static void fetchCoro(std::string url, U32 timeout_seconds, U32 max_bytes);
};

#endif // LL_TASIA_USER_CONFIG_H

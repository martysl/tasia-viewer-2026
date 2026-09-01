/**
 * @file lltasia_klipy_key.cpp
 * @brief Runtime access to the configured Tasia KLIPY API key.
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

#include "lltasia_klipy_key.h"
#include "lltasia_klipy_key.generated.h"
#include "llstring.h"
#include "llviewercontrol.h"

namespace LLTasiaKlipyKey
{
std::string getEmbeddedAPIKey()
{
    return LLTasiaKlipyKeyGenerated::getAPIKey();
}

bool hasEmbeddedAPIKey()
{
    return LLTasiaKlipyKeyGenerated::hasAPIKey();
}

std::string getConfiguredAPIKey()
{
    std::string key = gSavedSettings.getString("TasiaKlipyAPIKey");
    LLStringUtil::trim(key);
    if (!key.empty())
    {
        return key;
    }

    return getEmbeddedAPIKey();
}

bool hasConfiguredAPIKey()
{
    return !getConfiguredAPIKey().empty();
}
}

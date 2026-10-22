/**
 * @file llviewernetwork.cpp
 * @author James Cook, Richard Nelson
 * @brief Networking constants and globals for viewer.
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

#include "llviewernetwork.h"
#include "llviewercontrol.h"
#include "llsdserialize.h"
#include "llsecapi.h"
#include "lltrans.h"
#include "llweb.h"
#include "lluri.h"
#include "lltasia_config_fetch.h"
#include "llpanellogin.h"
#include <sstream>
#include <algorithm>


/// key used to store the grid, and the name attribute in the grid data
const std::string  GRID_VALUE = "keyname";
/// the value displayed in the grid selector menu, and other human-oriented text
const std::string  GRID_LABEL_VALUE = "label";
/// the value used on the --grid command line argument
const std::string  GRID_ID_VALUE = "grid_login_id";
/// the url for the login cgi script
const std::string  GRID_LOGIN_URI_VALUE = "login_uri";
/// url base for update queries
const std::string  GRID_UPDATE_SERVICE_URL = "update_query_url_base";
///
const std::string  GRID_HELPER_URI_VALUE = "helper_uri";
/// the splash page url
const std::string  GRID_LOGIN_PAGE_VALUE = "login_page";
/// url for the web profile site
const std::string  GRID_WEB_PROFILE_VALUE = "web_profile_url";
/// internal data on system grids
const std::string  GRID_IS_SYSTEM_GRID_VALUE = "system_grid";
/// whether this is single or double names
const std::string  GRID_LOGIN_IDENTIFIER_TYPES = "login_identifier_types";

// defines slurl formats associated with various grids.
// we need to continue to support existing forms, as slurls
// are shared between viewers that may not understand newer
// forms.
const std::string GRID_SLURL_BASE = "slurl_base";
const std::string GRID_APP_SLURL_BASE = "app_slurl_base";

const std::string DEFAULT_LOGIN_PAGE = "https://startup.tasiaviewer.work";

const std::string MAIN_GRID_LOGIN_URI = "https://login.agni.lindenlab.com/cgi-bin/login.cgi";

const std::string SL_UPDATE_QUERY_URL = "https://update.secondlife.com/update";

const std::string MAIN_GRID_SLURL_BASE = "https://maps.secondlife.com/secondlife/";
const std::string SYSTEM_GRID_APP_SLURL_BASE = "secondlife:///app";

const std::string MAIN_GRID_WEB_PROFILE_URL = "https://my.secondlife.com/";

const char* SYSTEM_GRID_SLURL_BASE = "secondlife://%s/secondlife/";
const char* DEFAULT_SLURL_BASE = "https://%s/region/";
const char* DEFAULT_APP_SLURL_BASE = "x-grid-location-info://%s/app";

namespace
{
bool gridURL(const std::string& value, bool login = false)
{
    if (value.empty()) return !login;
    const LLURI uri(value);
    if (value.size() > 2048 || (uri.scheme() != "http" && uri.scheme() != "https") ||
        uri.hostName().empty() || uri.authority().find('@') != std::string::npos ||
        value.find_first_of("\\\r\n\t ") != std::string::npos || value.find('#') != std::string::npos)
        return false;
    if (login && value.find('?') != std::string::npos) return false;
    const auto authority = uri.authority();
    const auto colon = authority.find(':');
    if (colon != std::string::npos)
    {
        const auto port = authority.substr(colon + 1);
        if (port.empty() || port.size() > 5 || port.find_first_not_of("0123456789") != std::string::npos ||
            std::stoul(port) == 0 || std::stoul(port) > 65535) return false;
    }
    return true;
}

bool gridMetadata(LLSD& target, const LLSD& source)
{
    for (const auto* key : {"login_page", "helper_uri", "web_profile_url", "register",
        "password", "help", "about", "search", "gatekeeper", "currency_uri", "marketplace"})
    {
        if (!source.has(key)) continue;
        if (!source[key].isString()) return false;
        if (std::string(key) == "login_page" && source[key].asString() == "about:blank")
        { target[key] = source[key]; continue; }
        if (!gridURL(source[key].asString())) return false;
        target[key] = source[key];
    }
    return true;
}
}

LLGridManager::LLGridManager()
:	mIsInProductionGrid(false)
{
	// by default, we use the 'grids.xml' file in the user settings directory
	// this file is an LLSD file containing multiple grid definitions.
	// This file does not contain definitions for secondlife.com grids,
	// as that would be a security issue when they are overwritten by
	// an attacker.  Don't want someone snagging a password.
	std::string grid_file = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
														   "grids.xml");
	LL_DEBUGS("GridManager")<<LL_ENDL;

	initialize(grid_file);

}


//
// LLGridManager - class for managing the list of known grids, and the current
// selection
//


//
// LLGridManager::initialze - initialize the list of known grids based
// on the fixed list of linden grids (fixed for security reasons)
// and the grids.xml file
void LLGridManager::initialize(const std::string& grid_file)
{
	// default grid list.
	// Don't move to a modifiable file for security reasons,
	mGrid.clear() ;

	// set to undefined
	mGridList = LLSD();
    ++mCatalogGeneration;
    mCatalogPending = false;
    mRemovedGrids = LLSD::emptyMap();
	mGridFile = grid_file;
	// as we don't want an attacker to override our grid list
	// to point the default grid to an invalid grid
  	addSystemGrid(LLTrans::getString("AgniGridLabel"),
				  MAINGRID,
				  MAIN_GRID_LOGIN_URI,
				  "https://secondlife.com/helpers/",
				  DEFAULT_LOGIN_PAGE,
				  SL_UPDATE_QUERY_URL,
				  MAIN_GRID_WEB_PROFILE_URL,
				  "Agni");
	addSystemGrid(LLTrans::getString("AditiGridLabel"),
                  BETAGRID,
				  "https://login.aditi.lindenlab.com/cgi-bin/login.cgi",
				  "https://secondlife.aditi.lindenlab.com/helpers/",
				  DEFAULT_LOGIN_PAGE,
				  SL_UPDATE_QUERY_URL,
				  "https://my.secondlife-beta.com/",
				  "Aditi");

	LLSD other_grids;
	llifstream llsd_xml;
	// grids.xml is not supplied by default
	if (!grid_file.empty() && LLFile::isfile(grid_file))
	{
        LL_INFOS("GridManager")<<"Grid configuration file '"<<grid_file<<"'"<<LL_ENDL;
			llsd_xml.open(grid_file.c_str(), std::ios::in | std::ios::binary);

			// parse through the gridfile, inserting grids into the list unless
			// they overwrite an existing grid.
			if( llsd_xml.is_open())
			{
				LLSDSerialize::fromXMLDocument( other_grids, llsd_xml );
				if(other_grids.isMap())
				{
                    if (other_grids["tasia_removed_grids"].isMap())
                        mRemovedGrids = other_grids["tasia_removed_grids"];
					for(LLSD::map_iterator grid_itr = other_grids.beginMap();
						grid_itr != other_grids.endMap();
						++grid_itr)
					{
						LLSD::String key_name = grid_itr->first;
                        if (key_name == "tasia_removed_grids") continue;
						LLSD grid = grid_itr->second;

						std::string existingGrid = getGrid(grid);
						if (mGridList.has(key_name) || !existingGrid.empty())
						{
							LL_WARNS("GridManager") << "Cannot override existing grid '" << key_name << "'; ignoring definition from '"<<grid_file<<"'" << LL_ENDL;
						}
						else if ( addGrid(grid) )
						{
							LL_INFOS("GridManager") << "added grid '"<<key_name<<"'"<<LL_ENDL;
						}
						else
						{
							LL_WARNS("GridManager") << "failed to add invalid grid '"<<key_name<<"'"<<LL_ENDL;
						}
					}
					llsd_xml.close();
				}
				else
				{
					LL_WARNS("GridManager")<<"Failed to parse grid configuration '"<<grid_file<<"'"<<LL_ENDL;
				}
			}
			else
			{
				// S24 - Removed warning spam - grids.xml is optional (user-created for custom grids)
				LL_DEBUGS("GridManager")<<"Grid configuration file not found: '"<<grid_file<<"' (this is normal)"<<LL_ENDL;
			}
		}
	else
	{
		LL_DEBUGS("GridManager")<<"no grid file specified"<<LL_ENDL;
	}

    // Bundled defaults use the same non-overwriting, tombstone-aware importer.
    const auto defaults = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "tasia_default_grids.xml");
    llifstream defaults_file(defaults.c_str(), std::ios::binary);
    if (defaults_file.is_open())
    {
        std::string xml((std::istreambuf_iterator<char>(defaults_file)), std::istreambuf_iterator<char>());
        defaults_file.close();
        std::string error;
        if (!importGridCatalog(xml, error, false))
        {
            LL_WARNS("GridManager") << "Bundled grid defaults rejected: " << error << LL_ENDL;
        }
    }

	// load a grid from the command line.
	// if the actual grid name is specified from the command line,
	// set it as the 'selected' grid.
	std::string cmd_line_grid = gSavedSettings.getString("CmdLineGridChoice");
	if(!cmd_line_grid.empty())
	{
		// try to find the grid assuming the command line parameter is
		// the case-insensitive 'label' of the grid.  ie 'Agni'
		mGrid = getGrid(cmd_line_grid);
		if(mGrid.empty())
		{
			LL_WARNS("GridManager")<<"Unknown grid '"<<cmd_line_grid<<"'"<<LL_ENDL;
		}
		else
		{
			LL_INFOS("GridManager")<<"Command line specified '"<<cmd_line_grid<<"': "<<mGrid<<LL_ENDL;
		}
	}
	else
	{
		// if a grid was not passed in via the command line, grab it from the CurrentGrid setting.
		// if there's no current grid, that's ok as it'll be either set by the value passed
		// in via the login uri if that's specified, or will default to maingrid
		std::string last_grid = gSavedSettings.getString("CurrentGrid");
		if ( ! getGrid(last_grid).empty() )
		{
			LL_INFOS("GridManager")<<"Using last grid: "<<last_grid<<LL_ENDL;
			mGrid = last_grid;
		}
		else
		{
			LL_INFOS("GridManager")<<"Last grid '"<<last_grid<<"' not configured"<<LL_ENDL;
		}
	}

	if(mGrid.empty())
	{
		// no grid was specified so default to maingrid
		LL_INFOS("GridManager") << "Default grid to "<<MAINGRID<< LL_ENDL;
		mGrid = MAINGRID;
	}

	LLControlVariablePtr grid_control = gSavedSettings.getControl("CurrentGrid");
	if (grid_control.notNull())
	{
		grid_control->getSignal()->connect(boost::bind(&LLGridManager::updateIsInProductionGrid, this));
	}

	// since above only triggers on changes, trigger the callback manually to initialize state
	updateIsInProductionGrid();

	setGridChoice(mGrid);
}

LLGridManager::~LLGridManager()
{
}

bool LLGridManager::addCustomGrid(const std::string& login_uri, const std::string& label, std::string& error,
    const LLSD& metadata)
{
    error.clear();
    std::string endpoint = login_uri;
    LLStringUtil::trim(endpoint);
    const LLURI uri(endpoint);
    const std::string scheme = uri.scheme();
    const std::string authority = uri.authority();
    const std::string host = uri.hostName();
    // Never persist embedded credentials or ambiguous/query-bearing endpoints.
    if ((scheme != "http" && scheme != "https") || host.empty() ||
        endpoint.find("://") == std::string::npos ||
        endpoint.find_first_of("?#\\") != std::string::npos ||
        std::any_of(endpoint.begin(), endpoint.end(), [](unsigned char c) { return c <= 32 || c == 127; }) ||
        authority.find('@') != std::string::npos || uri.hostPort() == 0)
    {
        error = "Enter an HTTP or HTTPS login URI without credentials, query or fragment.";
        return false;
    }

    // LLURI::hostPort() uses atoi and narrows to U16, so it alone does not
    // reject malformed or overflowing ports. This viewer's LLURI supports
    // DNS/IPv4 authorities, not bracketed IPv6 literals.
    if (host.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.") != std::string::npos)
    {
        error = "Use a DNS hostname or IPv4 address for this grid.";
        return false;
    }
    const std::size_t colon = authority.find(':');
    if (colon != std::string::npos)
    {
        const std::string port = authority.substr(colon + 1);
        if (port.empty() || port.size() > 5 || port.find_first_not_of("0123456789") != std::string::npos ||
            std::stoul(port) == 0 || std::stoul(port) > 65535)
        {
            error = "The login port must be a number between 1 and 65535.";
            return false;
        }
    }

    // The host and port identify a grid, while the exact login path is kept
    // untouched in login_uri (OpenSim endpoints are not SL CGI endpoints).
    const std::string key = utf8str_tolower(authority);
    if (!getGrid(key).empty())
    {
        error = "This grid is already in the list.";
        return false;
    }

    LLSD data = LLSD::emptyMap();
    std::string display_label = label;
    LLStringUtil::trim(display_label);
    data[GRID_VALUE] = key;
    data[GRID_LABEL_VALUE] = display_label.empty() ? key : display_label;
    data[GRID_ID_VALUE] = key;
    data[GRID_LOGIN_URI_VALUE] = LLSD::emptyArray();
    data[GRID_LOGIN_URI_VALUE].append(endpoint);
    data[GRID_LOGIN_PAGE_VALUE] = "about:blank";
    data[GRID_HELPER_URI_VALUE] = "";
    data[GRID_WEB_PROFILE_VALUE] = "";
    data[GRID_LOGIN_IDENTIFIER_TYPES] = LLSD::emptyArray();
    data[GRID_LOGIN_IDENTIFIER_TYPES].append(CRED_IDENTIFIER_TYPE_AGENT);
    if (!gridMetadata(data, metadata)) { error = "Invalid grid metadata URL."; return false; }
    if (!addGrid(data))
    {
        error = "The grid conflicts with an existing entry.";
        return false;
    }
    if (!saveCustomGrids(error))
    {
        mGridList.erase(key);
        return false;
    }
    return true;
}

bool LLGridManager::removeCustomGrid(const std::string& grid, std::string& error)
{
    error.clear();
    const std::string key = getGrid(grid);
    if (key.empty() || isSystemGrid(key) || key == mGrid)
    {
        error = "Cannot remove an unknown, built-in or currently selected grid.";
        return false;
    }
    const LLSD previous = mGridList[key];
    const LLSD removed_before = mRemovedGrids;
    mRemovedGrids[key] = true;
    mGridList.erase(key);
    if (!saveCustomGrids(error))
    {
        mGridList[key] = previous;
        mRemovedGrids = removed_before;
        return false;
    }
    return true;
}

bool LLGridManager::saveCustomGrids(std::string& error)
{
    if (mGridFile.empty())
    {
        error = "No user grid configuration file is available.";
        return false;
    }
    LLSD custom_grids = LLSD::emptyMap();
    custom_grids["tasia_removed_grids"] = mRemovedGrids;
    for (LLSD::map_const_iterator it = mGridList.beginMap(); it != mGridList.endMap(); ++it)
    {
        if (!isSystemGrid(it->first))
        {
            custom_grids[it->first] = it->second;
        }
    }

    // Replace only after a complete write; a failed save leaves the old file
    // intact, and callers roll the in-memory change back as well.
    const std::string temporary = mGridFile + ".tmp";
    llofstream stream(temporary.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (stream.is_open())
    {
        LLSDSerialize::toPrettyXML(custom_grids, stream);
        stream.flush();
        const bool written = stream.good();
        stream.close();
        if (written && !stream.fail() && LLFile::rename(temporary, mGridFile) == 0)
        {
            return true;
        }
    }
    LLFile::remove(temporary);
    error = "Could not save the user grid configuration.";
    return false;
}

LLSD LLGridManager::getGridInfo(const std::string& grid) const
{
    return mGridList[grid];
}

std::string LLGridManager::resolveGridKey(const std::string& grid)
{
    std::string value = grid;
    LLStringUtil::trim(value);
    if (value.empty() || value.size() > 255) return {};

    // A full URL is reduced to its authority, so "https://i.let-us.cyou:8002/"
    // and "i.let-us.cyou:8002" resolve identically.
    std::string host = value;
    if (value.find("://") != std::string::npos)
    {
        const LLURI uri(value);
        if (uri.scheme() != "http" && uri.scheme() != "https") return {};
        if (uri.authority().find('@') != std::string::npos) return {};
        host = uri.authority();
    }
    host = utf8str_tolower(host);
    if (host.empty() || host.find_first_of("?#\\\r\n\t ") != std::string::npos) return {};

    // Exact internal key, or the grid id/label shown in the login list.
    const std::string exact = getGrid(host);
    if (!exact.empty()) return exact;

    // Friendly public names for the built-in Second Life grids.
    if (host == "secondlife.com" || host == "lindenlab.com" || host == "secondlife" ||
        host == "sl" || host == "agni" || host == "util.agni.lindenlab.com")
    {
        return MAINGRID;
    }
    if (host == "aditi" || host == "secondlife beta" || host == "beta.secondlife.com" ||
        host == "util.aditi.lindenlab.com")
    {
        return BETAGRID;
    }

    // Match a known grid by host alone, so a missing or extra port still
    // resolves (e.g. "i.let-us.cyou" matches "i.let-us.cyou:8002").
    const std::string bare_host = host.substr(0, host.find(':'));
    for (LLSD::map_const_iterator it = mGridList.beginMap(); it != mGridList.endMap(); ++it)
    {
        const std::string key = utf8str_tolower(it->first);
        if (key == bare_host || key.substr(0, key.find(':')) == bare_host) return it->first;
    }
    return {};
}

std::string LLGridManager::getCurrencyHelperURI()
{
    std::string uri = mGridList[mGrid]["currency_uri"].asString();
    if (!uri.empty()) return gridURL(uri) ? uri : std::string();
    uri = getHelperURI();
    if (uri.empty() || !gridURL(uri) || uri.find('?') != std::string::npos) return {};
    // Some OpenSim grids advertise the complete XML-RPC endpoint, others a base.
    if (uri.size() >= 4 && uri.substr(uri.size()-4) == ".php") return uri;
    if (uri.back() != '/') uri += '/';
    return uri + "currency.php";
}

std::string LLGridManager::getMarketplaceURL()
{
    if (isInSecondLife()) return gSavedSettings.getString("MarketplaceURL");
    const std::string custom = mGridList[mGrid]["marketplace"].asString();
    if (!custom.empty() && gridURL(custom)) return custom;
    if (mGrid == "i.let-us.cyou:8002" || mGrid == "not.let-us.cyou:8002")
        return "https://marketplace.i.let-us.cyou";
    if (mGrid == "os.tasia.work.gd:22000")
        return "https://marketplace.i.let-us.cyou";
    return {};
}

bool LLGridManager::setGridMetadata(const std::string& grid, const LLSD& metadata, std::string& error)
{
    if (!mGridList.has(grid) || isSystemGrid(grid))
    { error = "Select a custom grid; built-in Second Life URLs cannot be changed."; return false; }
    LLSD data = mGridList[grid];
    if (!gridMetadata(data, metadata)) { error = "Invalid grid metadata URL."; return false; }
    if (metadata.has("label") && metadata["label"].isString() &&
        !metadata["label"].asString().empty() && metadata["label"].asString().size() <= 128)
        data["label"] = metadata["label"];
    const LLSD previous = mGridList[grid];
    mGridList[grid] = data;
    if (saveCustomGrids(error)) return true;
    mGridList[grid] = previous;
    return false;
}

bool LLGridManager::importGridCatalog(const std::string& xml, std::string& error, bool persist)
{
    error.clear();
    const auto lower = utf8str_tolower(xml);
    if (xml.size() > 262144 || xml.find('\0') != std::string::npos || lower.find("<!doctype") != std::string::npos ||
        lower.find("<!entity") != std::string::npos)
    { error = "Invalid or oversized grid catalog."; return false; }
    LLSD catalog;
    std::istringstream input(xml);
    LLSDSerialize::fromXMLDocument(catalog, input);
    if ((!catalog.isMap() && !catalog.isArray()) || catalog.size() > 256)
    { error = "Grid catalog must be an LLSD map or array of at most 256 grids."; return false; }
    const LLSD previous = mGridList;
    bool changed = false;
    auto append = [&](LLSD record)
    {
        // Accept the existing viewer and Firestorm field spellings, but never
        // trust catalog keys/system_grid flags or infer Second Life CGI paths.
        const LLSD login = record.has("login_uri") ? record["login_uri"] : record["loginuri"];
        const LLSD first = login.isArray() && login.size() ? login[0] : login;
        if (!first.isString() || !gridURL(first.asString(), true)) return false;
        const LLURI uri(first.asString());
        const auto key = utf8str_tolower(uri.authority());
        if (!getGrid(key).empty() || mRemovedGrids.has(key)) return true;
        LLSD data = LLSD::emptyMap();
        data["keyname"] = key;
        data["grid_login_id"] = key;
        data["label"] = record.has("label") ? record["label"] : record["gridname"];
        if (!data["label"].isString() || data["label"].asString().empty() || data["label"].asString().size() > 128)
            data["label"] = key;
        data["login_uri"] = LLSD::emptyArray();
        if (login.isArray())
        {
            if (login.size() > 8) return false;
            for (auto it = login.beginArray(); it != login.endArray(); ++it)
            {
                if (!it->isString() || !gridURL(it->asString(), true)) return false;
                data["login_uri"].append(*it);
            }
        }
        else data["login_uri"].append(first);
        data["login_page"] = DEFAULT_LOGIN_PAGE;
        data["helper_uri"] = "";
        data["web_profile_url"] = "";
        data["login_identifier_types"] = LLSD::emptyArray();
        data["login_identifier_types"].append(CRED_IDENTIFIER_TYPE_AGENT);
        if (!record.has("login_page") && record["loginpage"].isString()) record["login_page"] = record["loginpage"];
        if (!record.has("helper_uri") && record["helperuri"].isString()) record["helper_uri"] = record["helperuri"];
        if (!gridMetadata(data, record)) return false;
        if (!addGrid(data)) return false;
        changed = true;
        return true;
    };
    bool valid = true;
    if (catalog.isMap())
    {
        for (auto it = catalog.beginMap(); it != catalog.endMap(); ++it)
            if (it->first != "tasia_removed_grids" && !append(it->second)) { valid = false; break; }
    }
    else for (auto it = catalog.beginArray(); it != catalog.endArray(); ++it)
        if (!append(*it)) { valid = false; break; }
    if (!valid || (changed && persist && !saveCustomGrids(error)))
    {
        mGridList = previous;
        if (error.empty()) error = "Grid catalog contains an invalid entry; no changes applied.";
        return false;
    }
    return true;
}

void LLGridManager::requestGridCatalog(std::function<void(bool)> callback)
{
    if (mCatalogPending || !gSavedSettings.getBOOL("TasiaGridCatalogEnabled"))
    { if (callback) callback(false); return; }
    mCatalogPending = true;
    const auto generation = ++mCatalogGeneration;
    tasiaFetchConfig("https://api.tasiaviewer.work/viewer/config/grid.xml", 262144,
        [callback, generation](bool ok, const std::string& text)
        {
            if (!LLGridManager::instanceExists()) return;
            auto* manager = LLGridManager::getInstance();
            if (generation != manager->mCatalogGeneration) return;
            manager->mCatalogPending = false;
            if (!ok || !gSavedSettings.getBOOL("TasiaGridCatalogEnabled"))
            { if (callback) callback(false); return; }
            std::string error;
            const bool imported = manager->importGridCatalog(text, error);
            if (imported) LLPanelLogin::refreshGridList();
            else LL_WARNS("GridManager") << "Grid catalog rejected; existing grids unchanged." << LL_ENDL;
            if (callback) callback(imported);
        });
}

//
// LLGridManager::addGrid - add a grid to the grid list, populating the needed values
// if they're not populated yet.
//

bool LLGridManager::addGrid(LLSD& grid_data)
{
	bool added = false;
	if (grid_data.isMap() && grid_data.has(GRID_VALUE))
	{
		std::string grid = utf8str_tolower(grid_data[GRID_VALUE].asString());

		if ( getGrid(grid_data[GRID_VALUE]).empty() && getGrid(grid).empty() )
		{
			std::string grid_id = grid_data.has(GRID_ID_VALUE) ? grid_data[GRID_ID_VALUE].asString() : "";
			if ( getGrid(grid_id).empty() )
			{
				// populate the other values if they don't exist
				if (!grid_data.has(GRID_LABEL_VALUE))
				{
					grid_data[GRID_LABEL_VALUE] = grid;
				}
				if (!grid_data.has(GRID_ID_VALUE))
				{
					grid_data[GRID_ID_VALUE] = grid;
				}

				// if the grid data doesn't include any of the URIs, then
				// generate them from the grid, which should be a dns address
				if (!grid_data.has(GRID_LOGIN_URI_VALUE))
				{
					grid_data[GRID_LOGIN_URI_VALUE] = LLSD::emptyArray();
					grid_data[GRID_LOGIN_URI_VALUE].append(std::string("https://") +
														   grid + "/cgi-bin/login.cgi");
				}
				// Populate to the default values
				if (!grid_data.has(GRID_LOGIN_PAGE_VALUE))
				{
                    grid_data[GRID_LOGIN_PAGE_VALUE] = std::string("https://") + grid + "/app/login/";
				}
				if (!grid_data.has(GRID_HELPER_URI_VALUE))
				{
					grid_data[GRID_HELPER_URI_VALUE] = std::string("https://") + grid + "/helpers/";
				}
				if (!grid_data.has(GRID_WEB_PROFILE_VALUE))
				{
					grid_data[GRID_WEB_PROFILE_VALUE] = std::string("https://") + grid + "/";
				}

				if (!grid_data.has(GRID_LOGIN_IDENTIFIER_TYPES))
				{
					// non system grids and grids that haven't already been configured with values
					// get both types of credentials.
					grid_data[GRID_LOGIN_IDENTIFIER_TYPES] = LLSD::emptyArray();
					grid_data[GRID_LOGIN_IDENTIFIER_TYPES].append(CRED_IDENTIFIER_TYPE_AGENT);
					grid_data[GRID_LOGIN_IDENTIFIER_TYPES].append(CRED_IDENTIFIER_TYPE_ACCOUNT);
				}

				LL_DEBUGS("GridManager") <<grid<<"\n"
										 <<"  id:          "<<grid_data[GRID_ID_VALUE].asString()<<"\n"
										 <<"  label:       "<<grid_data[GRID_LABEL_VALUE].asString()<<"\n"
										 <<"  login page:  "<<grid_data[GRID_LOGIN_PAGE_VALUE].asString()<<"\n"
										 <<"  helper page: "<<grid_data[GRID_HELPER_URI_VALUE].asString()<<"\n"
										 <<"  web profile: "<<grid_data[GRID_WEB_PROFILE_VALUE].asString()<<"\n";
				/* still in LL_DEBUGS */ 
				for (LLSD::array_const_iterator login_uris = grid_data[GRID_LOGIN_URI_VALUE].beginArray();
					 login_uris != grid_data[GRID_LOGIN_URI_VALUE].endArray();
					 login_uris++)
				{
					LL_CONT << "  login uri:   "<<login_uris->asString()<<"\n";
				}
				LL_CONT << LL_ENDL;
				mGridList[grid] = grid_data;
				added = true;
			}
			else
			{
				LL_WARNS("GridManager")<<"duplicate grid id'"<<grid_id<<"' ignored"<<LL_ENDL;
			}
		}
		else
		{
			LL_WARNS("GridManager")<<"duplicate grid name '"<<grid<<"' ignored"<<LL_ENDL;
		}
	}
	else
	{
		LL_WARNS("GridManager")<<"invalid grid definition ignored"<<LL_ENDL;
	}
	return added;
}

//
// LLGridManager::addSystemGrid - helper for adding a system grid.
void LLGridManager::addSystemGrid(const std::string& label,
								  const std::string& name,
								  const std::string& login_uri,
								  const std::string& helper,
								  const std::string& login_page,
								  const std::string& update_url_base,
								  const std::string& web_profile_url,
								  const std::string& login_id)
{
	LLSD grid = LLSD::emptyMap();
	grid[GRID_VALUE] = name;
	grid[GRID_LABEL_VALUE] = label;
	grid[GRID_HELPER_URI_VALUE] = helper;
	grid[GRID_LOGIN_URI_VALUE] = LLSD::emptyArray();
	grid[GRID_LOGIN_URI_VALUE].append(login_uri);
	grid[GRID_LOGIN_PAGE_VALUE] = login_page;
	grid[GRID_UPDATE_SERVICE_URL] = update_url_base;
	grid[GRID_WEB_PROFILE_VALUE] = web_profile_url;
	grid[GRID_IS_SYSTEM_GRID_VALUE] = true;
	grid[GRID_LOGIN_IDENTIFIER_TYPES] = LLSD::emptyArray();
	grid[GRID_LOGIN_IDENTIFIER_TYPES].append(CRED_IDENTIFIER_TYPE_AGENT);

	grid[GRID_APP_SLURL_BASE] = SYSTEM_GRID_APP_SLURL_BASE;
	if (login_id.empty())
	{
		grid[GRID_ID_VALUE] = name;
	}
	else
	{
		grid[GRID_ID_VALUE] = login_id;
	}

	if (name == std::string(MAINGRID))
	{
		grid[GRID_SLURL_BASE] = MAIN_GRID_SLURL_BASE;
	}
	else
	{
		grid[GRID_SLURL_BASE] = llformat(SYSTEM_GRID_SLURL_BASE, grid[GRID_ID_VALUE].asString().c_str());
	}

	addGrid(grid);
}

// return a list of grid name -> grid label mappings for UI purposes
std::map<std::string, std::string> LLGridManager::getKnownGrids()
{
	std::map<std::string, std::string> result;
	for(LLSD::map_iterator grid_iter = mGridList.beginMap();
		grid_iter != mGridList.endMap();
		grid_iter++)
	{
		result[grid_iter->first] = grid_iter->second[GRID_LABEL_VALUE].asString();
	}

	return result;
}

void LLGridManager::setGridChoice(const std::string& grid)
{
	// Set the grid choice based on a string.
	LL_DEBUGS("GridManager")<<"requested "<<grid<<LL_ENDL;
 	std::string grid_name = getGrid(grid); // resolved either the name or the id to the name

	if(!grid_name.empty())
	{
		LL_INFOS("GridManager")<<"setting "<<grid_name<<LL_ENDL;
		mGrid = grid_name;
		gSavedSettings.setString("CurrentGrid", grid_name);
		
		updateIsInProductionGrid();
	}
	else
	{
		// the grid was not in the list of grids.
		LL_WARNS("GridManager")<<"unknown grid "<<grid<<LL_ENDL;
	}
}

std::string LLGridManager::getGrid( const std::string &grid )
{
	std::string grid_name;

	if (mGridList.has(grid))
	{
		// the grid was the long name, so we're good, return it
		grid_name = grid;
	}
	else
	{
		// search the grid list for a grid with a matching id
		for(LLSD::map_iterator grid_iter = mGridList.beginMap();
			grid_name.empty() && grid_iter != mGridList.endMap();
			grid_iter++)
		{
			if (grid_iter->second.has(GRID_ID_VALUE))
			{
				if (0 == (LLStringUtil::compareInsensitive(grid,
														   grid_iter->second[GRID_ID_VALUE].asString())))
				{
					// found a matching label, return this name
					grid_name = grid_iter->first;
				}
			}
		}
	}
	return grid_name;
}

std::string LLGridManager::getGridLabel(const std::string& grid)
{
	std::string grid_label;
	std::string grid_name = getGrid(grid);
    if (!grid_name.empty())
	{
		grid_label = mGridList[grid_name][GRID_LABEL_VALUE].asString();
	}
	else
	{
		LL_WARNS("GridManager")<<"invalid grid '"<<grid<<"'"<<LL_ENDL;
	}
	LL_DEBUGS("GridManager")<<"returning "<<grid_label<<LL_ENDL;
	return grid_label;
}

std::string LLGridManager::getGridId(const std::string& grid)
{
	std::string grid_id;
	std::string grid_name = getGrid(grid);
    if (!grid_name.empty())
	{
		grid_id = mGridList[grid_name][GRID_ID_VALUE].asString();
	}
	else
	{
		LL_WARNS("GridManager")<<"invalid grid '"<<grid<<"'"<<LL_ENDL;
	}
	LL_DEBUGS("GridManager")<<"returning "<<grid_id<<LL_ENDL;
	return grid_id;
}

void LLGridManager::getLoginURIs(const std::string& grid, std::vector<std::string>& uris)
{
	uris.clear();
	std::string grid_name = getGrid(grid);
	if (!grid_name.empty())
	{
        if (mGridList[grid_name][GRID_LOGIN_URI_VALUE].isArray())
        {
		    for (LLSD::array_iterator llsd_uri = mGridList[grid_name][GRID_LOGIN_URI_VALUE].beginArray();
			     llsd_uri != mGridList[grid_name][GRID_LOGIN_URI_VALUE].endArray();
			     llsd_uri++)
		    {
			    uris.push_back(llsd_uri->asString());
		    }
        }
        else
        {
            uris.push_back(mGridList[grid_name][GRID_LOGIN_URI_VALUE].asString());
        }
	}
	else
	{
		LL_WARNS("GridManager")<<"invalid grid '"<<grid<<"'"<<LL_ENDL;
	}
}

void LLGridManager::getLoginURIs(std::vector<std::string>& uris)
{
	getLoginURIs(mGrid, uris);
}

std::string LLGridManager::getHelperURI(const std::string& grid)
{
	std::string helper_uri;
	std::string grid_name = getGrid(grid);
	if (!grid_name.empty())
	{
		helper_uri = mGridList[grid_name][GRID_HELPER_URI_VALUE].asString();
	}
	else
	{
		LL_WARNS("GridManager")<<"invalid grid '"<<grid<<"'"<<LL_ENDL;
	}

	LL_DEBUGS("GridManager")<<"returning "<<helper_uri<<LL_ENDL;
	return helper_uri;
}

std::string LLGridManager::getLoginPage(const std::string& grid)
{
	std::string grid_login_page;
	std::string grid_name = getGrid(grid);
	if (!grid_name.empty())
	{
		grid_login_page = mGridList[grid_name][GRID_LOGIN_PAGE_VALUE].asString();
	}
	else
	{
		LL_WARNS("GridManager")<<"invalid grid '"<<grid<<"'"<<LL_ENDL;
	}
	return grid_login_page;
}

std::string LLGridManager::getLoginPage()
{
	std::string login_page = mGridList[mGrid][GRID_LOGIN_PAGE_VALUE].asString();
	LL_DEBUGS("GridManager")<<"returning "<<login_page<<LL_ENDL;
	return login_page;
}

std::string LLGridManager::getWebProfileURL(const std::string& grid)
{
	std::string web_profile_url;
	std::string grid_name = getGrid(grid);
	if (!grid_name.empty())
	{
		web_profile_url = mGridList[grid_name][GRID_WEB_PROFILE_VALUE].asString();
	}
	else
	{
		LL_WARNS("GridManager")<<"invalid grid '"<<grid<<"'"<<LL_ENDL;
	}
	return web_profile_url;
}

void LLGridManager::getLoginIdentifierTypes(LLSD& idTypes)
{
	idTypes = mGridList[mGrid][GRID_LOGIN_IDENTIFIER_TYPES];
}

std::string LLGridManager::getGridLoginID()
{
	return mGridList[mGrid][GRID_ID_VALUE];
}

std::string LLGridManager::getUpdateServiceURL()
{
	std::string update_url_base = gSavedSettings.getString("CmdLineUpdateService");;
	if ( !update_url_base.empty() )
	{
		LL_INFOS("UpdaterService","GridManager")
			<< "Update URL base overridden from command line: " << update_url_base
			<< LL_ENDL;
	}
	else if ( mGridList[mGrid].has(GRID_UPDATE_SERVICE_URL) )
	{
		update_url_base = mGridList[mGrid][GRID_UPDATE_SERVICE_URL].asString();
	}
	else
	{
		LL_WARNS("UpdaterService","GridManager")
			<< "The grid property '" << GRID_UPDATE_SERVICE_URL
			<< "' is not defined for the grid '" << mGrid << "'"
			<< LL_ENDL;
	}
			
	return update_url_base;
}

void LLGridManager::updateIsInProductionGrid()
{
	mIsInProductionGrid = false;

	// *NOTE:Mani This used to compare GRID_INFO_AGNI to gGridChoice,
	// but it seems that loginURI trumps that.
	std::vector<std::string> uris;
	getLoginURIs(uris);
	if (uris.empty())
	{
		mIsInProductionGrid = true;
	}
	else
	{
		for ( std::vector<std::string>::iterator uri_it = uris.begin();
			  ! mIsInProductionGrid && uri_it != uris.end();
			  uri_it++
			 )
		{
			if( MAIN_GRID_LOGIN_URI == *uri_it )
			{
				mIsInProductionGrid = true;
			}
		}
	}
}

bool LLGridManager::isInProductionGrid()
{
	return mIsInProductionGrid;
}

bool LLGridManager::isSystemGrid(const std::string& grid)
{
	std::string grid_name = getGrid(grid);

	return (   !grid_name.empty()
			&& mGridList.has(grid)
			&& mGridList[grid].has(GRID_IS_SYSTEM_GRID_VALUE)
			&& mGridList[grid][GRID_IS_SYSTEM_GRID_VALUE].asBoolean()
			);
}

// build a slurl for the given region within the selected grid
std::string LLGridManager::getSLURLBase(const std::string& grid)
{
	std::string grid_base = "";
	std::string grid_name = getGrid(grid);
	if( ! grid_name.empty() && mGridList.has(grid_name) )
	{
		if (mGridList[grid_name].has(GRID_SLURL_BASE))
		{
			grid_base = mGridList[grid_name][GRID_SLURL_BASE].asString();
		}
		else
		{
			grid_base = llformat(DEFAULT_SLURL_BASE, grid_name.c_str());
		}
	}
	LL_DEBUGS("GridManager")<<"returning '"<<grid_base<<"'"<<LL_ENDL;
	return grid_base;
}

// build a slurl for the given region within the selected grid
std::string LLGridManager::getAppSLURLBase(const std::string& grid)
{
	std::string grid_base = "";
	std::string grid_name = getGrid(grid);
	if(!grid_name.empty() && mGridList.has(grid))
	{
		if (mGridList[grid].has(GRID_APP_SLURL_BASE))
		{
			grid_base = mGridList[grid][GRID_APP_SLURL_BASE].asString();
		}
		else
		{
			grid_base = llformat(DEFAULT_APP_SLURL_BASE, grid_name.c_str());
		}
	}
	LL_DEBUGS("GridManager")<<"returning '"<<grid_base<<"'"<<LL_ENDL;
	return grid_base;
}

/**
 * @file llviewernetwork_test.cpp
 * @author Roxie
 * @date 2009-03-9
 * @brief Test the viewernetwork functionality
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2014, Linden Research, Inc.
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
#include "../llviewerprecompiledheaders.h"
#include "../llviewernetwork.h"
#include "../test/lltut.h"
#include "../../llxml/llcontrol.h"
#include "llfile.h"
#include "llsdserialize.h"

namespace
{

// Should not collide with other test programs creating temp files.
static const char * const TEST_FILENAME("llviewernetwork_test.xml");

}

//
// Stub implementation for LLTrans
//
class LLTrans
{
public:
    static std::string getString(std::string_view xml_desc, const LLStringUtil::format_map_t& args, bool def_string = false);
};

std::string LLTrans::getString(std::string_view xml_desc, const LLStringUtil::format_map_t& args, bool def_string)
{
    std::string grid_label = std::string();
    if(xml_desc == "AgniGridLabel")
    {
        grid_label = "Second Life Main Grid (Agni)";
    }
    else if(xml_desc == "AditiGridLabel")
    {
        grid_label = "Second Life Beta Test Grid (Aditi)";
    }

    return grid_label;
}

//----------------------------------------------------------------------------
// Mock objects for the dependencies of the code we're testing

LLControlGroup::LLControlGroup(const std::string& name)
: LLInstanceTracker<LLControlGroup, std::string>(name) {}
LLControlGroup::~LLControlGroup() {}
LLControlVariable* LLControlGroup::declareString(const std::string& name,
                                   const std::string& initial_val,
                                   const std::string& comment,
                                   LLControlVariable::ePersist persist) {return NULL;}
void LLControlGroup::setString(std::string_view name, const std::string& val){}

std::string gCmdLineLoginURI;
std::string gCmdLineGridChoice;
std::string gCmdLineHelperURI;
std::string gLoginPage;
std::string gCurrentGrid;
std::string LLControlGroup::getString(std::string_view name)
{
    if (name == "CmdLineGridChoice")
        return gCmdLineGridChoice;
    else if (name == "CmdLineHelperURI")
        return gCmdLineHelperURI;
    else if (name == "LoginPage")
        return gLoginPage;
    else if (name == "CurrentGrid")
        return gCurrentGrid;
    return "";
}

LLSD LLControlGroup::getLLSD(std::string_view name)
{
    if (name == "CmdLineLoginURI")
    {
        if(!gCmdLineLoginURI.empty())
        {
            return LLSD(gCmdLineLoginURI);
        }
    }
    return LLSD();
}

LLPointer<LLControlVariable> LLControlGroup::getControl(std::string_view name)
{
    ctrl_name_table_t::iterator iter = mNameTable.find(name.data());
    return iter == mNameTable.end() ? LLPointer<LLControlVariable>() : iter->second;
}

LLControlGroup gSavedSettings("test");

const char *gSampleGridFile =
    "<?xml version=\"1.0\"?>"
    "<llsd>"
    "  <map>"
    "    <key>altgrid.long.name</key>"
    "    <map>"
    "      <key>helper_uri</key><string>https://helper1/helpers/</string>"
    "      <key>label</key><string>Alternative Grid</string>"
    "      <key>login_page</key><string>altgrid/loginpage</string>"
    "      <key>login_uri</key>"
    "      <array>"
    "        <string>altgrid/myloginuri1</string>"
    "        <string>altgrid/myloginuri2</string>"
    "      </array>"
    "      <key>keyname</key><string>altgrid.long.name</string>"
    "      <key>credential_type</key><string>agent</string>"
    "      <key>grid_login_id</key><string>AltGrid</string>"
    "    </map>"
    "    <key>minimal.long.name</key>"
    "    <map>"
    "      <key>keyname</key><string>minimal.long.name</string>"
    "    </map>"
    "    <!-- Note that the values for agni and aditi below are deliberately"
    "         incorrect to test that they are not overwritten -->"
    "    <key>util.agni.lindenlab.com</key> <!-- conflict -->"
    "    <map>"
    "      <key>helper_uri</key><string>https://helper1/helpers/</string>"
    "      <key>grid_login_id</key><string>mylabel</string>"
    "      <key>label</key><string>mylabel</string>"
    "      <key>login_page</key><string>loginpage</string>"
    "      <key>login_uri</key>"
    "      <array>"
    "        <string>myloginuri</string>"
    "      </array>"
    "      <key>keyname</key><string>util.agni.lindenlab.com</string> <!-- conflict -->"
    "    </map>"
    "    <key>util.foobar.lindenlab.com</key>"
    "    <map>"
    "      <key>helper_uri</key><string>https://helper1/helpers/</string>"
    "      <key>grid_login_id</key><string>Aditi</string> <!-- conflict -->"
    "      <key>label</key><string>mylabel</string>"
    "      <key>login_page</key><string>loginpage</string>"
    "      <key>login_uri</key>"
    "      <array>"
    "        <string>myloginuri</string>"
    "      </array>"
    "      <key>update_query_url_base</key><string>https://update.secondlife.com/update</string>"
    "      <key>keyname</key><string>util.foobar.lindenlab.com</string>"
    "    </map>"
    "  </map>"
    "</llsd>"
    ;
// -------------------------------------------------------------------------------------------
// TUT
// -------------------------------------------------------------------------------------------
namespace tut
{
  // Test wrapper declaration : wrapping nothing for the moment
  struct viewerNetworkTest
    {
        viewerNetworkTest()
        {
            LLFile::remove(TEST_FILENAME);
            gCmdLineLoginURI.clear();
            gCmdLineGridChoice.clear();
            gCmdLineHelperURI.clear();
            gLoginPage.clear();
            gCurrentGrid.clear();
        }
        ~viewerNetworkTest()
        {
            LLFile::remove(TEST_FILENAME);
            LLFile::remove(std::string(TEST_FILENAME) + ".tmp");
        }
    };

    // Tut templating thingamagic: test group, object and test instance
    typedef test_group<viewerNetworkTest> viewerNetworkTestFactory;
    typedef viewerNetworkTestFactory::object viewerNetworkTestObject;
    tut::viewerNetworkTestFactory tut_test("LLViewerNetwork");

    // ---------------------------------------------------------------------------------------
    // Test functions
    // ---------------------------------------------------------------------------------------
    // initialization without a grid file
    template<> template<>
    void viewerNetworkTestObject::test<1>()
    {
        LLGridManager *manager = LLGridManager::getInstance();
        // grid file doesn't exist
        manager->initialize(TEST_FILENAME);
        // validate that some of the defaults are available.
        std::map<std::string, std::string> known_grids = manager->getKnownGrids();
        ensure_equals("Known grids is a string-string map of size 2", known_grids.size(), 2);
        ensure_equals("Agni has the right name and label",
                      known_grids[std::string("util.agni.lindenlab.com")],
                      std::string("Second Life Main Grid (Agni)"));
        ensure_equals("Aditi has the right name and label",
                      known_grids[std::string("util.aditi.lindenlab.com")],
                      std::string("Second Life Beta Test Grid (Aditi)"));
        ensure_equals("name for agni",
                      LLGridManager::getInstance()->getGrid("util.agni.lindenlab.com"),
                      std::string("util.agni.lindenlab.com"));
        ensure_equals("id for agni",
                      std::string("Agni"),
                      LLGridManager::getInstance()->getGridId("util.agni.lindenlab.com"));
        ensure_equals("update url base for Agni", // relies on agni being the default
                      std::string("https://update.secondlife.com/update"),
                      LLGridManager::getInstance()->getUpdateServiceURL());
        ensure_equals("label for agni",
                      LLGridManager::getInstance()->getGridLabel("util.agni.lindenlab.com"),
                      std::string("Second Life Main Grid (Agni)"));

        std::vector<std::string> login_uris;
        LLGridManager::getInstance()->getLoginURIs(std::string("util.agni.lindenlab.com"), login_uris);
        ensure_equals("Number of login uris for agni", 1, login_uris.size());
        ensure_equals("Agni login uri",
                      login_uris[0],
                      std::string("https://login.agni.lindenlab.com/cgi-bin/login.cgi"));
        ensure_equals("Agni helper uri",
                      LLGridManager::getInstance()->getHelperURI("util.agni.lindenlab.com"),
                      std::string("https://secondlife.com/helpers/"));
        ensure_equals("Agni login page",
                      LLGridManager::getInstance()->getLoginPage("util.agni.lindenlab.com"),
                      std::string("https://viewer-splash.secondlife.com/"));
        ensure("Agni is a system grid",
               LLGridManager::getInstance()->isSystemGrid("util.agni.lindenlab.com"));

        ensure_equals("name for aditi",
                      LLGridManager::getInstance()->getGrid("util.aditi.lindenlab.com"),
                      std::string("util.aditi.lindenlab.com"));
        ensure_equals("id for aditi",
                      LLGridManager::getInstance()->getGridId("util.aditi.lindenlab.com"),
                      std::string("Aditi"));
        ensure_equals("label for aditi",
                      LLGridManager::getInstance()->getGridLabel("util.aditi.lindenlab.com"),
                      std::string("Second Life Beta Test Grid (Aditi)"));

        LLGridManager::getInstance()->getLoginURIs(std::string("util.aditi.lindenlab.com"), login_uris);

        ensure_equals("Number of login uris for aditi", 1, login_uris.size());
        ensure_equals("Aditi login uri",
                      login_uris[0],
                      std::string("https://login.aditi.lindenlab.com/cgi-bin/login.cgi"));
        ensure_equals("Aditi helper uri",
                      LLGridManager::getInstance()->getHelperURI("util.aditi.lindenlab.com"),
                      std::string("https://secondlife.aditi.lindenlab.com/helpers/"));
        ensure_equals("Aditi login page",
                      LLGridManager::getInstance()->getLoginPage("util.aditi.lindenlab.com"),
                      std::string("https://viewer-splash.secondlife.com/"));
        ensure("Aditi is a system grid",
               LLGridManager::getInstance()->isSystemGrid("util.aditi.lindenlab.com"));
    }

    // initialization with a grid file
    template<> template<>
    void viewerNetworkTestObject::test<2>()
    {
        llofstream gridfile(TEST_FILENAME);
        gridfile << gSampleGridFile;
        gridfile.close();

        LLGridManager::getInstance()->initialize(TEST_FILENAME);
        std::map<std::string, std::string> known_grids = LLGridManager::getInstance()->getKnownGrids();
        ensure_equals("adding a grid via a grid file increases known grid size",4,
                      known_grids.size());

        // Verify that Agni and Aditi were not overwritten
        ensure_equals("Agni has the right name and label",
                      known_grids[std::string("util.agni.lindenlab.com")],
                      std::string("Second Life Main Grid (Agni)"));
        ensure_equals("Aditi has the right name and label",
                      known_grids[std::string("util.aditi.lindenlab.com")],
                      std::string("Second Life Beta Test Grid (Aditi)"));
        ensure_equals("name for agni",
                      LLGridManager::getInstance()->getGrid("util.agni.lindenlab.com"),
                      std::string("util.agni.lindenlab.com"));
        ensure_equals("id for agni",
                      LLGridManager::getInstance()->getGridId("util.agni.lindenlab.com"),
                      std::string("Agni"));
        ensure_equals("update url base for Agni", // relies on agni being the default
                      std::string("https://update.secondlife.com/update"),
                      LLGridManager::getInstance()->getUpdateServiceURL());
        ensure_equals("label for agni",
                      LLGridManager::getInstance()->getGridLabel("util.agni.lindenlab.com"),
                      std::string("Second Life Main Grid (Agni)"));
        std::vector<std::string> login_uris;
        LLGridManager::getInstance()->getLoginURIs(std::string("util.agni.lindenlab.com"), login_uris);
        ensure_equals("Number of login uris for agni", 1, login_uris.size());
        ensure_equals("Agni login uri",
                      login_uris[0],
                      std::string("https://login.agni.lindenlab.com/cgi-bin/login.cgi"));
        ensure_equals("Agni helper uri",
                      LLGridManager::getInstance()->getHelperURI("util.agni.lindenlab.com"),
                      std::string("https://secondlife.com/helpers/"));
        ensure_equals("Agni login page",
                      LLGridManager::getInstance()->getLoginPage("util.agni.lindenlab.com"),
                      std::string("https://viewer-splash.secondlife.com/"));
        ensure("Agni is a system grid",
               LLGridManager::getInstance()->isSystemGrid("util.agni.lindenlab.com"));

        ensure_equals("name for aditi",
                      LLGridManager::getInstance()->getGrid("util.aditi.lindenlab.com"),
                      std::string("util.aditi.lindenlab.com"));
        ensure_equals("id for aditi",
                      LLGridManager::getInstance()->getGridId("util.aditi.lindenlab.com"),
                      std::string("Aditi"));
        ensure_equals("label for aditi",
                      LLGridManager::getInstance()->getGridLabel("util.aditi.lindenlab.com"),
                      std::string("Second Life Beta Test Grid (Aditi)"));

        LLGridManager::getInstance()->getLoginURIs(std::string("util.aditi.lindenlab.com"), login_uris);
        ensure_equals("Number of login uris for aditi", 1, login_uris.size());
        ensure_equals("Aditi login uri",
                      login_uris[0],
                      std::string("https://login.aditi.lindenlab.com/cgi-bin/login.cgi"));
        ensure_equals("Aditi helper uri",
                      LLGridManager::getInstance()->getHelperURI("util.aditi.lindenlab.com"),
                      std::string("https://secondlife.aditi.lindenlab.com/helpers/"));
        ensure_equals("Aditi login page",
                      LLGridManager::getInstance()->getLoginPage("util.aditi.lindenlab.com"),
                      std::string("https://viewer-splash.secondlife.com/"));
        ensure("Aditi is a system grid",
               LLGridManager::getInstance()->isSystemGrid("util.aditi.lindenlab.com"));

        // Check the additional grid from the file
        ensure_equals("alternative grid is in name<->label map",
                      known_grids["altgrid.long.name"],
                      std::string("Alternative Grid"));
        ensure_equals("alternative grid name is set",
                      LLGridManager::getInstance()->getGrid("altgrid.long.name"),
                      std::string("altgrid.long.name"));
        ensure_equals("alternative grid id",
                      LLGridManager::getInstance()->getGridId("altgrid.long.name"),
                      std::string("AltGrid"));
        ensure_equals("alternative grid label",
                      LLGridManager::getInstance()->getGridLabel("altgrid.long.name"),
                      std::string("Alternative Grid"));
        std::vector<std::string> alt_login_uris;
        LLGridManager::getInstance()->getLoginURIs(std::string("altgrid.long.name"), alt_login_uris);
        ensure_equals("Number of login uris for altgrid", 2, alt_login_uris.size());
        ensure_equals("alternative grid first login uri",
                      alt_login_uris[0],
                      std::string("altgrid/myloginuri1"));
        ensure_equals("alternative grid second login uri",
                      alt_login_uris[1],
                      std::string("altgrid/myloginuri2"));
        ensure_equals("alternative grid helper uri",
                      LLGridManager::getInstance()->getHelperURI("altgrid.long.name"),
                      std::string("https://helper1/helpers/"));
        ensure_equals("alternative grid login page",
                      LLGridManager::getInstance()->getLoginPage("altgrid.long.name"),
                      std::string("altgrid/loginpage"));
        ensure("alternative grid is NOT a system grid",
               ! LLGridManager::getInstance()->isSystemGrid("altgrid.long.name"));

        ensure_equals("minimal grid is in name<->label map",
                      known_grids["minimal.long.name"],
                      std::string("minimal.long.name"));
        ensure_equals("minimal grid name is set",
                      LLGridManager::getInstance()->getGrid("minimal.long.name"),
                      std::string("minimal.long.name"));
        ensure_equals("minimal grid id",
                      LLGridManager::getInstance()->getGridId("minimal.long.name"),
                      std::string("minimal.long.name"));
        ensure_equals("minimal grid label",
                      LLGridManager::getInstance()->getGridLabel("minimal.long.name"),
                      std::string("minimal.long.name"));

        LLGridManager::getInstance()->getLoginURIs(std::string("minimal.long.name"), alt_login_uris);
        ensure_equals("Number of login uris for altgrid", 1, alt_login_uris.size());
        ensure_equals("minimal grid login uri",
                      alt_login_uris[0],
                      std::string("https://minimal.long.name/cgi-bin/login.cgi"));
        ensure_equals("minimal grid helper uri",
                      LLGridManager::getInstance()->getHelperURI("minimal.long.name"),
                      std::string("https://minimal.long.name/helpers/"));
        ensure_equals("minimal grid login page",
                      LLGridManager::getInstance()->getLoginPage("minimal.long.name"),
                      std::string("http://minimal.long.name/app/login/"));

    }


    template<> template<>
    void viewerNetworkTestObject::test<3>()
    {
        LLGridManager* manager = LLGridManager::getInstance();
        manager->initialize(TEST_FILENAME);
        std::string error;
        ensure("add explicit OpenSim URI", manager->addCustomGrid("http://localhost:8002/", "Local OpenSim", error));
        ensure("success clears error", error.empty());
        ensure_equals("label", manager->getGridLabel("localhost:8002"), std::string("Local OpenSim"));
        std::vector<std::string> uris;
        manager->getLoginURIs("localhost:8002", uris);
        ensure_equals("one endpoint", uris.size(), size_t(1));
        ensure_equals("no invented CGI path", uris[0], std::string("http://localhost:8002/"));

        LLSD saved;
        llifstream stream(TEST_FILENAME);
        LLSDSerialize::fromXMLDocument(saved, stream);
        stream.close();
        ensure("only custom grids are saved", saved.isMap() && saved.size() == 1);
        ensure("Agni is not serialized", !saved.has(MAINGRID));
        manager->initialize(TEST_FILENAME);
        ensure_equals("survives reload", manager->getGridLabel("localhost:8002"), std::string("Local OpenSim"));
        ensure("duplicate rejected", !manager->addCustomGrid("http://LOCALHOST:8002/", "Duplicate", error));
        manager->setGridChoice("localhost:8002");
        ensure("selected grid protected", !manager->removeCustomGrid("localhost:8002", error));
        manager->setGridChoice(MAINGRID);
        ensure("remove custom grid", manager->removeCustomGrid("localhost:8002", error));
        manager->initialize(TEST_FILENAME);
        ensure("removal survives reload", manager->getGrid("localhost:8002").empty());
    }

    template<> template<>
    void viewerNetworkTestObject::test<4>()
    {
        LLGridManager* manager = LLGridManager::getInstance();
        manager->initialize(TEST_FILENAME);
        const char* invalid[] = {
            "", "example.org:8002", "ftp://example.org/", "https:///",
            "https://user:password@example.org/", "https://example.org/?token=private",
            "https://example.org/#fragment", "http://example.org:0/",
            "http://example.org:65536/", "http://example.org:8002junk/",
            "http://example.org:/", "http://example.org:123456789/",
            "http://bad host:8002/", "http://example.org\\other/"
        };
        std::string error;
        for (const char* endpoint : invalid)
        {
            ensure("invalid endpoint rejected", !manager->addCustomGrid(endpoint, "Invalid", error));
            ensure("error explains rejection", !error.empty());
        }
        ensure("system grid cannot be replaced", !manager->addCustomGrid("https://util.agni.lindenlab.com/", "Fake SL", error));
        ensure("Agni cannot be removed", !manager->removeCustomGrid("Agni", error));
        ensure("Aditi cannot be removed", !manager->removeCustomGrid("Aditi", error));
        ensure_equals("built-in list intact", manager->getKnownGrids().size(), size_t(2));
    }

    template<> template<>
    void viewerNetworkTestObject::test<5>()
    {
        LLGridManager* manager = LLGridManager::getInstance();
        // No destination forces a persistence failure without touching user data.
        manager->initialize("");
        std::string error;
        ensure("save failure reported", !manager->addCustomGrid("https://example.org:8002/Login", "Example", error));
        ensure("failed addition rolled back", manager->getGrid("example.org:8002").empty());

        llofstream gridfile(TEST_FILENAME);
        gridfile << gSampleGridFile;
        gridfile.close();
        manager->initialize(TEST_FILENAME);
        ensure("unknown grid rejected", !manager->removeCustomGrid("not-a-grid", error));
        ensure("existing custom grid preserved", !manager->getGrid("altgrid.long.name").empty());
    }

    template<> template<>
    void viewerNetworkTestObject::test<6>()
    {
        LLGridManager* manager = LLGridManager::getInstance();
        manager->initialize(TEST_FILENAME);
        std::string error;
        ensure("add grid before failed removal", manager->addCustomGrid("http://localhost:8002/", "Local", error));
        // A directory at the temporary-file path forces stream creation to fail
        // on both Windows and POSIX, without depending on permission semantics.
        const std::string temporary = std::string(TEST_FILENAME) + ".tmp";
        const int created = LLFile::mkdir(temporary);
        ensure_equals("create blocking directory", created, 0);
        const bool removed = manager->removeCustomGrid("localhost:8002", error);
        LLFile::rmdir(temporary);
        ensure("failed save prevents removal", !removed);
        ensure("in-memory grid restored", !manager->getGrid("localhost:8002").empty());
        manager->initialize(TEST_FILENAME);
        ensure("previous disk configuration intact", !manager->getGrid("localhost:8002").empty());
    }

    // validate grid selection
    template<> template<>
    void viewerNetworkTestObject::test<7>()
    {
        // adding a grid with simply a name will populate the values.
        llofstream gridfile(TEST_FILENAME);
        gridfile << gSampleGridFile;
        gridfile.close();

        LLGridManager::getInstance()->initialize(TEST_FILENAME);

        LLGridManager::getInstance()->setGridChoice("util.agni.lindenlab.com");
        ensure_equals("getGridLabel",
                      LLGridManager::getInstance()->getGridLabel(),
                      std::string("Second Life Main Grid (Agni)"));
        ensure_equals("getGridId",
                      LLGridManager::getInstance()->getGridId(),
                      std::string("Agni"));
        ensure_equals("getGrid",
                      LLGridManager::getInstance()->getGrid(),
                      std::string("util.agni.lindenlab.com"));
        ensure_equals("getHelperURI",
                      LLGridManager::getInstance()->getHelperURI(),
                      std::string("https://secondlife.com/helpers/"));
        ensure_equals("getLoginPage",
                      LLGridManager::getInstance()->getLoginPage(),
                      std::string("https://viewer-splash.secondlife.com/"));
        ensure_equals("update url base for Agni", // relies on agni being the default
                      std::string("https://update.secondlife.com/update"),
                      LLGridManager::getInstance()->getUpdateServiceURL());
        ensure("Is Agni a production grid", LLGridManager::getInstance()->isInProductionGrid());
        ensure("Agni uses Second Life compatibility", LLGridManager::getInstance()->isInSecondLife());
        ensure("Agni does not use OpenSim compatibility", !LLGridManager::getInstance()->isInOpenSim());
        std::vector<std::string> uris;
        LLGridManager::getInstance()->getLoginURIs(uris);
        ensure_equals("getLoginURIs size", 1, uris.size());
        ensure_equals("getLoginURIs",
                      uris[0],
                      std::string("https://login.agni.lindenlab.com/cgi-bin/login.cgi"));

        LLGridManager::getInstance()->setGridChoice("altgrid.long.name");
        ensure_equals("getGridLabel",
                      LLGridManager::getInstance()->getGridLabel(),
                      std::string("Alternative Grid"));
        ensure_equals("getGridId",
                      LLGridManager::getInstance()->getGridId(),
                      std::string("AltGrid"));
        ensure("alternative grid is not a system grid",
               !LLGridManager::getInstance()->isSystemGrid());
        ensure("alternative grid is not a production grid",
               !LLGridManager::getInstance()->isInProductionGrid());
        ensure("alternative grid uses OpenSim compatibility", LLGridManager::getInstance()->isInOpenSim());
        ensure("alternative grid is not Second Life", !LLGridManager::getInstance()->isInSecondLife());

        LLGridManager::getInstance()->setGridChoice("util.aditi.lindenlab.com");
        ensure("Aditi uses Second Life compatibility", LLGridManager::getInstance()->isInSecondLife());
        ensure("Aditi does not use OpenSim compatibility", !LLGridManager::getInstance()->isInOpenSim());
    }

    template<> template<>
    void viewerNetworkTestObject::test<8>()
    {
        LLGridManager* manager = LLGridManager::getInstance();
        manager->initialize(TEST_FILENAME);
        std::string error;
        LLSD first = LLSD::emptyMap();
        first["helper_uri"] = "https://one.example/helpers/";
        first["currency_uri"] = "https://one.example/economy/currency.php";
        LLSD second = LLSD::emptyMap();
        second["helper_uri"] = "https://two.example/economy/";
        ensure("first grid added", manager->addCustomGrid("https://one.example:8002/", "One", error, first));
        ensure("second grid added", manager->addCustomGrid("https://two.example:8002/", "Two", error, second));

        manager->setGridChoice("one.example:8002");
        ensure_equals("explicit currency endpoint", manager->getCurrencyHelperURI(),
            std::string("https://one.example/economy/currency.php"));
        LLSD clear_currency = LLSD::emptyMap();
        clear_currency["currency_uri"] = "";
        ensure("empty optional currency endpoint clears", manager->setGridMetadata("one.example:8002", clear_currency, error));
        ensure_equals("empty optional currency endpoint uses helper fallback", manager->getCurrencyHelperURI(),
            std::string("https://one.example/helpers/currency.php"));
        ensure_equals("first grid land endpoint", manager->getLandTransactionURI(),
            std::string("https://one.example/helpers/landtool.php"));
        manager->setGridChoice("two.example:8002");
        ensure_equals("helper fallback trims trailing slash", manager->getCurrencyHelperURI(),
            std::string("https://two.example/economy/currency.php"));
        ensure_equals("second grid land endpoint", manager->getLandTransactionURI(),
            std::string("https://two.example/economy/landtool.php"));

        LLSD invalid = LLSD::emptyMap();
        invalid["helper_uri"] = "https://user:secret@two.example/helpers/";
        ensure("invalid helper rejected", !manager->setGridMetadata("two.example:8002", invalid, error));
        invalid = LLSD::emptyMap();
        invalid["currency_uri"] = "https://two.example/currency.php?redirect=elsewhere";
        ensure("query-bearing currency endpoint rejected", !manager->setGridMetadata("two.example:8002", invalid, error));
        invalid["currency_uri"] = "https://two.example:99999/currency.php";
        ensure("invalid-port currency endpoint rejected", !manager->setGridMetadata("two.example:8002", invalid, error));
        invalid = LLSD::emptyMap();
        invalid["helper_uri"] = "two.example/economy/";
        ensure("helper without scheme rejected", !manager->setGridMetadata("two.example:8002", invalid, error));
        invalid["helper_uri"] = "https:///economy/";
        ensure("helper without host rejected", !manager->setGridMetadata("two.example:8002", invalid, error));
        invalid["helper_uri"] = "https://two.example/economy/#currency.php";
        // Grid metadata URLs must be absolute; fragments are never endpoint data.
        ensure("helper fragment rejected", !manager->setGridMetadata("two.example:8002", invalid, error));
        invalid["helper_uri"] = "/economy/";
        ensure("relative helper path rejected", !manager->setGridMetadata("two.example:8002", invalid, error));
        ensure_equals("invalid helper cannot route transaction", manager->getLandTransactionURI(),
            std::string("https://two.example/economy/landtool.php"));
    }

}

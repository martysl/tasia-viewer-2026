#!/usr/bin/env python3
"""OpenSim service compatibility: profile, currency, marketplace, grid list.

Production-source contract tests plus a compiled legacy-profile mapping check.
No live grid, HTTP service or GUI claim is made here.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET
from tasia_portable_branding_test import method

ROOT = Path(__file__).resolve().parents[3]
NV = ROOT/'indra/newview'


def compile_run(source):
    with tempfile.TemporaryDirectory(dir='/tmp/opencode',prefix='opensim-svc-') as tmp:
        cpp, exe = Path(tmp)/'test.cpp', Path(tmp)/'test'
        cpp.write_text(source)
        subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',
                        '-fsanitize=address,undefined',str(cpp),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)


class OpenSimServices(unittest.TestCase):
    def test_remote_profile_customization_is_rendered_safely(self):
        profile = (NV/'llpanelprofile.cpp').read_text()
        avatar = (NV/'llvoavatar.cpp').read_text()
        thumbnail = (NV/'llthumbnailctrl.cpp').read_text()
        self.assertIn('tasia_user.getNametagTitle()', avatar)
        rank_line = avatar.index('addNameTagLine(tasia_title, name_tag_color')
        self.assertLess(avatar.index('addNameTagLine(LLTasiaUserConfig::renderUsername'), rank_line)
        self.assertLess(avatar.index('addNameTagLine(tasia_user.cosmetic_username'), rank_line)
        self.assertIn('tasia_user.has_tag_color) name_tag_color = tasia_user.tag_color', avatar)
        self.assertIn('user.profile_text', profile)
        self.assertIn('user.tooltip', profile)
        self.assertIn('user.badge_icon', profile)
        self.assertIn('options->setFollowRedirects(false)', profile)
        self.assertIn('options->setBodyLimit(TASIA_PROFILE_BADGE_MAX_BYTES)', profile)
        self.assertIn('headers->append(std::string(HTTP_OUT_HEADER_ACCEPT)', profile)
        self.assertIn('LLCore::HttpStatus(HTTP_OK)', profile)
        self.assertIn('setTexture(LLViewerFetchedTexture* texture', thumbnail)
        xml = ET.parse(NV/'skins/default/xui/en/panel_profile_secondlife.xml').getroot()
        names = {node.attrib.get('name') for node in xml.iter()}
        self.assertTrue({'tasia_profile_badge_layout', 'tasia_profile_badge_icon',
                         'tasia_profile_badge_fallback', 'account_info'} <= names)

    def test_legacy_profile_mapping_copies_only_present_fields(self):
        header = (NV/'lltasia_profile_compat.h').read_text()
        fields = header[header.index('inline LLAvatarData tasiaLegacyProfileFields('):]
        fixture = r'''
#include <cassert>
#include <list>
#include <string>
#include <utility>
using U8=unsigned char;using U32=unsigned;using U64=unsigned long long;
struct LLUUID{std::string s;bool isNull()const{return s.empty();}bool operator==(const LLUUID& o)const{return s==o.s;}};
struct LLDate{};
struct LLAvatarData
{
    struct LLGroupData { U64 group_powers; bool accept_notices; std::string group_title;
                         LLUUID group_id; std::string group_name; LLUUID group_insignia_id; };
    LLUUID      agent_id; LLUUID avatar_id; LLUUID image_id; LLUUID fl_image_id; LLUUID partner_id;
    std::string about_text; std::string fl_about_text; LLDate born_on; std::string profile_url;
    U8 caption_index; std::string caption_text; std::string customer_type; U32 flags;
    bool hide_age = false; std::string notes;
    typedef std::list<LLGroupData> group_list_t; group_list_t group_list;
    typedef std::pair<LLUUID, std::string> pick_data_t; typedef std::list<pick_data_t> picks_list_t; picks_list_t picks_list;
};
struct LLAvatarLegacyData
{
    LLUUID agent_id; LLUUID avatar_id; LLUUID image_id; LLUUID fl_image_id; LLUUID partner_id;
    std::string about_text; std::string fl_about_text; LLDate born_on; std::string profile_url;
    U8 caption_index; std::string caption_text; std::string customer_type; U32 flags;
};
''' + fields + r'''
int main(){ LLAvatarData full=tasiaLegacyProfileFields(LLAvatarLegacyData{});
 assert(full.group_list.empty()&&full.picks_list.empty()&&full.notes.empty()); }
'''
        compile_run(fixture)

    def test_profile_request_falls_back_to_legacy_on_opensim(self):
        source = (NV/'llavatarpropertiesprocessor.cpp').read_text()
        send = method(source,'void LLAvatarPropertiesProcessor::sendRequest(')
        self.assertIn('isInOpenSim()', send)
        self.assertIn('sendAvatarLegacyPropertiesRequest(avatar_id)', send)
        self.assertIn('route=AgentProfile-HTTP', send)
        self.assertIn('route=legacy-UDP reason=AgentProfile-cap-absent', send)
        coro = method(source,'void LLAvatarPropertiesProcessor::requestAvatarPropertiesCoro(')
        self.assertIn('isInOpenSim()', coro)
        self.assertIn('sendAvatarLegacyPropertiesRequest(avatar_id)', coro)
        self.assertIn('gAgent.getSessionID() != session', coro)
        self.assertIn('Profile AgentProfile-HTTP result status=', coro)
        self.assertIn('Profile AgentProfile-HTTP parsed groups=', coro)
        processor = (NV/'llavatarpropertiesprocessor.cpp').read_text()
        legacy_reply = method(processor,'void LLAvatarPropertiesProcessor::processAvatarLegacyPropertiesReply(')
        self.assertIn('reply=AvatarPropertiesReply', legacy_reply)
        panel_base = (NV/'llpanelavatar.cpp').read_text()
        self.assertIn('Profile tab completion panel=', panel_base)
        panel = (NV/'llpanelprofile.cpp').read_text()
        profile = method(panel,'void LLPanelProfile::updateData(')
        self.assertIn('Profile floater loading started', profile)
        self.assertIn('APT_PROPERTIES_LEGACY', panel)
        self.assertIn('tasiaLegacyProfileFields', panel)
        picks = (NV/'llpanelprofilepicks.cpp').read_text()
        picks_handler = method(picks,'void LLPanelProfilePicks::processProperties(')
        self.assertIn('APT_PICKS', picks_handler)
        self.assertIn('profile_data.picks_list = picks.picks_list', picks_handler)
        picks_update = method(picks,'void LLPanelProfilePicks::updateData(')
        self.assertIn('sendAvatarPicksRequest(avatar_id)', picks_update)
        notes_handler = method(panel,'void LLPanelProfileNotes::processProperties(')
        self.assertIn('APT_NOTES', notes_handler)
        self.assertIn('profile_data.notes = notes.notes', notes_handler)
        self.assertIn('setLoaded()', method(panel,'void LLPanelProfileNotes::processProperties(const LLAvatarData*'))
        notes_update = method(panel,'void LLPanelProfileNotes::updateData(')
        self.assertIn('sendAvatarNotesRequest(avatar_id)', notes_update)
        self.assertIn('sendNotes(getAvatarId(), mCurrentNotes)', panel)
        self.assertNotIn('mPanelPicks->setIsLoading()', method(panel,'void LLPanelProfile::updateData('))
        self.assertNotIn('mPanelNotes->setIsLoading()', method(panel,'void LLPanelProfile::updateData('))

    def test_legacy_self_profile_updates_preserve_full_record(self):
        processor_source = (NV/'llavatarpropertiesprocessor.cpp').read_text()
        sender = method(processor_source,
                        'bool LLAvatarPropertiesProcessor::sendLegacyAvatarPropertiesUpdate(')
        for guard in ('isInOpenSim()', 'getRegionCapability("AgentProfile").empty()',
                      'mHasLegacySelfProfile', 'mLegacySelfProfile.avatar_id != gAgent.getID()',
                      'mLegacySelfProfileSessionID != gAgent.getSessionID()'):
            self.assertIn(guard, sender)
        for field in ('profile_url', 'image_id', 'fl_image_id', 'about_text',
                      'fl_about_text', 'allow_publish'):
            self.assertIn(f'"{field}"', sender)
        for packet_field in ('_PREHASH_ImageID', '_PREHASH_FLImageID',
                             '_PREHASH_AboutText', '_PREHASH_FLAboutText',
                             '_PREHASH_AllowPublish', '_PREHASH_MaturePublish',
                             '_PREHASH_ProfileURL'):
            self.assertIn(packet_field, sender)
        legacy_reply = method(processor_source,
                              'void LLAvatarPropertiesProcessor::processAvatarLegacyPropertiesReply(')
        self.assertIn('avatar_data.agent_id == gAgentID && avatar_data.avatar_id == gAgentID', legacy_reply)
        self.assertIn('mLegacySelfProfile = avatar_data', legacy_reply)

        profile = (NV/'llpanelprofile.cpp').read_text()
        legacy_helper = method(profile, 'static bool saveLegacyProfileField(')
        self.assertIn('avatar_id != gAgent.getID()', legacy_helper)
        for legacy_field in ('about_text', 'image_id', 'allow_publish',
                             'fl_about_text', 'fl_image_id'):
            self.assertIn(f'getAvatarId(), "{legacy_field}"', profile)
        # hide_age is absent from AvatarPropertiesUpdate and must not be sent
        # through this legacy fallback.
        self.assertNotIn('saveLegacyProfileField("hide_age"', profile)

    def test_legacy_profile_text_passes_snapshot_to_protocol_field_limits(self):
        processor_source = (NV/'llavatarpropertiesprocessor.cpp').read_text()
        sender = method(processor_source,
                        'bool LLAvatarPropertiesProcessor::sendLegacyAvatarPropertiesUpdate(')
        self.assertIn('addStringFast(_PREHASH_AboutText, mLegacySelfProfile.about_text)', sender)
        self.assertIn('addStringFast(_PREHASH_FLAboutText, mLegacySelfProfile.fl_about_text)', sender)
        self.assertNotIn('.substr(', sender)
        self.assertNotIn('truncate', sender.lower())

        template = (ROOT/'scripts/messages/message_template.msg').read_text()
        update = template[template.index('// AvatarPropertiesUpdate'):]
        update = update[:update.index('// AvatarInterestsUpdate')]
        self.assertRegex(update, r'AboutText\s+Variable 2')
        self.assertRegex(update, r'FLAboutText\s+Variable 1')

        # The shared serializer's 1-byte variable-field limit is 255 bytes total,
        # including its terminating NUL; this is a wire bound, not a sender trim.
        builder = (ROOT/'indra/llmessage/lltemplatemessagebuilder.cpp').read_text()
        clamp = builder[builder.index('// Variable 1 can only store 255 bytes'):
                        builder.index('// no correct size for MVT_VARIABLE')]
        self.assertIn('size > 255', clamp)
        self.assertIn('size = 255', clamp)
        self.assertIn('data_to_add = truncated_variable_data', clamp)
        self.assertIn('truncated_variable_data[254] = 0', clamp)
        self.assertNotIn('(char *)data', clamp)

    def test_search_uses_grid_scoped_opensim_endpoint(self):
        source = (NV/'llfloatersearch.cpp').read_text()
        endpoint = method(source, 'std::string getSearchEndpoint(')
        validator = method(source, 'bool isUsableSearchEndpoint(')
        self.assertIn('endpoint.size() > 2048', validator)
        self.assertIn('uri.scheme() == "http" || uri.scheme() == "https"', validator)
        self.assertIn('uri.authority().find(\'@\')', validator)
        precedence = [endpoint.index('extras["search-server-url"]'),
                      endpoint.index('getResponse("search")'),
                      endpoint.index('grid_info["search"]'),
                      endpoint.index('SearchURLOpenSim')]
        self.assertEqual(precedence, sorted(precedence))
        self.assertIn('authSuccess()', endpoint)
        self.assertIn('return gSavedSettings.getString("SearchURL")', endpoint)
        self.assertIn('isInSecondLife()', endpoint)
        self.assertIn('isInOpenSim()', endpoint)

        initiate = method(source, 'void LLFloaterSearch::initiateSearch(')
        self.assertIn('std::string url = getSearchEndpoint()', initiate)
        self.assertLess(initiate.index('if (url.empty())'),
                        initiate.index('mWebBrowser->navigateTo(url'))
        self.assertNotIn('gSavedSettings.getString("SearchURL")', initiate)

        settings = ET.parse(NV/'app_settings/settings.xml').getroot()
        names = {node.text for node in settings.iter('key')}
        self.assertIn('SearchURLOpenSim', names)

    def test_destinations_uses_grid_scoped_endpoint_and_login_fields(self):
        source = (NV/'llfloaterdestinations.cpp').read_text()
        endpoint = method(source, 'std::string getDestinationGuideEndpoint(')
        self.assertIn('isInSecondLife()', endpoint)
        self.assertIn('isInOpenSim()', endpoint)
        self.assertIn('getString("DestinationGuideURL")', endpoint)
        self.assertIn('extras["destination-guide-url"]', endpoint)
        self.assertIn('getResponse("destination_guide_url")', endpoint)
        self.assertIn('authSuccess()', endpoint)
        self.assertNotIn('getString("DestinationGuideURL")',
                         endpoint[endpoint.index('if (LLViewerRegion'):])

        floater = method(source, 'void LLFloaterDestinations::navigateToDestinationGuide(')
        self.assertIn('if (endpoint.empty())', floater)
        self.assertIn('navigateTo("about:blank"', floater)
        self.assertIn('if (url == mDestinationGuideURL) return', floater)
        invalid_expanded = floater.index('if (!isUsableDestinationGuideURL(url))')
        self.assertIn('navigateTo("about:blank"', floater[invalid_expanded:])

        login = (NV/'lllogininstance.cpp').read_text()
        options = method(login, 'void LLLoginInstance::constructAuthParams(')
        for option in ('"search"', '"destination_guide_url"'):
            self.assertIn(f'requested_options.append({option})', options)
        self.assertIn('isInOpenSim()', options)

    def test_opensim_legacy_profile_reply_family_is_consumed(self):
        processor = (NV/'llavatarpropertiesprocessor.cpp').read_text()
        cases = {
            'processAvatarNotesReply': ('APT_NOTES', 'removePendingRequest', 'notifyObservers'),
            'processAvatarPicksReply': ('APT_PICKS', 'PickID', 'PickName', 'notifyObservers'),
            'processAvatarGroupsReply': ('APT_GROUPS', 'GroupTitle', 'GroupID', 'notifyObservers'),
        }
        for name, markers in cases.items():
            handler = method(processor, f'void LLAvatarPropertiesProcessor::{name}(')
            for marker in markers:
                self.assertIn(marker, handler, (name, marker))

        profile = (NV/'llpanelprofile.cpp').read_text()
        second_life = method(profile,'void LLPanelProfileSecondLife::processProperties(')
        self.assertIn('APT_GROUPS', second_life)
        self.assertIn('groups.group_list', second_life)

        startup = (NV/'llstartup.cpp').read_text()
        for message in ('AvatarPropertiesReply', 'AvatarInterestsReply', 'AvatarGroupsReply',
                        'AvatarNotesReply', 'AvatarPicksReply', 'AvatarClassifiedReply'):
            self.assertIn(f'setHandlerFunc("{message}"', startup)

        aggregate = method((NV/'llpanelprofileclassifieds.cpp').read_text(),
                           'void LLPanelProfileClassifieds::processProperties(')
        self.assertNotIn('APT_CLASSIFIED_INFO', aggregate)

    def test_own_profile_refreshes_after_remote_identity_catalog(self):
        source = (NV/'lltasia_user_config.cpp').read_text()
        callback = method(source,'void LLTasiaUserConfig::requestOnce()')
        self.assertIn('applyConfig(LlsdFromJson(json), source_grid)', callback)
        self.assertIn('findTypedInstance<LLFloaterProfile>', callback)
        self.assertIn('self_profile->refreshName()', callback)

    def test_music_stop_is_in_avatar_menu_without_changing_status_bar(self):
        source = (NV/'llviewermenu.cpp').read_text()
        handler = method(source,'class LLAudioStopMusic : public view_listener_t')
        self.assertIn('setParcelAudioStoppedByUser(true)', handler)
        self.assertIn('stopInternetStreamWithAutoFade()', handler)
        self.assertIn('player->stopFromMediaControls()', handler)
        self.assertIn('"Audio.StopMusic"', source)
        menu = (NV/'skins/default/xui/en/menu_viewer.xml').read_text()
        self.assertIn('label="Stop Music"', menu)
        self.assertIn('function="Audio.StopMusic"', menu)
        self.assertIn('name="Stop Music"', (NV/'skins/default/xui/pl/menu_viewer.xml').read_text())

    def test_currency_never_reuses_another_grid_helper(self):
        source = (NV/'llcurrencyuimanager.cpp').read_text()
        start = method(source,'void LLCurrencyUIManager::Impl::startTransaction(')
        self.assertNotIn('static std::string transactionURI', start)
        self.assertIn('getCurrencyHelperURI()', start)
        self.assertIn('transactionURI.empty()', start)
        self.assertIn('setError("This grid has no valid currency helper configured."', start)
        network = (NV/'llviewernetwork.cpp').read_text()
        helper = method(network,'std::string LLGridManager::getCurrencyHelperURI(')
        self.assertIn('currency.php', helper)
        self.assertIn('gridURL(', helper)
        self.assertIn('currency-base-uri', helper)
        self.assertIn('trusted_host', helper)
        self.assertIn('advertised_uri.authority()', helper)
        self.assertIn('helper_uri.authority()', helper)
        self.assertIn('login_uri.authority()', helper)
        self.assertIn('getLoginURIs(mGrid, login_uris)', helper)
        self.assertIn("uri.find('?') == std::string::npos", helper)
        self.assertIn('requires_https = helper_uri.scheme() == "https"', helper)
        self.assertIn('requires_https = requires_https || login_uri.scheme() == "https"', helper)
        self.assertIn('trusted_host && (!requires_https || advertised_uri.scheme() == "https")', helper)
        currency_ui = (NV/'llfloaterbuycurrency.cpp').read_text()
        for signature in ('void LLFloaterBuyCurrency::buyCurrency()','void LLFloaterBuyCurrency::buyCurrency(const std::string& name, S32 price)'):
            opener = method(currency_ui, signature)
            self.assertIn('isInOpenSim()', opener)
            self.assertIn('handleBuyCurrency(true', opener)
            self.assertLess(opener.index('isInOpenSim()'), opener.index('new LLFetchAvatarPaymentInfo'))
            self.assertIn('sPropertiesRequest = NULL;', opener)
        server = (Path('/home/marty/opensim/telep-npc')/
                  'OpenSim/Region/OptionalModules/World/MoneyModule/SampleMoneyModule.cs').read_text()
        self.assertIn('new SimpleStreamHandler("/currency.php", processPHP)', server)
        self.assertIn('m_rpcHandlers.Add("getCurrencyQuote", quote_func)', server)
        self.assertIn('m_rpcHandlers.Add("buyCurrency", buy_func)', server)
        buy = (NV/'llbuycurrencyhtml.cpp').read_text()
        self.assertIn('isInOpenSim()', buy)
        self.assertIn('isInSecondLife()', buy)

    def test_currency_symbol_follows_login_and_active_region(self):
        ui_string = (ROOT/'indra/llui/lluistring.cpp').read_text()
        self.assertIn('LLCurrencySymbol::grid(symbol, opensim)', ui_string)
        self.assertIn('LLCurrencySymbol::region(symbol, opensim)', ui_string)
        self.assertIn('LLCurrencySymbol::active(sGridCurrencySymbol, sRegionCurrencySymbol)', ui_string)
        update_result = method(ui_string, 'void LLUIString::updateResult() const')
        self.assertIn('LLCurrencySymbol::replaceCurrencyToken(mResult, getCurrencySymbol())', update_result)
        self.assertNotIn('replaceFormattedAmount', update_result)
        helper_fixture = f'''#include <cassert>
#include "{ROOT}/indra/llui/llcurrency_symbol.h"
int main() {{
    using namespace LLCurrencySymbol;
    assert(grid("G$", true) == "G$");
    assert(grid("", true) == "OS$");
    assert(grid(std::string("bad\\x01", 4), true) == "OS$");
    assert(grid(std::string(17, 'x'), true) == "OS$");
    assert(grid("[BAD]", true) == "OS$");
    assert(grid("", false) == "L$");
    assert(region("OS$", true) == "OS$");
    assert(region("G$", false).empty());
    std::string current = active(grid("", true), region("G$", true));
    assert(current == "G$");
    current = active(grid("", true), region("", true));
    assert(current == "OS$");
    current = active(grid("", false), region("G$", false));
    assert(current == "L$");

    std::string arbitrary = "Custom user text says L$ literally";
    replaceCurrencyToken(arbitrary, current);
    assert(arbitrary == "Custom user text says L$ literally");
    std::string label = "Buy [CURRENCY] now";
    replaceCurrencyToken(label, "OS$");
    assert(label == "Buy OS$ now");
    std::string amount = "L$ 25";
    replaceFormattedAmount(amount, "G$");
    assert(amount == "G$ 25");
}}'''
        compile_run(helper_fixture)

        startup = (NV/'llstartup.cpp').read_text()
        world_init = method(startup, 'if (STATE_WORLD_INIT == LLStartUp::getStartupState())')
        self.assertIn('login_response["currency"].asString()', world_init)
        self.assertIn('setGridCurrencySymbol(currency_symbol,', world_init)
        self.assertIn('isInOpenSim()', world_init)
        self.assertIn('setRegionCurrencySymbol(std::string(),', world_init)

        region_source = (NV/'llviewerregion.cpp').read_text()
        update = method(region_source, 'void LLViewerRegion::updateCurrencySymbol()')
        self.assertIn('isInOpenSim()', update)
        self.assertIn('extras["currency"]', update)
        self.assertIn('setRegionCurrencySymbol(symbol,', update)
        self.assertIn('gStatusBar->updateCurrencySymbols()', update)
        self.assertIn('LLFloaterBuyCurrency::updateCurrencySymbols()', update)
        feature_update = method(region_source, 'void LLViewerRegion::setSimulatorFeatures(')
        self.assertIn('postToMainCoro(work)', feature_update)
        self.assertIn('active_region->getRegionID() == feature_region_id', feature_update)
        self.assertIn('active_region->updateCurrencySymbol()', feature_update)
        agent_region = method((NV/'llagent.cpp').read_text(), 'void LLAgent::setRegion(')
        self.assertIn('regionp->updateCurrencySymbol()', agent_region)

        status_xui = ET.parse(NV/'skins/default/xui/en/panel_status_bar.xml').getroot()
        status_xui_pl = ET.parse(NV/'skins/default/xui/pl/panel_status_bar.xml').getroot()
        currency_xui = ET.parse(NV/'skins/default/xui/en/floater_buy_currency.xml').getroot()
        currency_xui_pl = ET.parse(NV/'skins/default/xui/pl/floater_buy_currency.xml').getroot()
        for root in (status_xui, status_xui_pl, currency_xui, currency_xui_pl):
            self.assertTrue(any('[CURRENCY]' in value for value in root.itertext()) or
                            any('[CURRENCY]' in value for node in root.iter()
                                for value in node.attrib.values()))
        self.assertIn('[CURRENCY]', status_xui.find(".//panel[@name='balance_bg']/text[@name='balance']").attrib['value'])
        self.assertIn('[CURRENCY]', currency_xui.attrib['title'])
        for name in ('target_price', 'balance_amount', 'required_amount', 'currency_label', 'total_amount'):
            control = currency_xui.find(f".//text[@name='{name}']")
            self.assertIsNotNone(control)
            self.assertIn('[CURRENCY]', ''.join(control.itertext()))

    def test_land_transactions_resolve_selected_grid_without_static_cache(self):
        network = (NV/'llviewernetwork.cpp').read_text()
        resolver = method(network, 'std::string LLGridManager::getLandTransactionURI(')
        self.assertIn('getHelperURI()', resolver)
        self.assertIn('gridURL(uri)', resolver)
        self.assertIn("uri.find('?')", resolver)
        self.assertIn('uri += "landtool.php"', resolver)

        floater = method((NV/'llfloaterbuyland.cpp').read_text(),
                         'void LLFloaterBuyLandUI::startTransaction(')
        self.assertIn('getLandTransactionURI()', floater)
        self.assertIn('transaction_uri.empty()', floater)
        self.assertIn('tellUserError(', floater)
        self.assertNotIn('static std::string transaction_uri', floater)

        server = (Path('/home/marty/opensim/telep-npc')/
                  'OpenSim/Region/OptionalModules/World/MoneyModule/SampleMoneyModule.cs').read_text()
        self.assertIn('new SimpleStreamHandler("/landtool.php", processPHP)', server)

        fixture = r'''
#include <cassert>
#include <string>
bool gridURL(const std::string& value, bool login = false) {
    (void)login;
    const auto scheme = value.find("://");
    if (scheme == std::string::npos ||
        (value.substr(0, scheme) != "http" && value.substr(0, scheme) != "https")) return false;
    const auto authority_start = scheme + 3;
    const auto authority_end = value.find('/', authority_start);
    const auto authority = value.substr(authority_start, authority_end - authority_start);
    return !authority.empty() && authority.find('@') == std::string::npos &&
        value.find(' ') == std::string::npos &&
        value.find('#') == std::string::npos;
}
struct LLGridManager {
    std::string helper;
    std::string getHelperURI() { return helper; }
    std::string getLandTransactionURI();
};
''' + resolver + r'''
int main() {
    LLGridManager grid;
    grid.helper = "https://one.example/helpers/";
    assert(grid.getLandTransactionURI() == "https://one.example/helpers/landtool.php");
    grid.helper = "http://two.example/economy";
    assert(grid.getLandTransactionURI() == "http://two.example/economy/landtool.php");
    grid.helper = "https://two.example/helpers/?token=x";
    assert(grid.getLandTransactionURI().empty());
    grid.helper = "https://user:secret@two.example/helpers/";
    assert(grid.getLandTransactionURI().empty());
}
'''
        compile_run(fixture)

    def test_grid_status_is_grid_scoped_and_tls_verified(self):
        source = (NV/'llfloatergridstatus.cpp').read_text()
        endpoint = method(source, 'std::string getGridStatusEndpoint(')
        validator = method(source, 'bool isUsableGridStatusURL(')
        self.assertIn('isInSecondLife()', endpoint)
        self.assertIn('DEFAULT_GRID_STATUS_URL', endpoint)
        self.assertIn('isInOpenSim()', endpoint)
        self.assertIn('features["OpenSimExtras"]', endpoint)
        self.assertIn('metadata_key', endpoint)
        self.assertIn('authority.find(\'@\')', validator)
        self.assertIn('std::any_of', validator)
        self.assertIn('p.trusted_content = LLGridManager::getInstance()->isInSecondLife()', source)
        self.assertNotIn('setSSLVerifyPeer(false)', source)
        self.assertIn('getGrid() != grid', source)
        self.assertIn('getGridStatusRSSFilename(grid)', source)
        filename = method(source, 'std::string getGridStatusRSSFilename(')
        self.assertIn('digest.update(grid)', filename)
        self.assertIn('"grid_status_"', filename)

        manager = (NV/'llfloatergridmanager.cpp').read_text()
        self.assertIn('{"GridStatus", "grid_status"}', manager)
        self.assertIn('{"GridStatusRSS", "grid_status_rss"}', manager)
        for locale in ('en', 'pl'):
            xml = ET.parse(NV/f'skins/default/xui/{locale}/floater_grid_manager.xml').getroot()
            controls = {node.attrib.get('name') for node in xml.iter()}
            self.assertTrue({'grid_status', 'grid_status_rss'} <= controls)

    def test_marketplace_uses_grid_endpoint(self):
        network = (NV/'llviewernetwork.cpp').read_text()
        market = method(network,'std::string LLGridManager::getMarketplaceURL(')
        self.assertIn('isInSecondLife()', market)
        self.assertIn('marketplace.i.let-us.cyou', market)
        market_floater = (NV/'llfloatermarketplace.cpp').read_text()
        self.assertIn('getMarketplaceURL()', market_floater)
        self.assertNotIn('gSavedSettings.getString("MarketplaceURL")', market_floater)
        menu = (NV/'llviewermenu.cpp').read_text()
        self.assertIn('getMarketplaceURL()', menu)

    def test_complete_bundled_grid_list(self):
        defaults = NV/'app_settings/tasia_default_grids.xml'
        root = ET.parse(defaults).getroot()[0]
        keys = [node.text for node in root if node.tag == 'key']
        self.assertGreaterEqual(len(keys), 30, 'Bundled grid list must contain the full set')
        for required in ('i.let-us.cyou:8002','not.let-us.cyou:8002','login.osgrid.org',
                         'login.misfitzgrid.com:8002','hypergrid.org:8002','localhost:9000',
                         'os.tasia.work.gd:22000'):
            self.assertIn(required, keys)
        self.assertNotIn('util.agni.lindenlab.com', keys)  # built-in Second Life grids stay built in
        # Every entry must carry an explicit login URI; none is inferred.
        for entry in root:
            if entry.tag != 'map':
                continue
            values = {}
            children = list(entry)
            index = 0
            while index + 1 < len(children):
                if children[index].tag == 'key':
                    values[children[index].text] = children[index+1]
                index += 2
            uris = [node.text for node in values.get('loginuri', []) if node.tag == 'string']
            self.assertTrue(uris, 'Every bundled grid needs an explicit login URI')

    def test_bundled_defaults_are_loaded_for_every_profile(self):
        network = (NV/'llviewernetwork.cpp').read_text()
        self.assertIn('importGridCatalog(xml, error, false)', network)
        self.assertIn('tasia_default_grids.xml', network)
        catalog = method(network,'void LLGridManager::requestGridCatalog(')
        self.assertIn('https://api.tasiaviewer.work/viewer/config/grid.xml', catalog)
        self.assertIn('sourceGridForUrl', (NV/'lltasia_user_config.cpp').read_text())

    def test_identity_catalog_scopes_by_source_grid(self):
        source = (NV/'lltasia_user_config.cpp').read_text()
        self.assertIn('sourceGridForUrl', source)
        self.assertIn('applyConfig(LlsdFromJson(json), source_grid)', source)
        self.assertIn('https://i.let-us.cyou/hg/config.json', (NV/'app_settings/settings.xml').read_text())

    def test_friendly_grid_names_resolve(self):
        network = (NV/'llviewernetwork.cpp').read_text()
        resolver = method(network,'std::string LLGridManager::resolveGridKey(')
        self.assertIn('MAINGRID', resolver)
        self.assertIn('BETAGRID', resolver)
        self.assertIn('secondlife.com', resolver)
        self.assertIn('getGrid(host)', resolver)
        self.assertIn('resolveGridKey', (NV/'lltasia_user_config.cpp').read_text())
        header = (NV/'llviewernetwork.h').read_text()
        self.assertIn('std::string resolveGridKey(', header)
        # getGrid() is non-const, so the resolver must not be declared const.
        self.assertNotIn('resolveGridKey(const std::string& grid) const', header)
        self.assertIn('LLSD::map_const_iterator', method(network,'std::string LLGridManager::resolveGridKey('))
        # A friendly name must never be stored as the lookup key directly.
        identity = (NV/'lltasia_user_config.cpp').read_text()
        self.assertIn('resolveEntryGrid(getCleanString(item, "grid", 255))', identity)

    def test_polish_status_bar_is_complete(self):
        # The Polish status bar used to be a reduced stub: no volume button, no
        # preset icons, no menu search and no QUIC indicator.
        def controls(path):
            names = set()
            for node in ET.parse(path).getroot().iter():
                if node.get('name'):
                    names.add(node.get('name'))
            return names
        english = controls(NV/'skins/default/xui/en/panel_status_bar.xml')
        polish = controls(NV/'skins/default/xui/pl/panel_status_bar.xml')
        self.assertTrue(english <= polish, 'Polish status bar is missing controls')
        for required in ('quic_status', 'volume_btn', 'presets_icon_camera', 'media_toggle_btn'):
            self.assertIn(required, polish)

    def test_quic_settings_are_reachable_in_preferences(self):
        # The panel file alone is not enough: it must be registered as a tab,
        # otherwise none of the QUIC/TLS settings can be seen or changed.
        for language, label in (('en', 'Tasia - Network'), ('pl', 'Tasia - Sieć')):
            tabs = ET.parse(NV/f'skins/default/xui/{language}/floater_preferences.xml').getroot()
            registered = [panel.get('filename') for panel in tabs.iter('panel')
                         if panel.get('filename')]
            self.assertIn('panel_preferences_tasia_network.xml', registered,
                          f'{language}: Tasia preferences tab is not registered')
            self.assertEqual(registered.count('panel_preferences_tasia_network.xml'), 1,
                             f'{language}: the Tasia tab is registered more than once')
            labels = [panel.get('label') for panel in tabs.iter('panel')]
            self.assertIn(label, labels)
            # It must be the first tab, so it can never sit below the fold.
            self.assertEqual(registered[0], 'panel_preferences_tasia_network.xml',
                             f'{language}: the network tab is not first')
            panel_path = NV/f'skins/default/xui/{language}/panel_preferences_tasia_network.xml'
            panel = panel_path.read_text()
            root = ET.parse(panel_path).getroot()
            for child in root:
                self.assertLessEqual(int(child.get('top', 0)) + int(child.get('height', 0)), 420)
            self.assertNotIn('Pref.TasiaAnim', panel)
            settings_root = ET.parse(NV/'app_settings/settings.xml').getroot().find('map')
            defined = {node.text for node in settings_root if node.tag == 'key'}
            for filename in ('panel_preferences_tasia_network.xml', 'panel_preferences_tasia_media.xml'):
                for node in ET.parse(NV/f'skins/default/xui/{language}/{filename}').getroot().iter():
                    if node.get('control_name'):
                        self.assertIn(node.get('control_name'), defined)
            for control in ('TasiaQuicEnabled', 'TasiaQuicVerifyCert', 'TasiaGridCatalogEnabled',
                            'TasiaRemoteUserConfigEnabled'):
                self.assertIn(control, panel)
        # QUIC must be on by default for fresh profiles; the fallback covers failure.
        settings = (NV/'app_settings/settings.xml').read_text()
        block = settings.split('<key>TasiaQuicEnabled</key>')[1].split('</map>')[0]
        self.assertIn('<key>Value</key>\n    <integer>1</integer>', block)

    def test_status_bar_sizes_itself_to_content(self):
        source = (NV/'llstatusbar.cpp').read_text()
        self.assertIn('reshapeToFitContent', source)
        # LLView has no getNextSibling(); the child list must be used.
        self.assertNotIn('getNextSibling()', source)
        self.assertIn('getChildList()', source)
        header = (NV/'llstatusbar.h').read_text()
        self.assertIn('void reshapeToFitContent();', header)
        # The extra stop button must not remain in the crowded bar.
        for language in ('en','pl'):
            panel = (NV/f'skins/default/xui/{language}/panel_status_bar.xml').read_text()
            self.assertNotIn('stop_stream_btn', panel)
        media = (NV/'llviewermedia.cpp').read_text()
        self.assertIn('stopFromMediaControls()', media)


if __name__ == '__main__':
    unittest.main(verbosity=2)

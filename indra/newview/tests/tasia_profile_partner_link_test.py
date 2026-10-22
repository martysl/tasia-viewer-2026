#!/usr/bin/env python3
"""Profile partner names stay readable while links use the active grid SLURL."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
NV = ROOT / 'indra/newview'


class ProfilePartnerLink(unittest.TestCase):
    def test_partner_display_uses_avatar_name_cache(self):
        source = (NV / 'llpanelprofile.cpp').read_text()
        body = source.split('void LLPanelProfileSecondLife::fillPartnerData(', 1)[1]
        body = body.split('\nvoid LLPanelProfileSecondLife::onPartnerAvatarNameCache(', 1)[0]
        self.assertIn('LLAvatarNameCache::get(avatar_data->partner_id', body)
        self.assertIn('setPartnerLinkText(getString("partner_name_loading"))', body)
        self.assertNotIn('getSLURLString()', body)

    def test_name_callback_rejects_stale_partner_and_links_display_name(self):
        source = (NV / 'llpanelprofile.cpp').read_text()
        body = source.split('void LLPanelProfileSecondLife::onPartnerAvatarNameCache(', 1)[1]
        body = body.split('\nvoid LLPanelProfileSecondLife::setPartnerLinkText(', 1)[0]
        self.assertIn('agent_id != mPartnerAvatarId', body)
        self.assertIn('av_name.getDisplayName()', body)
        self.assertIn('setPartnerLinkText(partner_name)', body)

    def test_visible_name_is_separate_from_grid_aware_click_target(self):
        source = (NV / 'llpanelprofile.cpp').read_text()
        body = source.split('void LLPanelProfileSecondLife::setPartnerLinkText(', 1)[1]
        body = body.split('\nvoid LLPanelProfileSecondLife::fillAccountStatus(', 1)[0]
        self.assertIn('args["[LINK]"] = partner_name', body)
        self.assertIn('link_style.is_link = true', body)
        self.assertIn('link_style.link_href = LLSLURL("agent", mPartnerAvatarId, "inspect").getSLURLString()', body)

    def test_english_and_polish_have_partner_labels(self):
        for language in ('en', 'pl'):
            xui = (NV / f'skins/default/xui/{language}/panel_profile_secondlife.xml').read_text()
            self.assertIn('name="partner_text"', xui)
            self.assertIn('name="partner_name_loading"', xui)


if __name__ == '__main__':
    unittest.main(verbosity=2)

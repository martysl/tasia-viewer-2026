#!/usr/bin/env python3
"""Native wiring/privacy contracts and compiled transport policy; not GUI tests."""
from pathlib import Path
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
V = ROOT / 'indra/newview'


class Integration(unittest.TestCase):
    def test_shared_chat_editor_and_history_wiring(self):
        registry = (V / 'llviewerfloaterreg.cpp').read_text()
        for name in ('nearby_chat', 'impanel'):
            self.assertIn(f'add("{name}", "floater_im_session.xml"', registry)
        source = (V / 'llfloaterimsessiontab.cpp').read_text()
        self.assertIn('editor_handle.get()', source)
        self.assertIn('editor->insertText(', source)
        self.assertIn('TasiaMediaLinkTooLong', source)
        self.assertNotIn('send_chat_from_viewer(', source[source.index('const LLHandle<LLView> editor_handle'):source.index('mChatLayoutPanel =')])
        root = ET.parse(V / 'skins/default/xui/en/floater_im_session.xml')
        for provider in ('giphy', 'klipy'):
            self.assertIsNotNone(root.find(f'.//button[@name="{provider}_picker_btn"]'))
        history = (V / 'llchathistory.cpp').read_text()
        self.assertIn('!has_provider_preview', history)
        self.assertIn('new TasiaGiphyPreviewPanel(preview)', history)
        preview = (V / 'lltasia_chat_preview.cpp').read_text()
        self.assertIn('provider_name == preview.provider', preview)
        self.assertIn('tasia_media/preview.html', preview)
        self.assertIn('".mp4"', preview)
        self.assertIn('".klipy.com"', preview)

    def test_keys_are_session_only_and_not_embedded_in_generated_sources(self):
        root = ET.parse(V / 'app_settings/settings.xml').getroot().find('map')
        children = list(root)
        for name in ('TasiaGiphyAPIKey', 'TasiaKlipyAPIKey'):
            index = next(i for i, node in enumerate(children) if node.tag == 'key' and node.text == name)
            fields = list(children[index + 1])
            values = {fields[i].text: fields[i+1].text for i in range(0, len(fields), 2)}
            self.assertEqual(values['Persist'], '0')
            self.assertFalse(values['Value'])
        for provider in ('giphy', 'klipy'):
            self.assertIn('kLength = 0u;', (V / f'lltasia_{provider}_key.generated.cpp').read_text())
            configured = (V / f'lltasia_{provider}_key.cpp').read_text().split('std::string getConfiguredAPIKey()')[1]
            self.assertNotIn('return getEmbeddedAPIKey()', configured)
        native = (V / 'llfloatertasiamediapicker.cpp').read_text()
        self.assertIn('setTrustedContent(false)', native)
        self.assertIn('setAllowFileDownload(false)', native)
        self.assertIn('gAgent.getSessionID() != mSession', native)
        self.assertIn('"#select=" + mNonce.asString()', native)
        self.assertIn('mBrowser->unloadMediaSource()', native)
        self.assertNotIn('mKeyEditor->setText("")', native)
        self.assertIn('mKeyEditor->clear()', native)
        self.assertIn('void    clear() override;', (ROOT / 'indra/llui/lllineeditor.h').read_text())
        self.assertIn('[this](LLUICtrl*, const LLSD&)', native)
        self.assertIn('[editor_handle, provider](LLUICtrl*, const LLSD&)', (V / 'llfloaterimsessiontab.cpp').read_text())
        for name in ('llplugininstance.cpp', 'llpluginmessagepipe.cpp', 'llpluginprocesschild.cpp', 'llpluginprocessparent.cpp'):
            source = (ROOT / 'indra/llplugin' / name).read_text()
            for raw in ('<< buffer <<', '<< message <<', '<< message_string <<'):
                self.assertNotIn(raw, source)

    def test_truthful_transport_status_production_header(self):
        cpp = f'''#include "{V / 'lltasia_transport_status.h'}"
#include <cassert>
int main() {{
 using S = TasiaTransportStatus;
 assert(tasiaTransportStatus(false,false,true,true)==S::Disconnected);
 assert(tasiaTransportStatus(true,true,true,true)==S::SecondLife);
 assert(tasiaTransportStatus(true,false,false,true)==S::Disabled);
 assert(tasiaTransportStatus(true,false,true,false)==S::Unavailable);
 assert(tasiaTransportStatus(true,false,true,true)==S::NotRouted);
}}
'''
        with tempfile.TemporaryDirectory(prefix='transport-test-', dir='/tmp/opencode') as tmp:
            source = Path(tmp) / 'test.cpp'
            source.write_text(cpp)
            exe = Path(tmp) / 'test'
            subprocess.run(['c++','-std=c++17','-Wall','-Werror',str(source),'-o',str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
        panel = ET.parse(V / 'skins/default/xui/en/panel_status_bar.xml')
        self.assertIsNotNone(panel.find('.//icon[@name="quic_status"]'))
        self.assertIn('setVisible(visible)', (V / 'llstatusbar.cpp').read_text())


if __name__ == '__main__':
    unittest.main(verbosity=2)

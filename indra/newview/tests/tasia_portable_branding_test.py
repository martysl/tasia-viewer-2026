#!/usr/bin/env python3
"""Offline portable routing/branding checks. Not a Windows GUI/runtime test."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
VIEWER = ROOT / 'indra/newview'
DIR = ROOT / 'indra/llfilesystem'


def method(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


class Portable(unittest.TestCase):
    def test_actual_directory_policy_methods(self):
        """Compile actual LLDir methods; only filesystem/logging surroundings stubbed."""
        source = (DIR / 'lldir.cpp').read_text()
        methods = '\n'.join(method(source, signature) for signature in (
            'const std::string  LLDir::getCacheDir(',
            'std::string LLDir::buildSLOSCacheDir(',
            'void LLDir::setChatLogsDir(',
            'bool LLDir::setCacheDir('))
        fixture = r'''
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#define LL_WARNS() std::cerr
#define LL_ENDL std::endl
using LLFILE = FILE;
struct LLFile {
 static int mkdir(const std::string& p) { std::filesystem::create_directories(p); return 0; }
 static FILE* fopen(const std::string& p, const char* m) { return std::fopen(p.c_str(), m); }
 static int remove(const std::string& p) { return std::remove(p.c_str()); }
};
struct LLDir {
 std::string mPortableDataDir, mCacheDir, mDefaultCacheDir, mChatLogsDir;
 std::string osCache, osApp;
 bool isPortable() const { return !mPortableDataDir.empty(); }
 std::string add(const std::string& p, const std::string& c) const { return p+"/"+c; }
 const std::string& getOSCacheDir() const { return osCache; }
 const std::string& getOSUserAppDir() const { return osApp; }
 const std::string getCacheDir(bool get_default=false) const;
 std::string buildSLOSCacheDir() const;
 void setChatLogsDir(const std::string&);
 bool setCacheDir(const std::string&);
};
''' + methods + r'''
int main() {
 LLDir normal;
 normal.osCache="AppData/Local";
 assert(normal.buildSLOSCacheDir()=="AppData/Local/Kirstens S24 DX");
 normal.osCache.clear(); normal.osApp="AppData/Roaming/Tasia";
 assert(normal.buildSLOSCacheDir()=="AppData/Roaming/Tasia/cache");
 normal.setChatLogsDir("chosen-transcripts");
 assert(normal.mChatLogsDir=="chosen-transcripts");
 for(const auto* base : {"copy-one/TasiaViewerData", "copy-two/TasiaViewerData", "older/portable-data"}) {
   LLDir portable;
   portable.mPortableDataDir=base;
   portable.mDefaultCacheDir=portable.buildSLOSCacheDir();
   assert(portable.getCacheDir()==std::string(base)+"/cache");
   assert(!portable.setCacheDir("other-installation/cache"));
   assert(!std::filesystem::exists("other-installation"));
   assert(portable.getCacheDir()==std::string(base)+"/cache");
   assert(portable.setCacheDir(""));
   assert(portable.setCacheDir(std::string(base)+"/cache"));
   portable.setChatLogsDir("other-installation/transcripts");
   assert(portable.mChatLogsDir==std::string(base)+"/chat_logs");
 }
}
'''
        with tempfile.TemporaryDirectory(prefix='portable-test-', dir='/tmp/opencode') as tmp:
            cpp = Path(tmp) / 'test.cpp'
            cpp.write_text(fixture)
            exe = Path(tmp) / 'test'
            subprocess.run(['c++', '-std=c++17', '-DDX_RENDER', '-Wall', '-Werror', str(cpp), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], cwd=tmp, check=True)

    def test_initialization_before_first_cache_creation(self):
        source = (DIR / 'lldir_win32.cpp').read_text()
        self.assertLess(source.index('"TasiaViewer.portable"'), source.index('mDefaultCacheDir = buildSLOSCacheDir()'))
        self.assertIn('add(mExecutableDir, "TasiaViewerData")', source)
        self.assertIn('has_legacy_portable ? legacy_portable', source)
        self.assertIn('Both portable-data and TasiaViewerData exist', source)
        self.assertIn('CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE', source)
        self.assertIn('ExitProcess(ERROR_ACCESS_DENIED)', source)
        for env in ('TEMP', 'TMP'):
            self.assertIn(f'_wputenv_s(L"{env}", temp.c_str())', source)

    def test_no_partial_expansion_override(self):
        source = (DIR / 'lldir.cpp').read_text()
        self.assertNotIn('std::string portable_filename', source)
        self.assertIn('add(prefix, subdir1, subdir2)', source)

    def test_no_automatic_old_profile_purge_or_move(self):
        source = (VIEWER / 'llappviewer.cpp').read_text()
        migrate = method(source, 'void LLAppViewer::migrateCacheDirectory()')
        self.assertLess(migrate.index('isPortable()'), migrate.index('rename('))
        cache = method(source, 'bool LLAppViewer::initCache()')
        self.assertLess(cache.index('"NewCacheLocation"'), cache.index('purgeCache();'))
        pref = (VIEWER / 'llfloaterpreference.cpp').read_text()
        for name in ('onClickLogPath', 'changeLogPath', 'moveTranscriptsAndLog', 'onClickSetCache', 'changeCachePath'):
            self.assertIn('isPortable()', method(pref, f'LLFloaterPreference::{name}('))
        startup = (VIEWER / 'llstartup.cpp').read_text()
        self.assertIn('gDirUtilp->isPortable() || gSavedPerAccountSettings.getString("InstantMessageLogPath").empty()', startup)

    def test_account_and_browser_paths_follow_directory_api(self):
        self.assertIn('getExpandedFilename(LL_PATH_USER_SETTINGS, "login_accounts.sqlite3")', (VIEWER / 'llpanellogin.cpp').read_text())
        media = (VIEWER / 'llviewermedia.cpp').read_text()
        self.assertIn('user_data_path_cache = gDirUtilp->getCacheDir(false)', media)
        self.assertIn('getExpandedFilename(LL_PATH_LOGS, "cef.log")', media)
        store = (VIEWER / 'lltasia_login_store.cpp').read_text()
        self.assertIn('CryptProtectData', store)
        self.assertIn('CryptUnprotectData', store)

    def test_new_packages_enable_portable(self):
        source = (ROOT / 'scripts/Package-TasiaViewer.ps1').read_text()
        self.assertIn("Join-Path $Destination 'TasiaViewer.portable'", source)
        self.assertIn("Join-Path $repo 'docs/PORTABLE.md'", source)


class Branding(unittest.TestCase):
    def test_source_and_generated_hashes_and_dimensions(self):
        manifest = json.loads((VIEWER / 'res/tasia_brand_assets.json').read_text())
        self.assertEqual(hashlib.sha256((VIEWER / manifest['source']).read_bytes()).hexdigest(), manifest['source_sha256'])
        self.assertEqual(manifest['source_sha256'], '6aec2c8cf7b04f7b38defcfbfcb3e659cef0697d202b6f534cf06d7eb66d51d5')
        for path, data in manifest['outputs'].items():
            asset = VIEWER / path
            self.assertEqual(hashlib.sha256(asset.read_bytes()).hexdigest(), data['sha256'], path)
            with Image.open(asset) as image:
                if asset.suffix == '.ico':
                    self.assertEqual(image.ico.sizes(), {tuple(s) for s in data['sizes']})
                else:
                    self.assertEqual(image.size, tuple(data['size']))
                    self.assertIsNotNone(image.getchannel('A').getbbox())

    def test_routing_and_login_aspect_ratio(self):
        textures = ET.parse(VIEWER / 'skins/default/textures/textures.xml').getroot()
        names = {t.get('name'): t.get('file_name') for t in textures}
        for name in ('SL_Logo', 'System_Notification'):
            self.assertEqual(names[name], 'icons/tasia_app.png')
        for xml, name in [('panel_login.xml', 'tasia_logo_small'), ('panel_login_first.xml', 'sl_logo'), ('floater_about.xml', 'tasia_about_logo')]:
            tree = ET.parse(VIEWER / 'skins/default/xui/en' / xml)
            icon = tree.find(f'.//icon[@name="{name}"]')
            self.assertAlmostEqual(float(icon.get('width')) / float(icon.get('height')), 16 / 9)
            self.assertTrue(icon.get('image_name').startswith('login_tasia_logo'))
        self.assertIn('IDI_LCD_LL_ICON         ICON                    "tasia_icon.ico"', (VIEWER / 'res/viewerRes.rc').read_text())

    def test_legacy_and_alternate_skin_logo_files_match_approved_art(self):
        for skin in ('default', 'Flat', 'Lysi', 'linden', 'midnight'):
            for filename, generated in (
                ('windows/login_sl_logo.png', 'windows/login_tasia_logo.png'),
                ('windows/login_sl_logo_horizontal.png', 'windows/login_tasia_logo_horizontal.png'),
                ('windows/login_sl_logo_small.png', 'windows/login_tasia_logo_small.png'),
                ('windows/startup_logo.png', 'windows/startup_logo.png'),
                ('icons/SL_Logo.png', 'icons/tasia_app.png'),
            ):
                with Image.open(VIEWER / f'skins/{skin}/textures' / filename) as legacy, Image.open(VIEWER / 'skins/default/textures' / generated) as approved:
                    self.assertEqual(legacy.size, approved.size, (skin, filename))
                    self.assertEqual(legacy.convert('RGBA').tobytes(), approved.convert('RGBA').tobytes(), (skin, filename))

    def test_packager_checks_every_manifest_asset_at_source_and_destination(self):
        source = (ROOT / 'scripts/Package-TasiaViewer.ps1').read_text()
        self.assertIn('foreach ($asset in $brandManifest.outputs.PSObject.Properties)', source)
        self.assertIn('foreach ($path in @($sourceAsset, $targetAsset))', source)
        self.assertIn('$asset.Value.sha256', source)
        self.assertIn("'tasia-brand-assets.json'", source)


if __name__ == '__main__':
    unittest.main(verbosity=2)

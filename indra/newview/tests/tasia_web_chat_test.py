#!/usr/bin/env python3
"""Check Chat XUI wiring and compile the actual handler with synthetic viewer APIs.

Does not compile the Windows viewer or test CEF, HTTP, cookies or authentication.
LLURI is a recording fake: encoding itself remains the existing viewer's job.
"""
import subprocess
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = (root / 'llviewermenu.cpp').read_text()
start = source.index('void handle_tasia_chat(')
end = source.index('\n}', start) + 2
handler = source[start:end]
assert 'global_commit.add("Tasia.Chat", boost::bind(&handle_tasia_chat, _2))' in source
assert '#include "llagentdata.h"' in source
assert '#include "llfloaterwebcontent.h"' in source
assert '#include "lluri.h"' in source
for forbidden in ('getSessionID', 'getSecureSessionID', 'LL_INFOS', 'LL_WARNS', 'loadURLExternal'):
    assert forbidden not in handler, forbidden

commands = ET.parse(root / 'app_settings/commands.xml').getroot()
matches = commands.findall("command[@name='tasia_chat']")
assert len(matches) == 1
command = matches[0]
assert command.get('execute_function') == 'Tasia.Chat'
assert command.get('available_in_toybox') == 'true'
toolbar = ET.parse(root / 'app_settings/toolbars.xml').getroot()
assert len(toolbar.findall(".//command[@name='tasia_chat']")) == 1
for locale in ('en', 'pl'):
    directory = root / 'skins/default/xui' / locale
    strings = ET.parse(directory / 'strings.xml').getroot()
    for attr in ('label_ref', 'tooltip_ref'):
        entries = strings.findall(f"string[@name='{command.get(attr)}']")
        assert len(entries) == 1 and entries[0].text.strip()
    for filename, menu in (('menu_viewer.xml', 'Communicate'), ('menu_login.xml', 'File')):
        tree = ET.parse(directory / filename).getroot()
        entries = tree.findall(f"menu[@name='{menu}']/menu_item_call[@name='TasiaChat']")
        assert len(entries) == 1
        if locale == 'en':
            assert entries[0].find('menu_item_call.on_click').get('function') == 'Tasia.Chat'
textures = ET.parse(root / 'skins/default/textures/textures.xml').getroot()
icons = textures.findall(f"texture[@name='{command.get('icon')}']")
assert len(icons) == 1
icon = root / 'skins/default/textures' / icons[0].get('file_name')
assert icon.read_bytes().startswith(b'\x89PNG\r\n\x1a\n')

fixture = r'''
#include <cassert>
#include <map>
#include <string>
struct LLSD {
    std::map<std::string, std::string> values;
    std::string& operator[](const std::string& key) { return values[key]; }
};
enum { STATE_LOGIN_WAIT, STATE_STARTED };
struct LLStartUp {
    static int state;
    static int getStartupState() { return state; }
};
int LLStartUp::state = STATE_LOGIN_WAIT;
struct ID {
    std::string value;
    bool notNull() const { return !value.empty(); }
    std::string asString() const { return value; }
};
struct Agent { ID id; const ID& getID() const { return id; } } gAgent;
std::string gAgentUsername;
struct LLAvatarName {
    std::string getDisplayName() const { return u8"Zażółć & + ?"; }
};
struct LLAvatarNameCache {
    static bool cached;
    static bool get(const ID&, LLAvatarName*) { return cached; }
};
bool LLAvatarNameCache::cached = false;
struct LLGridManager {
    std::string grid = "synthetic-grid";
    static LLGridManager* getInstance() { static LLGridManager instance; return &instance; }
    std::string getGridId() const { return grid; }
};
struct LLURI {
    static LLSD query;
    static int calls;
    static std::string mapToQueryString(const LLSD& q) {
        query = q; ++calls; return "?encoded-by-viewer";
    }
};
LLSD LLURI::query;
int LLURI::calls = 0;
struct LLFloaterWebContent {
    struct Params {
        std::string url, target;
        bool show_chrome = true, allow_address_entry = true;
        bool trusted_content = true, show_page_title = true;
    };
};
struct LLFloater {
    std::string title;
    void setTitle(const std::string& s) { title = s; }
};
struct LLFloaterReg {
    static LLFloater floater;
    static LLFloaterWebContent::Params last;
    static bool fail;
    static LLFloater* showInstance(const std::string& name, const LLFloaterWebContent::Params& p) {
        assert(name == "web_content"); last = p; return fail ? nullptr : &floater;
    }
};
LLFloater LLFloaterReg::floater;
LLFloaterWebContent::Params LLFloaterReg::last;
bool LLFloaterReg::fail = false;
struct LLTrans {
    static std::string getString(const std::string& key) {
        assert(key == "Command_TasiaChat_Label"); return "Tasia Chat";
    }
};
'''
main = r'''
int main() {
    // Stale/non-null identity before login must not be shared with the website.
    gAgent.id.value = "synthetic-avatar";
    gAgentUsername = "Synthetic User";
    handle_tasia_chat(LLSD());
    assert(LLFloaterReg::last.url == "https://chat.tasiaviewer.work/");
    assert(LLURI::calls == 0);
    assert(LLFloaterReg::last.target == "tasia_chat");
    assert(!LLFloaterReg::last.show_chrome);
    assert(!LLFloaterReg::last.allow_address_entry);
    assert(!LLFloaterReg::last.trusted_content);
    assert(!LLFloaterReg::last.show_page_title);
    assert(LLFloaterReg::floater.title == "Tasia Chat");

    LLStartUp::state = STATE_STARTED;
    handle_tasia_chat(LLSD());
    assert(LLURI::calls == 1);
    assert(LLFloaterReg::last.url == "https://chat.tasiaviewer.work/?encoded-by-viewer");
    assert(LLURI::query.values.size() == 5);
    assert(LLURI::query["uuid"] == "synthetic-avatar");
    assert(LLURI::query["username"] == "Synthetic User");
    assert(LLURI::query["display"] == "Synthetic User");
    assert(LLURI::query["hud"] == "1");
    assert(LLURI::query["grid"] == "synthetic-grid");

    LLAvatarNameCache::cached = true;
    gAgentUsername = u8"Mąka & + ?";
    LLGridManager::getInstance()->grid = "another-grid";
    handle_tasia_chat(LLSD());
    assert(LLURI::calls == 2);
    assert(LLURI::query["display"] == u8"Zażółć & + ?");
    assert(LLURI::query["username"] == gAgentUsername);
    assert(LLURI::query["grid"] == "another-grid");

    gAgent.id.value.clear();
    handle_tasia_chat(LLSD());
    assert(LLURI::calls == 2);
    assert(LLFloaterReg::last.url == "https://chat.tasiaviewer.work/");
    LLFloaterReg::fail = true;
    handle_tasia_chat(LLSD()); // Failed floater creation must not dereference null.
}
'''
with tempfile.TemporaryDirectory(prefix='tasia-web-chat-', dir='/tmp/opencode') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    cpp.write_text(fixture + handler + main)
    exe = Path(tmp) / 'test'
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: Chat XML/icon wiring and compiled handler fixture; Windows/CEF runtime not tested.')

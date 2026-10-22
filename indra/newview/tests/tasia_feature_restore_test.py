#!/usr/bin/env python3
"""Offline integration checks; no viewer build, credentials, network or uploads.

Compiles the real MP3 unit and native capture helper against fake APIs, and
the real text policy.
The optional FFmpeg smoke test uses the local Linux binary, NOT the Windows bundle.
"""
import re
import shutil
import subprocess
import tempfile
import wave
import xml.etree.ElementTree as ET
from pathlib import Path

root = Path(__file__).resolve().parents[1]
repo = root.parents[1]
parse = lambda p: ET.parse(root / p).getroot()
commands = parse('app_settings/commands.xml')
for name, callback in [('tasia_chat', 'Tasia.Chat'), ('tasia_feed', 'Tasia.Feed')]:
    command = commands.findall(f"command[@name='{name}']")
    assert len(command) == 1 and command[0].get('execute_function') == callback
    for lang in ('en', 'pl'):
        strings = parse(f'skins/default/xui/{lang}/strings.xml')
        for attr in ('label_ref', 'tooltip_ref'):
            assert len(strings.findall(f"string[@name='{command[0].get(attr)}']")) == 1
for filename in ('menu_viewer.xml', 'menu_viewer_icons.xml'):
    menu = parse('skins/default/xui/en/' + filename)
    for callback in ('Tasia.Chat','Tasia.Feed','File.UploadClipboard','File.UploadMP3BatchSound'):
        assert len(menu.findall(f".//menu_item_call.on_click[@function='{callback}']")) == 1
login = parse('skins/default/xui/en/panel_login.xml')
browser = login.findall(".//web_browser[@name='login_html']")
assert len(browser) == 1 and browser[0].get('trusted_content') == 'false'
assert 'LLURI login_page("https://startup.tasiaviewer.work/")' in (root/'llpanellogin.cpp').read_text()
feed = (root/'llviewermenu.cpp').read_text().split('void handle_tasia_feed(',1)[1].split('\n}',1)[0]
assert 'https://api.tasiaviewer.work/feed/' in feed and 'trusted_content = false' in feed
assert 'params.target = "tasia_feed"' in feed

settings = parse('app_settings/settings.xml').find('map')
children = list(settings)
def setting(name):
    index = next(i for i,e in enumerate(children) if e.tag == 'key' and e.text == name)
    content = list(children[index+1])
    return {content[i].text: content[i+1].text for i in range(0,len(content),2)}
assert setting('TasiaAITranslateAPIKey')['Persist'] == '0'
assert not setting('TasiaAITranslateAPIKey').get('Value')
assert not setting('TasiaAITranslateEndpoint').get('Value')
ai = (root/'lltasia_ai_translate.cpp').read_text()
assert 'setSSLVerifyPeer(true)' in ai and 'setFollowRedirects(false)' in ai
assert 'in_flight >= 4' in ai and 'gAgent.getSessionID() != session' in ai
assert 'body["store"] = false' in ai and 'body["stream"] = false' in ai
assert not re.search(r'LL_(?:INFOS|WARNS|DEBUGS)', ai)
clipboard = (root/'llfloateruploadclipboard.cpp').read_text()
assert not any(x in clipboard for x in ('std::system','powershell.exe','CreateProcess'))
assert 'MASK_CONTROL' in clipboard and 'OpenClipboard' in clipboard
preview = (root/'llfloaterimagepreview.cpp').read_text()
assert 'args["temporary_clipboard_image"]' in preview
assert preview.index('loadImage(mFilenameAndPath)') < preview.index('tasia_release_clipboard_image(mFilenameAndPath)')
assert 'captured_files.find(filename)' in clipboard
registry = (root/'llviewerfloaterreg.cpp').read_text()
for name in ('upload_clipboard','mp3_batch_progress'):
    assert len(re.findall(r'LLFloaterReg::add\("'+name+r'"',registry)) == 1
    assert registry.count('"'+name+'",') >= 3 # registry and both web SLapp blacklists
cmake = (root/'CMakeLists.txt').read_text()
for source in ('llmp3batchupload.cpp','llfloateruploadclipboard.cpp','lltasia_ai_translate.cpp'):
    assert cmake.count('\n    '+source+'\n') == 1
assert 'OUTPUT_NAME "TasiaViewer" PDB_NAME "TasiaViewer"' in cmake
assert 'TASIA_UI_COPY_INPUTS' in cmake
assert 'Kirstenlee Cinquetti' in (root/'res/viewerRes.rc').read_text() # attribution retained
assert (root/'res/tasia_icon.ico').read_bytes()[:4] == b'\x00\x00\x01\x00'
for locale in ('en','pl'):
    about = parse(f'skins/default/xui/{locale}/floater_about.xml')
    credits = about.find(".//text_editor[@name='credits']")
    assert credits is not None and credits.get('parse_urls') == 'true'
    text = ''.join(credits.itertext())
    assert 'Kirstenlee Cinquetti' in text and 'DirectX 11' in text
    assert 'Advanced Multiverse Viewer by Tasia and Marty' in text
    assert 'https://github.com/Kirstenlee/Kirstensviewer' in text
    assert 'https://sourceforge.net/projects/kirstensviewers/' in text
    assert '4015' not in text and 'Alpha 1.2' not in text
assert 'Kirstens Viewer (C) 2008-2026, Kirstenlee Cinquetti.' in ''.join(
    parse('skins/default/xui/en/floater_about.xml').find(".//text_editor[@name='licenses_editor']").itertext())
tagline='Advanced Multiverse Viewer by Tasia and Marty'
win32=(repo/'indra/llwindow/llwindowwin32.cpp').read_text()
shutdown=(repo/'indra/llwindow/AresWin32.cpp').read_text()
resources=(root/'res/viewerRes.rc').read_text()
assert f'SetWindowText(mWindow, TEXT("{tagline}"))' in win32
assert 'TEXT("Kirstens Viewer")' not in win32
assert f'"{tagline}",668,60,36,240,26' in resources
assert 'Advanced Second Life Viewer' not in resources
assert f'SetConsoleTitleW(L"{tagline} - Shutdown in Progress")' in shutdown
assert f'UpdateAresDialog("Thank you for using {tagline}!")' in shutdown
for text in re.findall(r'UpdateAresDialog\("(\| (?:Advanced Multiverse Viewer|Shutdown sequence)[^"\n]+)"\)',shutdown):
    assert len(text)==69, text
progress=parse('skins/default/xui/en/panel_progress.xml')
assert progress.find(".//text[@name='viewer_tagline']").text==tagline
notifications = parse('skins/default/xui/en/notifications.xml')
mp3 = (root/'llmp3batchupload.cpp').read_text()
assert 'LL_DIR_DELIM_STR' not in mp3
directory_api=(repo/'indra/llfilesystem/lldir.h').read_text()
assert 'void append(std::string& destpath, const std::string& name) const' in directory_api
assert 'std::string add(const std::string& path, const NAMES& ... names) const' in directory_api
assert 'LLFilePicker::ELoadFilter load_filter, LLFilePicker::ESaveFilter save_filter' in (root/'llviewermenufile.h').read_text()
for name in set(re.findall(r'"(MP3Batch\w+|ConfirmMP3BatchUpload)"',mp3)):
    if name in ('MP3BatchConverting','MP3BatchUploading'): continue
    assert len(notifications.findall(f"notification[@name='{name}']")) == 1, name
assert 'status.mData == 0' in mp3 and 'mOwnDirectory' in mp3

policy_main = r'''
#include "lltasia_translation_policy.h"
#include <cassert>
int main() {
 using TasiaTranslation::isOrdinaryText;
 for(const char* s : {"hello there", u8"Zażółć gęślą jaźń", u8"Привет", u8"こんにちは", u8"안녕하세요"}) assert(isOrdinaryText(s));
 for(const char* s : {"", "   ", "/dance", "/me waves", "https://example.invalid/", ":-)", u8"😂🎉", "12345", "hello\nworld", u8"╔══╗"}) assert(!isOrdinaryText(s));
 assert(!isOrdinaryText(std::string(8193,'a')));
 assert(!isOrdinaryText(std::string("\xc0\xaf",2)));
}
'''
mp3_main = r'''
void tick() {
 auto tasks=std::move(repeating); repeating.clear();
 for(auto& f:tasks) if(!f()) repeating.push_back(f);
 auto singles=std::move(once); once.clear(); for(auto& f:singles)f();
}
void select() {
 start_mp3_batch_sound_upload();
 LLFilePickerReplyThread::picked({"Synthetic.MP3"},LLFilePicker::FFLOAD_ALL,LLFilePicker::FFSAVE_ALL);
}
void part(int i){std::ofstream(directory.last+llformat("/part_%05d.wav",i))<<"synthetic WAV";}
void confirm(int option){LLSD response;response.number=option;auto f=std::move(LLNotificationsUtil::confirm);LLNotificationsUtil::confirm={};f(LLSD(),response);}
int main(int argc,char**argv) {
 assert(argc==2);work=argv[1];std::ofstream(work+"/tasia-ffmpeg.exe")<<"synthetic converter";
 select(); const auto first=directory.last;part(0);part(1);tick();
 assert(LLNotificationsUtil::last=="ConfirmMP3BatchUpload");
 assert(LLNotificationsUtil::args["TOTAL_COST"].number==20);
 assert(LLViewerAssetUpload::queue.empty());
 assert(std::find(LLProcess::last.args.values.begin(),LLProcess::last.args.values.end(),"29.00")!=LLProcess::last.args.values.end());
 assert(std::find(LLProcess::last.args.values.begin(),LLProcess::last.args.values.end(),"-nostdin")!=LLProcess::last.args.values.end());
 confirm(0);assert(LLViewerAssetUpload::queue.size()==1);
 LLSD result;result["new_asset"]="synthetic-asset";
 auto upload=LLViewerAssetUpload::queue.back();LLViewerAssetUpload::queue.clear();upload->finishUpload(result);upload.reset();tick();
 assert(LLViewerAssetUpload::queue.size()==1);
 upload=LLViewerAssetUpload::queue.back();LLViewerAssetUpload::queue.clear();upload->finishUpload(result);upload.reset();tick();
 assert(LLNotificationsUtil::last=="MP3BatchFinished");assert(!std::filesystem::exists(first));
 select();const auto cancelled=directory.last;part(0);tick();confirm(1);
 assert(!std::filesystem::exists(cancelled));assert(LLViewerAssetUpload::queue.empty());
 select();const auto repriced=directory.last;part(0);tick();LLAgentBenefitsMgr::current().cost=11;confirm(0);
 assert(LLNotificationsUtil::last=="MP3BatchCostUnavailable");assert(!std::filesystem::exists(repriced));
 LLAgentBenefitsMgr::current().cost=10;
 select();const auto invalid=directory.last;part(0);valid_wav=false;tick();valid_wav=true;
 assert(LLNotificationsUtil::last=="MP3BatchConversionFailed");assert(!std::filesystem::exists(invalid));
 select();LLProcess::running=true;cancel_mp3_batch_sound_upload();tick();
 assert(LLProcess::killed);assert(LLNotificationsUtil::last=="MP3BatchCancelled");
 select();part(512);tick();assert(LLNotificationsUtil::last=="MP3BatchConversionFailed");
 select();part(0);tick();gAgent.session.value="another-session";confirm(0);
 assert(LLNotificationsUtil::last=="MP3BatchCancelled");
 directory.collide=true;std::filesystem::create_directory(work+"/existing");std::ofstream(work+"/existing/preserve.wav")<<"keep";
 select();assert(std::filesystem::exists(work+"/existing/preserve.wav"));
}
'''
clipboard_main = r'''
int main(int argc,char** argv) {
 assert(argc==2);const std::string file=std::string(argv[1])+"/capture.png";
 png_data={137,80,78,71,13,10,26,10,0,0,0,13,'I','H','D','R',0,0,0,2,0,0,0,1};
 assert(capture_clipboard_png(file));assert(opened==closed);assert(locks==unlocks);
 png_data[16]=0xff;assert(!capture_clipboard_png(file));assert(opened==closed);assert(locks==unlocks);
 png_present=false;bitmap_present=true;
 assert(capture_clipboard_png(file));assert((encoded_pixels==std::vector<U8>{1,2,3,255,4,5,6,255}));
 rows_ok=false;assert(!capture_clipboard_png(file));assert(opened==closed);
 rows_ok=true;bitmap_info.bmWidth=9000;assert(!capture_clipboard_png(file));
 open_ok=false;assert(!capture_clipboard_png(file));assert(opened==closed);
 LLFile::remove(file);
}
'''
capture_start=clipboard.index('bool capture_clipboard_png(')
capture=clipboard[capture_start:clipboard.index('\n#endif',capture_start)]
with tempfile.TemporaryDirectory(prefix='tasia-feature-check-',dir='/tmp/opencode') as directory:
    tmp=Path(directory)
    for name,content in [('policy',policy_main),('mp3',(root/'tests/tasia_mp3_fixture.cpp').read_text()+'\n'+
            re.sub(r'^#include[^\n]*\n','',mp3,flags=re.M)+'\n'+mp3_main),
            ('clipboard',(root/'tests/tasia_clipboard_fixture.cpp').read_text()+'\n'+capture+clipboard_main)]:
        cpp=tmp/(name+'.cpp');cpp.write_text(content)
        exe=tmp/name
        subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-DLL_WINDOWS=1',
            '-I'+str(root),str(cpp),'-o',str(exe)],check=True)
        subprocess.run([str(exe),str(tmp)],check=True)
    ffmpeg=shutil.which('ffmpeg')
    if ffmpeg:
        # Exercise the conversion/segment format locally, using synthetic audio only.
        audio=tmp/'test.mp3'
        subprocess.run([ffmpeg,'-nostdin','-v','error','-f','lavfi','-i','sine=frequency=440:duration=31',
            '-c:a','libmp3lame',str(audio)],check=True)
        subprocess.run([ffmpeg,'-nostdin','-v','error','-i',str(audio),'-map','0:a:0','-vn',
            '-ar','44100','-ac','2','-c:a','pcm_s16le','-f','segment','-segment_time','29.00',
            str(tmp/'audio_%05d.wav')],check=True)
        parts=sorted(tmp.glob('audio_*.wav'));assert len(parts)==2
        for part in parts:
            with wave.open(str(part),'rb') as wav:
                assert wav.getnchannels()==2 and wav.getframerate()==44100 and wav.getsampwidth()==2
                assert 0 < wav.getnframes()/wav.getframerate() <= 30
        print('PASS: local Linux FFmpeg synthetic MP3 -> two validated WAV segments.')
    else:
        print('SKIP: local FFmpeg unavailable; conversion smoke test not run.')
print('PASS: feature wiring, compiled production MP3/capture fixtures and real text policy. Windows/CEF/API runtime NOT tested.')

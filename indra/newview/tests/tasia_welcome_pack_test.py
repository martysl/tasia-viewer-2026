#!/usr/bin/env python3
"""Exercise the actual welcome-pack postBuild destination and trust policy."""
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET

from tasia_portable_branding_test import method

root=Path(__file__).resolve().parents[3]
viewer=root/'indra/newview'
source=(viewer/'llfloateravatarwelcomepack.cpp').read_text()
body=method(source,'bool LLFloaterAvatarWelcomePack::postBuild()')
fixture=r'''
#include <cassert>
#include <string>
const char* HTTP_CONTENT_TEXT_HTML="text/html";
struct LLMediaCtrl {
 std::string url,mime;bool trusted=true,downloads=true;int clears=0;
 void clearCache(){++clears;}void setTrustedContent(bool v){trusted=v;}
 void setAllowFileDownload(bool v){downloads=v;}void setErrorPageURL(const std::string&){}
 void navigateTo(const std::string& u,const std::string& m){url=u;mime=m;}
} browser;
struct Settings {std::string getString(const char* name){assert(std::string(name)=="GenericErrorPageURL");return "about:blank";}} gSavedSettings;
struct LLFloaterAvatarWelcomePack {
 LLMediaCtrl* mAvatarPicker=nullptr;bool has_browser=true;
 template<class T>T* findChild(const char*){return has_browser?&browser:nullptr;}
 bool postBuild();
};
'''+body+r'''
int main(){LLFloaterAvatarWelcomePack floater;assert(floater.postBuild());
 assert(browser.url=="https://startup.tasiaviewer.work/welcomepack/");
 assert(browser.mime=="text/html"&&!browser.trusted&&!browser.downloads);
 floater.has_browser=false;assert(floater.postBuild());}
'''
with tempfile.TemporaryDirectory(prefix='welcome-pack-',dir='/tmp/opencode') as tmp:
    path=Path(tmp);(path/'test.cpp').write_text(fixture)
    subprocess.run(['c++','-std=c++17','-Wall','-Werror',str(path/'test.cpp'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
nodes=list(ET.parse(viewer/'app_settings/settings.xml').getroot().find('map'))
index=next(i for i,n in enumerate(nodes) if n.tag=='key' and n.text=='AvatarWelcomePack')
fields=list(nodes[index+1]);values={fields[i].text:fields[i+1].text for i in range(0,len(fields),2)}
assert values['Value']=='https://startup.tasiaviewer.work/welcomepack/'
xml=ET.parse(viewer/'skins/default/xui/en/floater_avatar_welcome_pack.xml')
assert xml.find('.//web_browser').get('trusted_content')=='false'
print('PASS: actual welcome-pack postBuild, exact destination despite copied old settings, untrusted/no-download policy. CEF runtime not tested.')

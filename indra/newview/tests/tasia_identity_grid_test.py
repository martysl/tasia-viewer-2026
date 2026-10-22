#!/usr/bin/env python3
"""Compile production identity/catalog policy with fixture LLSD/transport types.

Does not claim full Windows compilation, live HTTP, or native UI verification.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET
from tasia_portable_branding_test import method

ROOT = Path(__file__).resolve().parents[3]
NV = ROOT/'indra/newview'

COMMON = r'''
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
using U32=unsigned;using U64=uint64_t;using S32=int;using F32=float;
struct LLSD {
    enum Type {UNDEF,MAP,ARRAY,STRING,BOOL} type=UNDEF;
    std::map<std::string,LLSD> fields;std::vector<LLSD> array;std::string text;
    using array_const_iterator=std::vector<LLSD>::const_iterator;
    LLSD()=default;LLSD(const char* p):type(STRING),text(p){} LLSD(std::string p):type(STRING),text(p){}
    LLSD(bool b):type(BOOL),text(b?"true":"false"){}
    static LLSD emptyMap(){LLSD d;d.type=MAP;return d;}static LLSD emptyArray(){LLSD d;d.type=ARRAY;return d;}
    bool isMap()const{return type==MAP;}bool isArray()const{return type==ARRAY;}bool isString()const{return type==STRING;}
    bool has(const std::string& key)const{return fields.count(key);}
    void erase(const std::string& key){fields.erase(key);}
    LLSD& operator[](const std::string& key){type=MAP;return fields[key];}
    const LLSD& operator[](const std::string& key)const{static LLSD nil;auto it=fields.find(key);return it==fields.end()?nil:it->second;}
    LLSD& operator[](int i){return array.at(i);}const LLSD& operator[](int i)const{return array.at(i);}
    std::string asString()const{return text;}
    void append(LLSD d){type=ARRAY;array.push_back(d);}
    int size()const{return isArray()?array.size():fields.size();}
    auto beginMap(){return fields.begin();}auto endMap(){return fields.end();}
    auto beginArray()const{return array.begin();}auto endArray()const{return array.end();}
};
std::string utf8str_tolower(std::string s){for(auto& c:s)c=std::tolower(static_cast<unsigned char>(c));return s;}
struct LLStringUtil{
 static void trim(std::string& s){auto b=s.find_first_not_of(" \r\n\t");auto e=s.find_last_not_of(" \r\n\t");s=b==s.npos?"":s.substr(b,e-b+1);}
 static bool startsWith(const std::string& s,const char* p){return s.find(p)==0;}
};
struct LLURI{
 std::string value;LLURI(std::string s):value(s){}
 std::string scheme()const{auto p=value.find("://");return p==value.npos?"":value.substr(0,p);}
 std::string authority()const{auto p=value.find("://");if(p==value.npos)return "";p+=3;return value.substr(p,value.find_first_of("/?#",p)-p);}
 std::string hostName()const{auto a=authority();return a.substr(0,a.find(':'));}
};
std::ostringstream logs;
#define LL_WARNS(tag) logs
#define LL_INFOS(tag) logs
#define LL_ENDL "\n"
'''


def compile_run(source):
    with tempfile.TemporaryDirectory(dir='/tmp/opencode', prefix='identity-grid-') as tmp:
        cpp, exe = Path(tmp)/'fixture.cpp', Path(tmp)/'fixture'
        cpp.write_text(source)
        subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',
                        '-fsanitize=address,undefined',str(cpp),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)


class IdentityAndGrids(unittest.TestCase):
    def test_actual_identity_parser_and_grid_uuid_lookup(self):
        source = (NV/'lltasia_user_config.cpp').read_text()
        header = (NV/'lltasia_user_config.h').read_text()
        declaration = header[header.index('class LLTasiaUserConfig'):header.rindex('#endif')]
        prefix = r'''
struct LLUUID {
 std::string id;LLUUID(std::string s=""):id(s){}
 bool isNull()const{return !std::regex_match(id,std::regex("[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}"));}
 std::string asString()const{return id;}
};
struct LLColor4{float r=0,g=0,b=0,a=0;void set(float R,float G,float B,float A){r=R;g=G;b=B;a=A;}};
struct LLAvatarName;
struct LLVOAvatar{static void invalidateNameTags(){}};
struct Agent{LLUUID getSessionID()const{return LLUUID("11111111-1111-1111-1111-111111111111");}}gAgent;
struct Settings{bool enabled=true;bool getBOOL(const char*)const{return enabled;}}gSavedSettings;
struct LLGridManager{
 std::string grid="grid-a.test:8002";
 static LLGridManager* getInstance(){static LLGridManager m;return &m;}
 std::string getGrid()const{return grid;}
 std::string resolveGridKey(std::string v){
   if(v=="secondlife.com")return "util.agni.lindenlab.com";
   if(v=="grid-a.test:8002"||v=="grid-b.test:8002"||v=="grid-c.test:8002"||v=="active.test:8002")return v;
   return "";
 }
 std::map<std::string,std::string> getKnownGrids(){return {{"grid-a.test:8002","A"},{"grid-b.test:8002","B"},
   {"i.let-us.cyou:8002","I-Grid"},{"os.tasia.work.gd:22000","Fresh"}};}
};
'''
        namespace = source[source.index('const S32 MAX_USERS'):source.index('bool sRequested')]
        functions = '\n'.join(method(source,name) for name in (
            'std::string gridKey(', 'std::string resolveEntryGrid(', 'std::string cleanText(',
            'std::string getCleanString(',
            'std::string getFirstString(', 'bool startsWithHttpURL(', 'std::string getCleanURL(',
            'bool parseHexByte(', 'bool parseColorString(', 'bool parseColor(',
            'LLTasiaUserConfig::User parseUser(', 'void applyConfig(',
            'std::string LLTasiaUserConfig::User::getNametagTitle(',
            'bool LLTasiaUserConfig::User::hasProfileBadge(', 'bool LLTasiaUserConfig::getUser('))
        # The production signatures gained parameters; give the fixture defaults
        # so the extracted bodies compile unchanged.
        functions = functions.replace('void applyConfig(const LLSD& response, const std::string& source_grid)',
                                      'void applyConfig(const LLSD& response, const std::string& source_grid = std::string())')
        fixture = r'''
int main(){
 const char* id="aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
 LLSD config=LLSD::emptyMap();config["users"]=LLSD::emptyArray();
 LLSD a=LLSD::emptyMap();a["grid"]="https://GRID-A.test:8002/";a["uuid"]=id;
  a["title"]="Creator";a["display_name"]="Local alias";a["color"]="#12ab34";
  a["username"]="Wrong user";a["page_name"]="Wrong page name";a["account_type"]="Wrong account type";
 LLSD b=a;b["grid"]="grid-b.test:8002";b["title"]="Guest";
 config["users"].append(a);config["users"].append(b);applyConfig(config);
 LLTasiaUserConfig::User user;assert(sUsers.size()==2);
  assert(LLTasiaUserConfig::getUser(LLUUID(id),user)&&user.custom_title=="Creator"&&user.has_tag_color);
  assert(user.cosmetic_display_name=="Local alias"&&user.cosmetic_username=="Wrong user"&&user.cosmetic_account_type.empty());
  LLSD website_only=a;website_only["display_name"]="  ";website_only["username"]="\t";
  auto website_user=parseUser(website_only);
  assert(website_user.cosmetic_display_name.empty()&&website_user.cosmetic_username.empty());
  website_only.fields.erase("display_name");website_only.fields.erase("username");
  website_user=parseUser(website_only);assert(website_user.cosmetic_display_name.empty()&&website_user.cosmetic_username.empty());
 LLGridManager::getInstance()->grid="grid-b.test:8002";
 assert(LLTasiaUserConfig::getUser(LLUUID(id),user)&&user.custom_title=="Guest");
 LLGridManager::getInstance()->grid="grid-c.test:8002";assert(!LLTasiaUserConfig::getUser(LLUUID(id),user));
 applyConfig(LLSD::emptyMap());assert(sUsers.size()==2); // malformed refresh retains last good
 config["users"].append(a);applyConfig(config);assert(sUsers.size()==2); // duplicate identity rejects whole response
 LLSD legacy;legacy["users"]=LLSD::emptyArray();a.fields.erase("grid");legacy["users"].append(a);
 applyConfig(legacy);assert(sUsers.empty()); // no grid and no source grid grants no alias
 applyConfig(legacy,"grid-a.test:8002");assert(sUsers.size()==1); // source grid scopes it
 LLColor4 color;assert(!parseColorString("#+fab34",color));
 assert(gridKey("https://user:pass@grid.test/").empty());
 // Friendly names must resolve to the internal grid key, never be stored raw.
 assert(resolveEntryGrid("secondlife.com")=="util.agni.lindenlab.com");
 assert(resolveEntryGrid("grid-a.test:8002")=="grid-a.test:8002");
 assert(resolveEntryGrid("https://grid-a.test:8002/")=="grid-a.test:8002");
 assert(resolveEntryGrid("unknown.test:8002")=="unknown.test:8002");
}
'''
        compile_run(COMMON+prefix+declaration+namespace+'bool sLoaded=false;user_map_t sUsers;\n'+functions+fixture)

    def test_actual_catalog_merge_preserves_entries_and_removals(self):
        source = (NV/'llviewernetwork.cpp').read_text()
        prefix = r'''
LLSD parsed_catalog;
struct LLSDSerialize{static void fromXMLDocument(LLSD& out,std::istream&){out=parsed_catalog;}};
const char* CRED_IDENTIFIER_TYPE_AGENT="agent";
const std::string DEFAULT_LOGIN_PAGE="https://startup.tasiaviewer.work/";
struct LLGridManager{
 LLSD mGridList=LLSD::emptyMap(),mRemovedGrids=LLSD::emptyMap();bool save_ok=true;int saves=0;
 std::string mGrid="active.test:8002";
 std::string getGrid(std::string k){return mGridList.has(k)?k:"";}
 std::string resolveGridKey(std::string v){
   if(v=="secondlife.com")return "util.agni.lindenlab.com";
   if(v=="active.test:8002"||v=="grid-a.test:8002"||v=="grid-b.test:8002"||v=="grid-c.test:8002")return v;
   return "";
 }
 bool isSystemGrid(const std::string& k){return k=="util.agni.lindenlab.com";}
 bool addGrid(LLSD d){mGridList[d["keyname"].asString()]=d;return true;}
 bool saveCustomGrids(std::string&){++saves;return save_ok;}
 bool importGridCatalog(const std::string&,std::string&,bool persist);
 bool removeCustomGrid(const std::string&,std::string&);
 bool setGridMetadata(const std::string&,const LLSD&,std::string&);
 std::map<std::string,std::string> getKnownGrids(){return {{"grid-a.test:8002","A"},{"grid-b.test:8002","B"},
   {"active.test:8002","Active"},{"i.let-us.cyou:8002","I-Grid"},{"os.tasia.work.gd:22000","Fresh"}};}
};
'''
        functions = '\n'.join(method(source,n) for n in (
            'bool gridURL(', 'bool gridMetadata(', 'bool LLGridManager::importGridCatalog(',
            'bool LLGridManager::removeCustomGrid(', 'bool LLGridManager::setGridMetadata('))
        functions = functions.replace('bool LLGridManager::importGridCatalog(const std::string& xml, std::string& error, bool persist)',
                                      'bool LLGridManager::importGridCatalog(const std::string& xml, std::string& error, bool persist)')
        functions = functions.replace('m.importGridCatalog("fixture",error,true)',
                                      'm.importGridCatalog("fixture",error,true)')
        functions = functions.replace('m.importGridCatalog("<!DOCTYPE bad>",error,true)',
                                      'm.importGridCatalog("<!DOCTYPE bad>",error,true)')
        functions = functions.replace('m.importGridCatalog(std::string(262145,\'x\'),error)',
                                      'm.importGridCatalog(std::string(262145,\'x\'),error,true)')
        fixture = r'''
LLSD entry(const char* url){LLSD d=LLSD::emptyMap();d["loginuri"]=LLSD::emptyArray();d["loginuri"].append(url);d["gridname"]="Example";return d;}
int main(){
 LLGridManager m;std::string error;m.mGridList["existing.test:8002"]["label"]="My edit";
 m.mRemovedGrids["removed.test:8002"]=true;
 parsed_catalog=LLSD::emptyArray();parsed_catalog.append(entry("https://existing.test:8002/"));
 parsed_catalog.append(entry("https://removed.test:8002/"));auto fresh=entry("http://new.test:8002/");
 fresh["loginuri"].append("https://backup.test:8002/");fresh["helperuri"]="https://new.test/helpers/";
 parsed_catalog.append(fresh);assert(m.importGridCatalog("fixture",error,true));
 assert(m.mGridList.size()==2 && m.mGridList["existing.test:8002"]["label"].asString()=="My edit");
 assert(!m.mGridList.has("removed.test:8002"));assert(m.mGridList["new.test:8002"]["login_uri"].size()==2);
 assert(m.mGridList["new.test:8002"]["helper_uri"].asString()=="https://new.test/helpers/");
 assert(!m.mGridList["new.test:8002"].has("system_grid"));
 parsed_catalog.append(entry("javascript:alert(1)"));m.mGridList=LLSD::emptyMap();
 assert(!m.importGridCatalog("fixture",error,true)&&m.mGridList.size()==0); // atomic invalid-tail rejection
 parsed_catalog=LLSD::emptyArray();parsed_catalog.append(entry("https://new.test:8002/"));m.save_ok=false;
 assert(!m.importGridCatalog("fixture",error,true)&&m.mGridList.size()==0); // disk-save rollback
 assert(!m.importGridCatalog("<!DOCTYPE bad>",error,true));assert(!m.importGridCatalog(std::string(262145,'x'),error,true));
 assert(!gridURL("https://grid.test:99999/",true));assert(!gridURL("https://u:p@grid.test/",true));
 assert(gridURL("https://grid.test/register?action=new"));assert(!gridURL("https://grid.test/?secret=x",true));
 m.save_ok=true;m.mGridList["custom.test:8002"]["label"]="Custom";
 LLSD metadata;metadata["helper_uri"]="https://custom.test/economy/";metadata["login_page"]="about:blank";
 assert(m.setGridMetadata("custom.test:8002",metadata,error));
 assert(m.mGridList["custom.test:8002"]["helper_uri"].asString()=="https://custom.test/economy/");
 assert(m.removeCustomGrid("custom.test:8002",error));
 assert(m.mRemovedGrids.has("custom.test:8002")&&!m.mGridList.has("custom.test:8002"));
 m.mGridList["keep.test:8002"]["label"]="Keep";m.save_ok=false;
 assert(!m.removeCustomGrid("keep.test:8002",error));
 assert(m.mGridList.has("keep.test:8002")&&!m.mRemovedGrids.has("keep.test:8002"));
 m.mGridList["util.agni.lindenlab.com"]["label"]="Second Life";
 assert(!m.setGridMetadata("util.agni.lindenlab.com",metadata,error));
 assert(!m.removeCustomGrid("util.agni.lindenlab.com",error));
 m.mGridList[m.mGrid]["label"]="Active";assert(!m.removeCustomGrid(m.mGrid,error));
}
'''
        compile_run(COMMON+prefix+functions+fixture)

    def test_actual_http_write_limit(self):
        source = (ROOT/'indra/llcorehttp/_httpoprequest.cpp').read_text()
        prefix = r'''
struct BufferArray{std::string data;size_t size()const{return data.size();}size_t append(const char* p,size_t n){data.append(p,n);return n;}};
struct Options{size_t limit=5;size_t getBodyLimit()const{return limit;}};
struct HTTPStats{static HTTPStats& instance(){static HTTPStats s;return s;}void recordDataDown(size_t){}};
struct HttpOpRequest{
 using ptr_t=HttpOpRequest*;BufferArray* mReplyBody=nullptr;std::shared_ptr<Options> mReqOptions=std::make_shared<Options>();
 template<class T>static T* fromHandle(void* p){return static_cast<T*>(p);}
 static size_t writeCallback(void*,size_t,size_t,void*);
 ~HttpOpRequest(){delete mReplyBody;}
};
'''
        compile_run(COMMON+prefix+method(source,'size_t HttpOpRequest::writeCallback(')+r'''
int main(){
 HttpOpRequest op;char data[]="123456";
 assert(HttpOpRequest::writeCallback(data,1,4,&op)==4);
 assert(HttpOpRequest::writeCallback(data,1,2,&op)==0&&op.mReplyBody->size()==4);
 assert(HttpOpRequest::writeCallback(data,1,1,&op)==1&&op.mReplyBody->size()==5);
 assert(HttpOpRequest::writeCallback(data,(std::numeric_limits<size_t>::max)(),2,&op)==0);
 op.mReqOptions->limit=0;assert(HttpOpRequest::writeCallback(data,1,6,&op)==6);
}
''')

    def test_settings_and_ui_contract(self):
        root = ET.parse(NV/'app_settings/settings.xml').getroot()
        self.assertEqual([node.tag for node in root], ['map'])
        keys = [node.text for node in root[0] if node.tag == 'key']
        self.assertEqual(len(keys), len(set(keys)))
        for key in ('TasiaGridCatalogEnabled','TasiaRemoteUserConfigEnabled','TasiaRemoteUserConfigURL'):
            self.assertIn(key, keys)
        panel = ET.parse(NV/'skins/default/xui/en/floater_grid_manager.xml').getroot()
        names = {node.get('name') for node in panel}
        for key in ('discover_grid','save_metadata','update_catalog','login_page','helper_uri','register','password','gatekeeper'):
            self.assertIn(key, names)
        ET.parse(NV/'app_settings/tasia_default_grids.xml')
        self.assertIn('"tasia_default_grids.xml"', (NV/'Copy-ViewerManifest.ps1').read_text())
        fetch = (NV/'lltasia_config_fetch.cpp').read_text()
        for guard in ('setBodyLimit(limit)','setSSLVerifyPeer(true)','setSSLVerifyHost(true)','setFollowRedirects(false)'):
            self.assertIn(guard, fetch)
        identity = (NV/'lltasia_user_config.cpp').read_text()
        for guard in ('generation != sGeneration','gAgent.getSessionID() != session','+ 300.0'):
            self.assertIn(guard, identity)


if __name__ == '__main__':
    unittest.main(verbosity=2)

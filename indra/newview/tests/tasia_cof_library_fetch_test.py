#!/usr/bin/env python3
"""Compile actual bounded COF library request/response-guard code with fixtures.
No real inventory, network or avatar data is used.
"""
from pathlib import Path
import subprocess
import tempfile

from tasia_portable_branding_test import method

root=Path(__file__).resolve().parents[3]
source=(root/'indra/newview/llagentwearables.cpp').read_text()
handler=method(source,'class LLTasiaCOFLibraryFetch final')+';'
request=method(source,'bool requestLegacyCOFLibraryLinks(')
fixture=r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
using U64=std::uint64_t;
using S32=int;
struct LLUUID {int value=0;bool isNull()const{return value==0;}bool operator==(LLUUID other)const{return value==other.value;}bool operator!=(LLUUID other)const{return value!=other.value;}};
using uuid_vec_t=std::vector<LLUUID>;
struct LLSD {std::map<std::string,LLSD> fields;std::vector<LLSD> array;LLUUID id;
 LLSD& operator[](const std::string& key){return fields[key];}LLSD& operator=(LLUUID value){id=value;return *this;}
 void append(const LLSD& value){array.push_back(value);}};
namespace LLCore {
 using HttpHandle=void*;struct HttpResponse{};
 struct HttpHandler {using ptr_t=std::shared_ptr<HttpHandler>;virtual ~HttpHandler()=default;virtual void onCompleted(HttpHandle,HttpResponse*){}};
}
#define LLCORE_HTTP_HANDLE_INVALID nullptr
namespace LLAssetType {enum {AT_LINK=1};}
struct Item {int actual=1;LLUUID target;bool finished=false;int getActualType()const{return actual;}
 LLUUID getLinkedUUID()const{return target;}bool isFinished()const{return finished;}};
int imported=0;
struct LLInventoryModel {
 using item_array_t=std::vector<Item*>;
 struct FetchItemHttpHandler:LLCore::HttpHandler {explicit FetchItemHttpHandler(const LLSD&){}
 void onCompleted(LLCore::HttpHandle,LLCore::HttpResponse*)override{++imported;}};
};
struct Wearables {bool current=true;U64 generation=7;bool isLegacyInitialWearablesCurrent(U64 g)const{return current&&g==generation;}} gAgentWearables;
struct Region {std::string cap="https://grid.test/cap";std::string getCapability(const char* name){assert(std::string(name)=="FetchLib2");return cap;}} region;
struct Agent {bool has_region=true;Region* getRegion(){return has_region?&region:nullptr;}LLUUID getID()const{return {1};}} gAgent;
struct Category {int version=3;int getVersion()const{return version;}} category;
struct LLAppearanceMgr {LLUUID cof{500};static LLAppearanceMgr& instance(){static LLAppearanceMgr mgr;return mgr;}LLUUID getCOF()const{return cof;}};
struct Inventory {
 LLUUID owner{99};std::map<int,Item> known;int posts=0;bool queue=true;LLSD last;std::string last_url;LLCore::HttpHandler::ptr_t response;
 bool category_present=true,complete=true;
 Category* getCategory(LLUUID){return category_present?&category:nullptr;}bool isCategoryComplete(LLUUID)const{return complete;}
 LLUUID getLibraryOwnerID()const{return owner;}Item* getItem(LLUUID id){auto i=known.find(id.value);return i==known.end()?nullptr:&i->second;}
 LLCore::HttpHandle requestPost(bool foreground,const std::string& url,const LLSD& body,const LLCore::HttpHandler::ptr_t& handler,const char* const label){
 assert(foreground&&std::string(label)=="COF Library Item");++posts;last=body;last_url=url;response=handler;return queue?reinterpret_cast<void*>(1):nullptr;}
} gInventory;
std::ostringstream log_stream;
#define LL_WARNS(tag) log_stream
#define LL_ENDL "\n"
'''
fixture+=handler+'\n'+request+r'''
int main(){
 Item first{1,{10},false},duplicate{1,{10},false},second{1,{11},false},null{1,{0},false},folder{2,{12},false};
 LLInventoryModel::item_array_t links{&first,&duplicate,&second,&null,&folder};
 assert(requestLegacyCOFLibraryLinks(links,7));assert(gInventory.posts==1);
 assert(gInventory.last.fields["agent_id"].id.value==1);
 auto& items=gInventory.last.fields["items"].array;assert(items.size()==2);
 assert(items[0].fields["owner_id"].id.value==99&&items[1].fields["owner_id"].id.value==99);
 assert(items[0].fields["item_id"].id.value==10&&items[1].fields["item_id"].id.value==11);
 auto response=gInventory.response;
 gAgentWearables.current=false;response->onCompleted(nullptr,nullptr);assert(imported==0);
 gAgentWearables.current=true;gAgentWearables.generation=8;response->onCompleted(nullptr,nullptr);assert(imported==0);
 gAgentWearables.generation=7;response->onCompleted(nullptr,nullptr);assert(imported==1);
 LLAppearanceMgr::instance().cof={501};response->onCompleted(nullptr,nullptr);assert(imported==1);LLAppearanceMgr::instance().cof={500};
 ++category.version;response->onCompleted(nullptr,nullptr);assert(imported==1);--category.version;
 gInventory.complete=false;response->onCompleted(nullptr,nullptr);assert(imported==1);gInventory.complete=true;
 gInventory.category_present=false;response->onCompleted(nullptr,nullptr);assert(imported==1);gInventory.category_present=true;
 assert(!requestLegacyCOFLibraryLinks(links,6)&&gInventory.posts==1);
 gAgent.has_region=false;assert(!requestLegacyCOFLibraryLinks(links,7));gAgent.has_region=true;
 gInventory.owner={};assert(!requestLegacyCOFLibraryLinks(links,7));gInventory.owner={1};assert(!requestLegacyCOFLibraryLinks(links,7));gInventory.owner={99};
 region.cap.clear();assert(!requestLegacyCOFLibraryLinks(links,7));region.cap="https://grid.test/cap";
 gInventory.known[10]={1,{10},true};gInventory.known[11]={1,{11},true};
 assert(!requestLegacyCOFLibraryLinks(links,7));gInventory.known.clear();
 std::vector<Item> too_many;for(int i=1;i<258;++i)too_many.push_back({1,{i},false});
 LLInventoryModel::item_array_t big;for(auto& item:too_many)big.push_back(&item);
 assert(!requestLegacyCOFLibraryLinks(big,7)&&gInventory.posts==1);
 gInventory.queue=false;assert(!requestLegacyCOFLibraryLinks(links,7));
}
'''
with tempfile.TemporaryDirectory(prefix='cof-library-',dir='/tmp/opencode') as tmp:
    path=Path(tmp);(path/'test.cpp').write_text(fixture)
    subprocess.run(['c++','-std=c++17','-Wall','-Werror',str(path/'test.cpp'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)

header=(root/'indra/newview/llinventorymodel.h').read_text()
assert 'const LLCore::HttpHandler::ptr_t &handler' in header
assert 'virtual void onCompleted(LLCore::HttpHandle handle' in header
retry=method(source,'void LLAgentWearables::retryLegacyCOFLinkFetch(')
assert 'if (!mLegacyCOFLibraryFetchAttempted)' in retry
assert retry.index('mLegacyCOFLibraryFetchAttempted = true')<retry.index('requestLegacyCOFLibraryLinks(')
assert 'mLegacyCOFLibraryFetchAttempted = false;' in method(source,'void LLAgentWearables::cleanup(')
print('PASS: production COF library request/response guards, advertised owner, bounded/deduplicated unresolved targets; no live network/avatar proof.')

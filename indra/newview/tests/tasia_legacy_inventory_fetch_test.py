#!/usr/bin/env python3
"""Compile production legacy inventory fetch methods against small service doubles."""
from pathlib import Path
import subprocess
import tempfile
import unittest

from tasia_portable_branding_test import method

ROOT = Path(__file__).resolve().parents[3]
NV = ROOT / 'indra/newview'


class LegacyInventoryFetch(unittest.TestCase):
    def test_production_item_and_folder_fetch_routes(self):
        inventory = (NV / 'llviewerinventory.cpp').read_text()
        model = (NV / 'llinventorymodel.cpp').read_text()
        item_fetch = method(inventory, 'void LLViewerInventoryItem::fetchFromServer(void) const')
        folder_fetch = method(inventory, 'bool LLViewerInventoryCategory::fetch(S32 expiry_seconds)')

        fixture = r'''
#include <cassert>
#include <memory>
#include <sstream>
#include <string>
using S32=int; using U32=unsigned; using BOOL=bool; using F32=float;
std::ostringstream log_stream;
#define LL_WARNS(x) log_stream
#define LL_WARNS_ONCE(x) log_stream
#define LL_DEBUGS(x) log_stream
#define LL_ENDL '\n'
const char* LOG_INV="Inventory";
struct LLUUID { int v=1; bool isNull() const { return v==0; } bool operator!=(const LLUUID& o) const {return v!=o.v;} };
std::ostream& operator<<(std::ostream& o,const LLUUID& id){return o<<id.v;}
struct LLSD { LLSD& operator[](const char*) {return *this;} LLSD& operator[](int) {return *this;} template<class T> LLSD& operator=(const T&) {return *this;} };
namespace LLCore { struct HttpHandler {using ptr_t=std::shared_ptr<void>;}; }
struct LLViewerRegion { std::string url; std::string getCapability(const char*) {return url;} } region;
struct Agent { LLViewerRegion* getRegion(){return &region;} LLUUID getID(){return {};} LLUUID getSessionID(){return {};} void sendReliableMessage(){++reliable;} int reliable=0; } gAgent;
struct LLGridManager {bool os=false; static LLGridManager* getInstance(){static LLGridManager g;return &g;} bool isInOpenSim(){return os;}};
struct AISAPI {static bool available; static bool isAvailable(){return available;}}; bool AISAPI::available=false;
struct BackgroundFetch {static BackgroundFetch* getInstance(){static BackgroundFetch f;return &f;} static BackgroundFetch& instance(){return *getInstance();} void scheduleItemFetch(LLUUID){++scheduled;} void start(LLUUID,bool){++started;} int scheduled=0,started=0;};
struct Message {std::string name; int sends=0; void newMessage(const char* s){name=s;} void nextBlock(const char*){} void addUUID(const char*,LLUUID){} void addS32(const char*,int){} void addBOOL(const char*,bool){} };
using LLMessageSystem=Message; LLMessageSystem* gMessageSystem;
struct Settings {U32 getU32(const char*){return 1;}} gSavedSettings;
struct LLInventoryPanel {static constexpr const char* DEFAULT_SORT_ORDER="sort";};
struct Permissions {LLUUID getOwner()const{return {};}};
struct LLViewerInventoryItem {bool mIsComplete=false; Permissions mPermissions; LLUUID mUUID; void fetchFromServer()const;};
struct Timer {bool expired=true; bool hasExpired()const{return expired;} void reset(){expired=false;} void setTimerExpirySec(float){expired=false;}};
struct LLViewerInventoryCategory {static constexpr int VERSION_UNKNOWN=-1; int version=-1; std::string mName="folder"; LLUUID mUUID,mOwnerID; Timer mDescendentsRequested; int getVersion()const{return version;} bool fetch(S32);};
struct LLInventoryModel {struct FetchItemHttpHandler {explicit FetchItemHttpHandler(const LLSD&){} }; void requestPost(bool,const std::string&,const LLSD&,LLCore::HttpHandler::ptr_t,const char*){++http;} int http=0;};
LLInventoryModel gInventory;
// Production names the common singleton type for both item and folder scheduling.
using LLInventoryModelBackgroundFetch = BackgroundFetch;
'''
        fixture += item_fetch + '\n' + folder_fetch + r'''
int main(){
 Message msg;gMessageSystem=&msg;auto* grid=LLGridManager::getInstance();
 LLViewerInventoryItem item;item.mUUID.v=7;
 grid->os=false;item.fetchFromServer();assert(msg.sends==0&&gInventory.http==0);
 grid->os=true;item.fetchFromServer();assert(msg.name=="FetchInventory"&&gAgent.reliable==1);
 region.url="https://grid.example/FetchInventory2";item.fetchFromServer();assert(gInventory.http==1&&gAgent.reliable==1);
  region.url.clear();AISAPI::available=true;item.fetchFromServer();assert(BackgroundFetch::getInstance()->scheduled==1&&gAgent.reliable==1);
 AISAPI::available=false;
  LLViewerInventoryCategory folder;folder.mUUID.v=8;folder.mOwnerID.v=9;
  grid->os=false;folder.fetch(10);assert(gAgent.reliable==1);
  folder.mDescendentsRequested.expired=true;
  grid->os=true;folder.fetch(10);assert(msg.name=="FetchInventoryDescendents"&&gAgent.reliable==2);
 folder.mDescendentsRequested.expired=true;region.url="https://grid.example/FetchInventoryDescendents2";
  folder.fetch(10);assert(BackgroundFetch::getInstance()->started==1&&gAgent.reliable==2);
}
'''
        with tempfile.TemporaryDirectory(dir='/tmp/opencode', prefix='legacy-inventory-') as tmp:
            cpp, exe = Path(tmp) / 'test.cpp', Path(tmp) / 'test'
            cpp.write_text(fixture)
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            str(cpp), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_legacy_reply_handlers_are_registered_and_grid_scoped(self):
        model = (NV / 'llinventorymodel.cpp').read_text()
        header = (NV / 'llinventorymodel.h').read_text()
        registration = method(model, 'void LLInventoryModel::registerCallbacks(')
        fetch_reply = method(model, 'void LLInventoryModel::processFetchInventoryReply(')
        folder_reply = method(model, 'void LLInventoryModel::processInventoryDescendents(')
        self.assertIn('"FetchInventoryReply", processFetchInventoryReply', registration)
        self.assertIn('"InventoryDescendents", processInventoryDescendents', registration)
        self.assertIn('processFetchInventoryReply(LLMessageSystem* msg, void**);', header)
        self.assertIn('processInventoryDescendents(LLMessageSystem* msg, void**);', header)
        for body in (fetch_reply, folder_reply):
            self.assertIn('isInOpenSim()', body)
            self.assertIn('agent_id != gAgent.getID()', body)
        self.assertIn('messageUpdateCore(msg, false)', fetch_reply)
        self.assertIn('gInventory.updateCategory(category)', folder_reply)
        self.assertIn('gInventory.updateItem(item)', folder_reply)


if __name__ == '__main__':
    unittest.main(verbosity=2)

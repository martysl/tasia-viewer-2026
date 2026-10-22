#!/usr/bin/env python3
"""Compile the production picker event handler with instrumented surroundings.

The mock emitter rejects destruction during dispatch, reproducing the dangerous
inline-close ordering. This is a native lifetime regression test, not CEF/GUI.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

from tasia_portable_branding_test import method

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'indra/newview/llfloatertasiamediapicker.cpp'

PREFIX = r'''
#include <cassert>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
struct ID {
 std::string value;
 std::string asString() const { return value; }
 bool operator!=(const ID& other) const { return value!=other.value; }
};
struct Agent { ID session{"session-1"}; ID getSessionID() const { return session; } } gAgent;
bool gDisconnected=false;
struct Settings { bool enabled=true; bool getBOOL(const char*) const { return enabled; } } gSavedSettings;
enum EMediaEvent { MEDIA_EVENT_NAVIGATE_COMPLETE, MEDIA_EVENT_LOCATION_CHANGED };
struct LLPluginClassMedia { std::string location; std::string getLocation() const {return location;} std::string getNavigateURI() const {return "file:///picker.html";} };
struct Browser { LLPluginClassMedia* plugin; LLPluginClassMedia* getMediaPlugin() {return plugin;} };
struct LLView { virtual ~LLView()=default; };
struct Handle { std::shared_ptr<LLView*> slot; LLView* get()const{return *slot;} };
struct LLURI { static std::string unescape(const std::string& value) {return value;} };
bool validSelection(const std::string& url,const std::string&) {return url=="https://giphy.com/gifs/test";}
std::vector<std::function<void()>> idle;
void doOnIdleOneTime(std::function<void()> callback) { idle.push_back(std::move(callback)); }
void flushIdle() {auto callbacks=std::move(idle);idle.clear();for(auto& callback:callbacks)callback();}
bool dispatching=false;
int inserted=0;
struct LLFloaterTasiaMediaPicker : LLView {
 LLPluginClassMedia plugin;
 Browser browser{&plugin}; Browser* mBrowser=&browser;
 bool visible=true,mInitialized=true,emitterAlive=true;
 int closed=0;
 std::string mProvider="giphy",mPageURL="file:///picker.html";
 ID mNonce{"nonce-1"},mSession{"session-1"};
 std::function<void(const std::string&)> mSelect=[](const std::string&){++inserted;};
 std::shared_ptr<LLView*> slot=std::make_shared<LLView*>(this);
 LLFloaterTasiaMediaPicker(){plugin.location=mPageURL+"#select=nonce-1:https://giphy.com/gifs/test";}
 ~LLFloaterTasiaMediaPicker(){*slot=nullptr;}
 bool getVisible() const {return visible;}
 Handle getHandle(){return {slot};}
 void initializePage(){}
 void closeFloater(){
  if(dispatching)throw std::runtime_error("Emitter destroyed while emitEvent still uses it");
  visible=false;mNonce={};mSelect={};mInitialized=false;emitterAlive=false;++closed;
 }
 void handleMediaEvent(LLPluginClassMedia*,EMediaEvent);
};
void dispatch(LLFloaterTasiaMediaPicker& picker){
 dispatching=true;
 try{picker.handleMediaEvent(&picker.plugin,MEDIA_EVENT_LOCATION_CHANGED);}
 catch(...){dispatching=false;throw;}
 // LLViewerMediaImpl::emitEvent accesses its own members after notifying observers.
 if(!picker.emitterAlive)throw std::runtime_error("Use after free after observer returned");
 dispatching=false;
}
'''

SUFFIX = r'''
int main(){
 try {
  {LLFloaterTasiaMediaPicker picker;dispatch(picker);dispatch(picker);
   assert(picker.closed==0&&inserted==0&&idle.size()==1);
   flushIdle();assert(picker.closed==1&&inserted==1);}
  inserted=0;
  {LLFloaterTasiaMediaPicker picker;dispatch(picker);picker.closeFloater();flushIdle();assert(inserted==0);}
  {auto picker=std::make_unique<LLFloaterTasiaMediaPicker>();dispatch(*picker);picker.reset();flushIdle();assert(inserted==0);}
  {LLFloaterTasiaMediaPicker picker;dispatch(picker);picker.mNonce={"reopened-page"};flushIdle();assert(inserted==0&&picker.closed==0);}
  {LLFloaterTasiaMediaPicker picker;dispatch(picker);gAgent.session={"session-2"};flushIdle();assert(inserted==0);gAgent.session={"session-1"};}
  {LLFloaterTasiaMediaPicker picker;dispatch(picker);gDisconnected=true;flushIdle();assert(inserted==0);gDisconnected=false;}
  {LLFloaterTasiaMediaPicker picker;dispatch(picker);gSavedSettings.enabled=false;flushIdle();assert(inserted==0);gSavedSettings.enabled=true;}
  {LLFloaterTasiaMediaPicker picker;LLPluginClassMedia stale;stale.location=picker.plugin.location;
   picker.handleMediaEvent(&stale,MEDIA_EVENT_LOCATION_CHANGED);assert(idle.empty());}
 }catch(const std::exception&){return 9;}
}
'''


class Lifetime(unittest.TestCase):
    def compile_and_run(self, handler):
        with tempfile.TemporaryDirectory(prefix='media-lifetime-', dir='/tmp/opencode') as tmp:
            source=Path(tmp)/'test.cpp';exe=Path(tmp)/'test'
            source.write_text(PREFIX+'\n'+handler+'\n'+SUFFIX)
            subprocess.run(['c++','-std=c++17','-Wall','-Werror',str(source),'-o',str(exe)],check=True)
            return subprocess.run([str(exe)],capture_output=True).returncode

    def test_production_deferred_selection_and_cancellation_guards(self):
        handler=method(SOURCE.read_text(),'void LLFloaterTasiaMediaPicker::handleMediaEvent(')
        self.assertEqual(self.compile_and_run(handler),0)

    def test_regression_detector_rejects_old_inline_close_order(self):
        handler=method(SOURCE.read_text(),'void LLFloaterTasiaMediaPicker::handleMediaEvent(')
        start=handler.index('        const auto handle = getHandle();')
        end=handler.index('        });',start)+len('        });')
        unsafe=handler[:start]+'        closeFloater();\n        select(url);'+handler[end:]
        self.assertEqual(self.compile_and_run(unsafe),9)


if __name__=='__main__':
    unittest.main(verbosity=2)

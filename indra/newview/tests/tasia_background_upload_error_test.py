#!/usr/bin/env python3
"""Background bake recovery owns its errors; user uploads still show failures."""
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET
from tasia_portable_branding_test import method
from tasia_sound_globe_test import compile_run

NV = Path(__file__).resolve().parents[1]


class BackgroundUploadErrors(unittest.TestCase):
    def test_actual_handler_calls_owner_before_generic_popup(self):
        fixture = r'''
#include <cassert>
#include <map>
#include <memory>
#include <sstream>
#include <string>
std::ostringstream logs;
#define LL_WARNS(...) logs
#define LL_ENDL "\n"
struct LLSD{std::map<std::string,LLSD> fields;std::string value;
 bool has(std::string k){return fields.count(k);}LLSD& operator[](std::string k){return fields[k];}
 LLSD& operator=(std::string v){value=v;return *this;}std::string asString(){return value;}
 template<class T>LLSD with(const char*,T)const{return *this;} };
namespace LLCore{struct HttpStatus{int code;int getType(){return code;}std::string toTerseString(){return std::to_string(code);} };}
namespace LLAssetType{enum EType{AT_TEXTURE,AT_IMAGE_JPEG};}
struct LLResourceUploadInfo{using ptr_t=std::shared_ptr<LLResourceUploadInfo>;bool handled=true;int callbacks=0;LLAssetType::EType type=LLAssetType::AT_TEXTURE;
 bool failedUpload(LLSD&,std::string&){++callbacks;return handled;}auto getAssetType(){return type;}std::string getDisplayName(){return "file";} };
struct LLFloaterSnapshot{bool waiting=true;int notifications=0;static LLFloaterSnapshot* findInstance(){static LLFloaterSnapshot s;return &s;}
 bool isWaitingState(){return waiting;}void notify(LLSD){++notifications;} };
struct LLTrans{static std::string getString(const char* key){return key;}};
struct LLNotificationsUtil{static int& count(){static int n=0;return n;}static void add(std::string,LLSD){++count();}};
struct LLViewerAssetUpload{static void HandleUploadError(LLCore::HttpStatus,LLSD&,LLResourceUploadInfo::ptr_t&);};
'''
        fixture += method((NV/'llviewerassetupload.cpp').read_text(),
                          'void LLViewerAssetUpload::HandleUploadError(')
        fixture += r'''
int main(){LLSD result;result["error_body"]="PRIVATE-ERROR-BODY";
 auto upload=std::make_shared<LLResourceUploadInfo>();
 LLViewerAssetUpload::HandleUploadError({503},result,upload);
 assert(upload->callbacks==1&&LLNotificationsUtil::count()==0);
 assert(LLFloaterSnapshot::findInstance()->notifications==0);
 upload->handled=false;LLViewerAssetUpload::HandleUploadError({503},result,upload);
 assert(upload->callbacks==2&&LLNotificationsUtil::count()==1);
 assert(LLFloaterSnapshot::findInstance()->notifications==1);
 assert(logs.str().find("PRIVATE-ERROR-BODY")==std::string::npos);
}
'''
        compile_run(fixture)

    def test_http_is_tried_once_before_legacy_transfer(self):
        compile_run(r'''
#include <cassert>
#include "lltasia_bake_upload.h"
int main(){TasiaBake::UploadPolicy policy;policy.request();
 assert(policy.useHttp(true)&&!policy.useHttp(false));
 auto ticket=policy.begin();policy.complete(ticket,false);
 assert(!policy.useHttp(true)); // Do not repeat six failing HTTP uploads.
 ticket=policy.begin();policy.complete(ticket,true);
 policy.request();assert(policy.useHttp(true));
}
''')
        source = (NV/'llviewertexlayer.cpp').read_text()
        self.assertIn('mUploadPolicy.useHttp(!cap.empty())', source)
        for language in ('en','pl'):
            root = ET.parse(NV/f'skins/default/xui/{language}/notifications.xml').getroot()
            entries = root.findall("notification[@name='TasiaAppearanceUploadFailed']")
            self.assertEqual(len(entries), 1)
            self.assertIsNotNone(entries[0].find('unique'))


if __name__ == '__main__':
    unittest.main(verbosity=2)

#!/usr/bin/env python3
"""Compile production wearable fallback with fake transfer/network services."""
from pathlib import Path
import unittest
from tasia_portable_branding_test import method
from tasia_sound_globe_test import compile_run

NV = Path(__file__).resolve().parents[1]


class WearableAssetTransport(unittest.TestCase):
    def test_production_fallback_is_bounded_and_opensim_only(self):
        source = (NV/'llviewerassetstorage.cpp').read_text()
        fixture = r'''
#include <algorithm>
#include <cassert>
#include <list>
#include <sstream>
using S32=int; using F32=float; using LLUUID=int;
namespace LLAssetType { enum EType { AT_BODYPART=13,AT_CLOTHING=5,AT_TEXTURE=0,AT_MESH=49 }; }
std::ostringstream logs;
#define LL_INFOS(...) logs
#define LL_ENDL "\n"
struct LLGridManager { bool os=true; static LLGridManager* getInstance(){ static LLGridManager g;return &g;} bool isInOpenSim(){return os;} };
struct Agent { bool region=true; void* getRegion(){return region?this:nullptr;} } gAgent;
struct LLAssetRequest { int id=7; LLAssetType::EType type=LLAssetType::AT_BODYPART;
 int getUUID()const{return id;} auto getType()const{return type;} };
struct Time { int value(){return 1;} };
struct LLViewerAssetRequest:LLAssetRequest { bool mWithHTTP=true,mIsPriority=false;Time mMetricsStartTime; };
struct Host { bool ok=true;bool isOk(){return ok;} };
namespace LLViewerAssetStatsFF { int enqueued=0,dequeued=0; void record_dequeue(LLAssetType::EType,bool,bool){++dequeued;} void record_enqueue(LLAssetType::EType,bool,bool){++enqueued;} }
struct LLTransferSourceParamsAsset {void setAsset(int,LLAssetType::EType){} };
struct LLTransferTargetParamsVFile { void setAsset(int,LLAssetType::EType){} void setCallback(void(*)(),const LLViewerAssetRequest&){} };
struct Channel {int transfers=0;float priority=0;void requestTransfer(const LLTransferSourceParamsAsset&,const LLTransferTargetParamsVFile&,float p){++transfers;priority=p;} };
constexpr int LLTCT_ASSET=1;
struct Manager { Channel channel;bool available=true;Channel* getTargetChannel(Host,int){return available?&channel:nullptr;} } gTransferManager;
struct LLViewerAssetStorage { Host mUpstreamHost;std::list<LLAssetRequest*> mPendingDownloads;
 static void downloadCompleteCallback(){} bool startLegacyWearableTransfer(const LLUUID&,LLAssetType::EType); };
'''
        fixture += method(source, 'bool LLViewerAssetStorage::startLegacyWearableTransfer(')
        fixture += r'''
int main(){
 LLViewerAssetStorage storage;LLViewerAssetRequest req; storage.mPendingDownloads.push_back(&req);
 auto* grid=LLGridManager::getInstance();
 grid->os=false;assert(!storage.startLegacyWearableTransfer(7,req.type));assert(req.mWithHTTP);
 grid->os=true;gAgent.region=false;assert(!storage.startLegacyWearableTransfer(7,req.type));
 gAgent.region=true;storage.mUpstreamHost.ok=false;assert(!storage.startLegacyWearableTransfer(7,req.type));
 storage.mUpstreamHost.ok=true;assert(!storage.startLegacyWearableTransfer(99,req.type));
 req.type=LLAssetType::AT_TEXTURE;assert(!storage.startLegacyWearableTransfer(7,req.type));
 req.type=LLAssetType::AT_MESH;assert(!storage.startLegacyWearableTransfer(7,req.type));
 req.type=LLAssetType::AT_BODYPART;gTransferManager.available=false;assert(!storage.startLegacyWearableTransfer(7,req.type));assert(req.mWithHTTP);
 gTransferManager.available=true;assert(storage.startLegacyWearableTransfer(7,req.type));
 assert(!req.mWithHTTP&&gTransferManager.channel.transfers==1);
 assert(LLViewerAssetStatsFF::enqueued==1&&LLViewerAssetStatsFF::dequeued==1);
 assert(storage.startLegacyWearableTransfer(7,req.type));assert(gTransferManager.channel.transfers==1);
 req.type=LLAssetType::AT_CLOTHING;req.mWithHTTP=true;req.mIsPriority=true;
 assert(storage.startLegacyWearableTransfer(7,req.type));assert(gTransferManager.channel.transfers==2&&gTransferManager.channel.priority==101.f);
 storage.mPendingDownloads.clear();assert(!storage.startLegacyWearableTransfer(7,req.type));
}
'''
        compile_run(fixture)

    def test_capability_is_refreshed_and_failed_http_hands_off_callbacks(self):
        source = (NV/'llviewerassetstorage.cpp').read_text()
        coro = method(source, 'void LLViewerAssetStorage::assetRequestCoro(')
        self.assertNotIn('mViewerAssetUrl.empty() && gAgent.getRegion()', coro)
        self.assertIn('mViewerAssetUrl = gAgent.getRegion()->getViewerAssetUrl();', coro)
        self.assertIn('if (startLegacyWearableTransfer(uuid, atype)) return;', coro)
        self.assertIn('if (missing_route && startLegacyWearableTransfer(uuid, atype)) return;', coro)
        self.assertIn('status.getType() == 404 || status.getType() == 410', coro)
        self.assertNotIn('status.getType() == 403', coro)
        self.assertGreaterEqual(coro.count('gAgent.getSessionID() != request_session'), 3)
        self.assertGreaterEqual(coro.count('gAssetStorage != this'), 4)


if __name__ == '__main__':
    unittest.main(verbosity=2)

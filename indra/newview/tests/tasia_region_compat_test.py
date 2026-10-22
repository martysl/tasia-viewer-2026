#!/usr/bin/env python3
"""Offline tests of actual region/parcel/terrain functions with dependency stubs.

No viewer, GPU, network or Windows build. CXX can include sanitizer flags.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
view = root / "indra/newview"

region_features = (view / "llviewerregion.cpp").read_text()
world_map = (view / "llfloaterworldmap.cpp").read_text()
speakers = (view / "llspeakers.cpp").read_text()
place_info = (view / "llpanelplaceinfo.cpp").read_text()
place_panel = (view / "llpanelplaces.cpp").read_text()
url_dispatcher = (view / "llurldispatcher.cpp").read_text()
landmarks = (view / "llpanellandmarks.cpp").read_text()
profile_picks = (view / "llpanelprofilepicks.cpp").read_text()
classified = (view / "llpanelclassified.cpp").read_text()
profile_classifieds = (view / "llpanelprofileclassifieds.cpp").read_text()
group_land = (view / "llpanelgrouplandmoney.cpp").read_text()
land_holdings = (view / "llfloaterlandholdings.cpp").read_text()
place_profile = (view / "llpanelplaceprofile.cpp").read_text()
assert 'extras["MinSimHeight"]' in region_features
assert 'LLGridManager::getInstance()->isInOpenSim()' in region_features
assert "getMinSimHeight()" in world_map
assert "clampWorldMapTeleportZ" in world_map
assert "mTeleportCoordSpinZ->setMinValue" in world_map
assert 'extras["say-range"]' in region_features
assert "getSayRange()" in speakers
assert 'key["ox"] = region_origin.mdV[VX]' in url_dispatcher
assert 'key["oy"] = region_origin.mdV[VY]' in url_dispatcher
assert 'key.has("ox") && key.has("oy")' in place_panel
assert "pos_global - region_origin" in place_info
assert "getRegionFromPosGlobal(pos_global)" in place_info
assert "getRegionFromID(region_id)" in landmarks
assert "target_region->getPosRegionFromGlobal(pos_global)" in landmarks
for source in (profile_picks, classified):
    assert "getRegionFromPosGlobal(pos_global)" in source
    assert "region->getPosRegionFromGlobal(pos_global)" in source
for source in (profile_classifieds, group_land, land_holdings):
    assert "getRegionFromPosGlobal" in source
    assert "getPosRegionFromGlobal" in source
assert "mPosRegion = region->getPosRegionFromGlobal(pos_global)" in place_profile


def function(path, signature):
    text = path.read_text()
    start = text.index(signature)
    opening = text.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


# Use the real bit packer/header, replacing only its logging dependency.
bitpack = (root / "indra/llcommon/llbitpack.h").read_text().replace('#include "llerror.h"', '')
terrain = function(root / "indra/llmessage/patch_code.cpp", "void    decode_patch_header(")
overlay = function(view / "llviewerparceloverlay.cpp", "void LLViewerParcelOverlay::uncompressLandOverlay(")
methods = "\n".join(function(view / "llviewerparcelmgr.cpp", signature) for signature in (
    "bool LLViewerParcelMgr::ensureParcelCapacity(",
    "void LLViewerParcelMgr::resetAgentParcelOverlay(",
    "void LLViewerParcelMgr::resetSegments(",
    "void LLViewerParcelMgr::writeAgentParcelFromBitmap(",
    "bool LLViewerParcelMgr::inAgentParcel("))
containment = function(view / "llviewerregion.cpp", "bool LLViewerRegion::pointInRegionGlobal(")
region_local_position = function(view / "llviewerregion.cpp", "LLVector3 LLViewerRegion::getPosRegionFromGlobal(")

preamble = r'''
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstring>
#include <iostream>
#include <memory>
#include <limits>
#include <vector>
#include "stdtypes.h"
#include "patch_dct.h"
#include "lltasia_region_compat.h"
#define LL_WARNS(...) std::clog
#define LL_INFOS_ONCE(...) std::clog
#define LL_ERRS(...) std::clog
#define LL_ENDL std::endl
constexpr F32 PARCEL_GRID_STEP_METERS = 4.f;
constexpr int PARCEL_OVERLAY_CHUNKS = 4;
int gWordBits = 0;
enum { VX, VY, VZ };
struct LLVector3d {
    double x,y,z;
    LLVector3d operator-(const LLVector3d& other) const { return {x-other.x,y-other.y,z-other.z}; }
};
struct LLVector3 {
    F32 mV[3];
    void setVec(const LLVector3d& p) { mV[0]=F32(p.x); mV[1]=F32(p.y); mV[2]=F32(p.z); }
};
struct RegionImpl { LLVector3d mOriginGlobal; };
struct LLViewerRegion {
    F32 mWidth;
    RegionImpl storage;
    RegionImpl* mImpl;
    LLViewerRegion(F32 width, LLVector3d origin={0,0,0}) : mWidth(width), storage{origin}, mImpl(&storage) {}
    LLVector3 getPosRegionFromGlobal(const LLVector3d& p) const;
    bool pointInRegionGlobal(const LLVector3d&) const;
};
LLViewerRegion active_region{512};
struct LLWorld {
    static LLWorld* getInstance() { static LLWorld w; return &w; }
    LLViewerRegion* getRegionFromPosGlobal(const LLVector3d& p) {
        return active_region.pointInRegionGlobal(p) ? &active_region : nullptr;
    }
};
struct Agent { LLViewerRegion* getRegion() const { return &active_region; } } gAgent;
struct LLViewerParcelOverlay {
    int mParcelGridsPerEdge, dirty = 0;
    U8* mOwnership;
    void setDirty() { ++dirty; }
    void uncompressLandOverlay(S32, U8*);
};
struct LLViewerParcelMgr {
    S32 mParcelsPerEdge = 64, mParcelBufferGridsPerEdge = 64;
    S32 mAgentParcelGridsPerEdge = 64, mHighlightGridsPerEdge = 64, mCollisionGridsPerEdge = 64;
    S32 mAgentParcelSequenceID = 99;
    bool mRenderCollision = true;
    U8* mHighlightSegments = new U8[65*65]();
    U8* mCollisionSegments = new U8[65*65]();
    U8* mAgentParcelOverlay = new U8[64*64]();
    ~LLViewerParcelMgr() { delete[] mHighlightSegments; delete[] mCollisionSegments; delete[] mAgentParcelOverlay; }
    bool ensureParcelCapacity(S32);
    void resetAgentParcelOverlay(F32);
    void resetSegments(U8*);
    void writeAgentParcelFromBitmap(U8*);
    bool inAgentParcel(const LLVector3d&) const;
};
'''
tests = r'''
int main() {
    using namespace LLTasiaRegion;
    assert(safeMinSimHeight(true, -100.f) == -100.f);
    assert(safeMinSimHeight(false, -100.f) == 0.f);
    assert(safeMinSimHeight(true, 10.f) == 0.f);
    assert(safeMinSimHeight(true, std::numeric_limits<float>::infinity()) == 0.f);
    assert(safeMinSimHeight(true, std::numeric_limits<float>::quiet_NaN()) == 0.f);
    assert(clampWorldMapTeleportZ(true, -100.f, -25.f) == -25.f);
    assert(clampWorldMapTeleportZ(true, -100.f, -200.f) == -100.f);
    assert(clampWorldMapTeleportZ(false, -100.f, -25.f) == 0.f);
    assert(clampWorldMapTeleportZ(true, -100.f, 5000.f) == 4096.f);
    assert(clampWorldMapTeleportZ(true, -100.f, std::numeric_limits<float>::quiet_NaN()) == -100.f);
    assert(safeSayRange(true, 60.f) == 60.f);
    assert(safeSayRange(true, 0.0) == 0.f);
    assert(safeSayRange(false, 60.f) == 20.f);
    assert(safeSayRange(true, -1.f) == 20.f);
    assert(safeSayRange(true, std::numeric_limits<float>::infinity()) == 20.f);
    assert(safeSayRange(true, std::numeric_limits<double>::max()) == 20.f);
    assert(safeSayRange(true, static_cast<double>(std::numeric_limits<std::int32_t>::max()) + 1.0) == 20.f);
    for (U32 size : {16U, 256U, 512U, 1024U, 4096U}) {
        assert(validMapDimension(size));
        assert(safeMapDimension(size) == size);
        assert(validMapDimensions(size, 256U));
    }
    for (U32 size : {0U, 1U, 15U, 17U, 255U, 4112U, UINT_MAX, 8192U}) {
        assert(!validMapDimension(size));
        assert(safeMapDimension(size) == DEFAULT_WIDTH);
        assert(!validMapDimensions(size, 256U));
    }
    for (U32 width : {256U,512U,1024U,2048U,4096U}) {
        assert(supportedDimensions(width,width));
        assert(regionWidth(true,width,width) == width);
        assert(regionWidth(false,width,width) == 256); // Agni AND Aditi stay SL-sized
        const size_t cells = size_t(width/4)*(width/4);
        std::vector<U8> memory(cells+64, 0xcd);
        LLViewerParcelOverlay map{S32(width/4),0,memory.data()+32};
        std::fill(map.mOwnership,map.mOwnership+cells,0);
        U8 data[1024];
        const int count = int(cells/1024);
        for (int sequence=count-1; sequence>=0; --sequence) {
            std::fill(std::begin(data),std::end(data),U8(sequence%251+1));
            assert(overlayChunkFits(cells,sequence,sizeof(data)));
            map.uncompressLandOverlay(sequence,data); // out-of-order delivery
        }
        assert(map.dirty==count);
        for (int sequence=0; sequence<count; ++sequence)
            for (int byte=0; byte<1024; ++byte)
                assert(map.mOwnership[sequence*1024+byte] == U8(sequence%251+1));
        auto before=memory;
        for (int sequence : {-1,count,INT_MAX,INT_MIN}) {
            assert(!overlayChunkFits(cells,sequence,sizeof(data)));
            map.uncompressLandOverlay(sequence,data);
        }
        map.uncompressLandOverlay(0,nullptr);
        assert(memory==before && map.dirty==count);
        assert(std::all_of(memory.begin(),memory.begin()+32,[](U8 b){return b==0xcd;}));
        assert(std::all_of(memory.end()-32,memory.end(),[](U8 b){return b==0xcd;}));
        for (size_t bytes : {0U,1U,1023U,1025U}) assert(!overlayChunkFits(cells,0,bytes));
    }
    for (auto dims : {std::pair<U32,U32>{0,0},{512,0},{512,256},{768,768},{8192,8192},{UINT_MAX,UINT_MAX}}) {
        assert(!supportedDimensions(dims.first,dims.second));
        assert(regionWidth(true,dims.first,dims.second)==256);
    }
    assert(regionsAdjacent(0,0,512,512,0,256));
    assert(regionsAdjacent(0,0,512,512,256,256));
    assert(regionsAdjacent(0,0,512,512,512,256)); // corner-only contact
    assert(regionsAdjacent(-512,-512,512,0,-512,256)); // negative global coordinates
    assert(!regionsAdjacent(0,0,512,513,0,256));  // gap
    assert(!regionsAdjacent(0,0,512,256,0,256));  // area overlap
    assert(!regionsAdjacent(0,0,0,0,0,256));      // invalid extent
    assert(!regionsAdjacent(0,0,-256,0,0,256));   // negative width
    assert(!regionsAdjacent(0,0,512,256,256,512)); // partial overlap without shared boundary
    U32 large_first=99, small_first=99, patch_count=99;
    assert(sharedPatchSpan(0,512,0,256,16,large_first,small_first,patch_count));
    assert(large_first==0 && small_first==0 && patch_count==16);
    assert(sharedPatchSpan(0,512,256,512,16,large_first,small_first,patch_count));
    assert(large_first==16 && small_first==0 && patch_count==16);
    assert(!sharedPatchSpan(0,512,257,513,16,large_first,small_first,patch_count));
    assert(!sharedPatchSpan(0,512,512,768,16,large_first,small_first,patch_count));
    LLViewerRegion standard{256}, large{512};
    assert(!standard.pointInRegionGlobal({300,300,25}));
    assert(large.pointInRegionGlobal({300,300,25}));
    assert(!large.pointInRegionGlobal({512,0,25}));
    assert(!large.pointInRegionGlobal({-1,0,25}));
    LLViewerRegion offset_region{512, {1024,2048,0}};
    assert(offset_region.pointInRegionGlobal({1324,2248,30}));
    const LLVector3 local_position = offset_region.getPosRegionFromGlobal({1324,2248,30});
    assert(local_position.mV[0] == 300.f && local_position.mV[1] == 200.f && local_position.mV[2] == 30.f);

    LLViewerParcelMgr manager;
    manager.mAgentParcelOverlay[0]=17;
    assert(manager.ensureParcelCapacity(128));
    assert(manager.mAgentParcelOverlay[0]==17); // growing does not destroy existing maps
    manager.resetAgentParcelOverlay(512);
    assert(manager.mAgentParcelSequenceID==-1 && !manager.mRenderCollision);
    std::vector<U8> bitmap(128*128/8,0xff);
    manager.writeAgentParcelFromBitmap(bitmap.data());
    assert(manager.inAgentParcel({300,300,25}));
    manager.mParcelsPerEdge=64; // an unrelated 256m region's parcel reply
    manager.resetSegments(manager.mHighlightSegments);
    assert(manager.mHighlightGridsPerEdge==64 && manager.mAgentParcelGridsPerEdge==128);
    assert(manager.inAgentParcel({300,300,25})); // agent bitmap retains its own stride
    manager.mParcelsPerEdge=128;
    manager.resetSegments(manager.mCollisionSegments);
    assert(manager.mCollisionGridsPerEdge==128);
    assert(!manager.ensureParcelCapacity(-1) && !manager.ensureParcelCapacity(INT_MAX));
    assert(!manager.ensureParcelCapacity(65) && !manager.ensureParcelCapacity(127));
    manager.resetAgentParcelOverlay(256);
    assert(!manager.inAgentParcel({300,300,25}));

    for (bool extended : {false,true}) {
        for (U32 id : (extended ? std::vector<U32>{0,0x001f001f,0x012c00c8,0x01ff01ff}
                                : std::vector<U32>{0,1,0x3ff})) {
            U8 buffer[64]={};
            LLBitPack pack(buffer,sizeof(buffer));
            U8 quant=0x66, end=END_OF_PATCHES;
            F32 offset=12.5f; U16 range=42;
            pack.bitPack(&quant,8);
            pack.bitPack(reinterpret_cast<U8*>(&offset),32);
            pack.bitPack(reinterpret_cast<U8*>(&range),16);
            pack.bitPack(reinterpret_cast<U8*>(&id),extended?32:10);
            pack.bitPack(&end,8);
            const U32 size=pack.flushBitPack();
            LLBitPack unpack(buffer,size);
            LLPatchHeader header{};
            decode_patch_header(unpack,&header,extended);
            assert(header.quant_wbits==quant && header.dc_offset==offset && header.range==range);
            assert(header.patchids==id && gWordBits==8);
            // A following end marker proves the decoder consumed 32 vs 10 bits
            // correctly, without shifting the remainder of the packet.
            decode_patch_header(unpack,&header,extended);
            assert(header.quant_wbits==END_OF_PATCHES && header.patchids==0);
        }
    }
    std::cout << "PASS: SL size isolation; bounded256..4096 regions; all overlay chunks/canaries; "
                 "malformed sequences; parcel buffer growth/independent strides; "
                 "actual legacy10-bit and extended32-bit terrain headers\n";
}
'''

# Integration invariants not linked in the isolated harness.
world=(view/"llworld.cpp").read_text()
assert "region_width * mScale" in world and "mWidth = region_size_x" not in world
assert "neighborp->getWidth() == regionp->getWidth()" in world
assert "LLTasiaRegion::sharedPatchSpan" in world
assert "connectNeighborEdge" in world and "connectNeighborCorner" in world
surface_source=(view/"llsurface.cpp").read_text()
assert "mNeighborConnections" in surface_source
assert "this_first_patch" in surface_source and "neighbor_first_patch" in surface_source
region_source=(view/"llviewerregion.cpp").read_text()
assert "LLTasiaRegion::regionsAdjacent" in region_source
reachability=function(view/"llviewerobject.cpp", "bool LLViewerObject::isReachable(")
assert "getAdjacentRegions(neighbors)" in reachability
startup=(view/"llstartup.cpp").read_text()
assert 'response["region_size_x"]' in startup and 'response["region_size_y"]' in startup
events=(view/"llviewermessage.cpp").read_text()
for block in ("Info","RegionData"):
    assert f'getSize("{block}", "RegionSizeX")' in events
    assert f'getSize("{block}", "RegionSizeY")' in events
message_config=(view/"app_settings/message.xml").read_text()
for message_name in ("EnableSimulator", "TeleportFinish", "CrossedRegion"):
    message_block=message_config.split(f'<key>{message_name}</key>',1)[1].split('</map>',1)[0]
    assert '<string>llsd</string>' in message_block
assert 'getSize("SimulatorInfo", "RegionSizeX")' in world
assert 'getSize("SimulatorInfo", "RegionSizeY")' in world
mgr=(view/"llviewerparcelmgr.cpp").read_text()
assert "bitmap_size = size;" not in mgr
assert "mAgentParcelGridsPerEdge + column" in mgr
assert "b_large_patch ? ph.patchids >> 16" in (view/"llsurface.cpp").read_text()
assert "OPENSIM_LAND_LAYER_CODE" in (view/"llvlmanager.cpp").read_text()

with tempfile.TemporaryDirectory(prefix="tasia-regions-") as directory:
    source=Path(directory)/"regions.cpp"
    binary=Path(directory)/"regions"
    source.write_text(preamble+bitpack+terrain+containment+region_local_position+overlay+methods+tests)
    subprocess.run(shlex.split(os.environ.get("CXX","c++"))+[
        "-std=c++17","-Wall","-Wextra","-Werror",
        "-I"+str(view),"-I"+str(root/"indra/llcommon"),"-I"+str(root/"indra/llmessage"),
        str(source),"-o",str(binary)],check=True)
    result=subprocess.run([str(binary)],text=True,capture_output=True)
    if result.returncode:
        raise RuntimeError(result.stdout+result.stderr)
    print(result.stdout,end="")

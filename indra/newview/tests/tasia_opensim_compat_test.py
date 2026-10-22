#!/usr/bin/env python3
"""Compile actual compatibility functions with minimal viewer dependency stubs.

Run with Python 3 and a C++17 compiler (CXX, default c++). This is NOT a full
viewer, packet decoder, GPU, bake upload or interactive login test.
"""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile


def function(text, signature):
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


root = Path(__file__).resolve().parents[1]
avatar = (root / "llvoavatar.cpp").read_text()
grid = (root / "llviewernetwork.h").read_text()
wearables = (root / "llagentwearables.cpp").read_text()

# Unmodified production function bodies, not reimplementations of the policy.
grid_functions = "\n".join(function(grid, sig) for sig in (
    "bool isInSecondLife() const", "bool isInOpenSim() const"))
functions = "\n".join(function(avatar, sig) for sig in (
    "bool resolve_appearance_version(",
    "void LLVOAvatar::processAvatarAppearance(",
    "LLViewerFetchedTexture *LLVOAvatar::getBakedTextureImage("))
dummy = function(wearables, "void LLAgentWearables::sendDummyAgentWearablesUpdate()")

preamble = r'''
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
using S32 = int;
using U8 = unsigned char;
using LLUUID = int;
using LLHost = int;
template<class T> using LLPointer = std::shared_ptr<T>;
#define MAINGRID "util.agni.lindenlab.com"
#define BETAGRID "util.aditi.lindenlab.com"
#define LL_WARNS(...) std::clog
#define LL_INFOS(...) std::clog
#define LL_INFOS_ONCE(...) std::clog
#define LL_DEBUGS(...) std::clog
#define LL_ENDL std::endl
#define llassert(value) assert(value)
int gSavedSettings;
template<class T> struct LLCachedControl {
    LLCachedControl(int, const char*) {}
    operator bool() const { return false; }
};
struct LLGridManager {
    std::string mGrid;
    static LLGridManager* getInstance() { static LLGridManager g; return &g; }
    GRID_FUNCTIONS
};
struct LLAppearanceMessageContents {
    int mAppearanceVersion = -1, mParamAppearanceVersion = -1, mCOFVersion = -1;
    std::vector<float> mParamWeights{0.f, 1.f};
};
struct LLAppearanceMgr {
    static LLAppearanceMgr& instance() { static LLAppearanceMgr a; return a; }
    int getCOFVersion() { return 5; }
};
struct LLMessageSystem {};
struct Timer { void reset() {} };
constexpr int IMG_DEFAULT_AVATAR = 1, IMG_DEFAULT = 2, IMG_INVISIBLE = 3;
constexpr int TEX_LIST_STANDARD = 0, FTT_HOST_BAKE = 1, FTT_SERVER_BAKE = 2;
struct LLDXTexture { enum { BOOST_NONE }; };
struct LLViewerTexture { enum { LOD_TEXTURE }; };
struct LLViewerFetchedTexture {
    bool missing = false;
    bool isMissingAsset() const { return missing; }
    void setIsMissingAsset(bool v) { missing = v; }
};
LLViewerFetchedTexture host_texture, url_texture, default_texture;
struct TextureList {
    LLViewerFetchedTexture* findImage(int, int) { return &default_texture; }
} gTextureList;
struct LLViewerTextureManager {
    static inline int host_calls = 0, url_calls = 0;
    static LLViewerFetchedTexture* getFetchedTexture(int, int type, bool, int, int, int, int, int host) {
        assert(type == FTT_HOST_BAKE && host == 42);
        ++host_calls; return &host_texture;
    }
    static LLViewerFetchedTexture* getFetchedTextureFromUrl(const std::string& url, int type, bool, int, int, int, int, int) {
        assert(!url.empty() && type == FTT_SERVER_BAKE);
        ++url_calls; return &url_texture;
    }
};
struct LLVOAvatar {
    bool self = true, editing = false;
    bool mUseServerBakes = LLGridManager::getInstance()->isInSecondLife();
    int mLastUpdateReceivedCOFVersion = -1, applied = 0, url_lookups = 0;
    LLAppearanceMessageContents incoming;
    LLPointer<LLAppearanceMessageContents> mLastProcessedAppearance;
    Timer mLastAppearanceMessageTimer;
    enum { AOA_NORMAL };
    bool isSelf() const { return self; }
    bool isEditingAppearance() const { return editing; }
    std::string getDebugName() const { return "synthetic"; }
    std::string avString() const { return "synthetic"; }
    int getID() const { return 123; }
    int getOverallAppearance() const { return AOA_NORMAL; }
    void resetSkeleton(bool) {}
    void parseAppearanceMessage(LLMessageSystem*, LLAppearanceMessageContents& c) { c = incoming; }
    void dumpAppearanceMsgParams(const std::string&, LLAppearanceMessageContents&) {}
    void applyParsedAppearanceMessage(LLAppearanceMessageContents&, bool) { ++applied; }
    LLHost getObjectHost() const { return 42; }
    std::string getImageURL(U8, int) { ++url_lookups; return "https://synthetic.invalid/bake"; }
    void processAvatarAppearance(LLMessageSystem*);
    LLViewerFetchedTexture* getBakedTextureImage(U8, const LLUUID&);
};
constexpr int _PREHASH_AgentIsNowWearing = 1, _PREHASH_AgentData = 2,
    _PREHASH_AgentID = 3, _PREHASH_SessionID = 4, _PREHASH_WearableData = 5,
    _PREHASH_WearableType = 6, _PREHASH_ItemID = 7;
// Real dummy UUID strings are accepted by this logging stub, never sent.
struct MessageStub {
    int messages = 0, items = 0;
    void newMessageFast(int) { ++messages; }
    void nextBlockFast(int) {}
    template<class T> void addUUIDFast(int field, T) { if (field == _PREHASH_ItemID) ++items; }
    void addU8Fast(int, U8) {}
} message_stub;
auto* gMessageSystem = &message_stub;
struct AgentStub {
    int sent = 0;
    int getID() { return 0; }
    int getSessionID() { return 0; }
    void sendReliableMessage() { ++sent; }
} gAgent;
struct LLAgentWearables { void sendDummyAgentWearablesUpdate(); };
'''.replace("GRID_FUNCTIONS", grid_functions)

# Only the UUID constructor representation differs in the minimal stub.
# All branch, message-building and sending logic remains the production body.
dummy = dummy.replace('LLUUID("', 'std::string("')

tests = r'''
int main() {
    auto* grid = LLGridManager::getInstance();
    for (const auto* name : {MAINGRID, BETAGRID}) {
        grid->mGrid = name;
        assert(grid->isInSecondLife() && !grid->isInOpenSim());
        int version;
        LLAppearanceMessageContents c;
        assert(resolve_appearance_version(c, version) && version == 1);
        LLVOAvatar av;
        av.incoming.mAppearanceVersion = av.incoming.mParamAppearanceVersion = 1;
        av.incoming.mCOFVersion = 5;
        av.processAvatarAppearance(nullptr);
        assert(av.applied == 1 && av.mLastUpdateReceivedCOFVersion == 5 && av.mUseServerBakes);
        av.processAvatarAppearance(nullptr); // duplicate must still be dropped on SL
        assert(av.applied == 1);
        av.incoming.mCOFVersion = 4;
        av.processAvatarAppearance(nullptr); // rollback must still be dropped on SL
        assert(av.applied == 1);
        av.incoming.mCOFVersion = 6;
        av.processAvatarAppearance(nullptr);
        assert(av.applied == 2);
        av.mUseServerBakes = false; // even stale mode state cannot route SL to host
        assert(av.getBakedTextureImage(0, 123) == &url_texture);
        assert(av.url_lookups == 1);
        LLAgentWearables{}.sendDummyAgentWearablesUpdate();
    }
    assert(gAgent.sent == 2 && message_stub.messages == 2 && message_stub.items == 8);
    for (const auto* name : {"igrid.example", "other-grid.example", "localhost:9000"}) {
        grid->mGrid = name;
        assert(grid->isInOpenSim() && !grid->isInSecondLife());
        int version;
        LLAppearanceMessageContents c;
        assert(resolve_appearance_version(c, version) && version == 0);
        c.mAppearanceVersion = 0;
        assert(resolve_appearance_version(c, version) && version == 0);
        c.mAppearanceVersion = 1; c.mParamAppearanceVersion = 0;
        assert(!resolve_appearance_version(c, version)); // conflicting versions stay rejected
        LLVOAvatar av;
        assert(!av.mUseServerBakes);
        av.processAvatarAppearance(nullptr); // i-Grid: absent version/COF, last COF -1
        assert(av.applied == 1 && av.mLastUpdateReceivedCOFVersion == -1 && !av.mUseServerBakes);
        av.processAvatarAppearance(nullptr);
        assert(av.applied == 2);
        int calls = LLViewerTextureManager::url_calls;
        assert(av.getBakedTextureImage(0, 123) == &host_texture);
        assert(av.url_lookups == 0 && LLViewerTextureManager::url_calls == calls);
        assert(av.getBakedTextureImage(0, IMG_DEFAULT_AVATAR) == &default_texture);
        av.editing = true;
        av.processAvatarAppearance(nullptr);
        assert(av.applied == 2);
        av.editing = false;
        av.incoming.mParamWeights.clear();
        av.processAvatarAppearance(nullptr);
        assert(av.applied == 2); // empty appearance still rejected
        av.incoming.mParamWeights = {0.f, 1.f};
        av.incoming.mAppearanceVersion = av.incoming.mParamAppearanceVersion = 1;
        av.incoming.mCOFVersion = 7;
        av.processAvatarAppearance(nullptr);
        assert(av.applied == 3 && av.mUseServerBakes && av.mLastUpdateReceivedCOFVersion == 7);
        av.processAvatarAppearance(nullptr);
        assert(av.applied == 3); // OpenSim's explicit server-bake mode keeps COF protection
        assert(av.getBakedTextureImage(0, 123) == &url_texture);
        av.incoming = LLAppearanceMessageContents{};
        av.processAvatarAppearance(nullptr); // switch back without rolling COF backwards
        assert(av.applied == 4 && !av.mUseServerBakes && av.mLastUpdateReceivedCOFVersion == 7);
        LLVOAvatar other;
        other.self = false;
        other.processAvatarAppearance(nullptr);
        assert(other.applied == 1 && !other.mUseServerBakes);
        LLVOAvatar unsupported;
        unsupported.incoming.mAppearanceVersion = unsupported.incoming.mParamAppearanceVersion = 2;
        unsupported.processAvatarAppearance(nullptr);
        assert(unsupported.applied == 0);
        LLVOAvatar explicit_legacy;
        explicit_legacy.incoming.mAppearanceVersion = 0;
        explicit_legacy.processAvatarAppearance(nullptr);
        assert(explicit_legacy.applied == 1 && !explicit_legacy.mUseServerBakes);
        LLAgentWearables{}.sendDummyAgentWearablesUpdate();
    }
    assert(gAgent.sent == 2 && message_stub.messages == 2 && message_stub.items == 8);
    assert(LLViewerTextureManager::host_calls == 3);
    std::cout << "PASS: Agni/Aditi isolation, OpenSim legacy/self/other appearance, COF guards, "
                 "exclusive host/URL texture routing, dummy-wearables suppression\n";
}
'''

# Guards for integration points not linked by this isolated harness.
region = (root / "llviewerregion.cpp").read_text()
assert "mCentralBakeVersion = region_protocols & 1;" in region
assert "mCentralBakeVersion = 1;" not in function(region, "void LLViewerRegion::unpackRegionHandshake()")
startup = (root / "llstartup.cpp").read_text()
assert "if (!succ && LLGridManager::getInstance()->isInOpenSim())" in startup
assert "if (LLGridManager::getInstance()->isInOpenSim() && !mUseServerBakes)" in function(
    avatar, "const std::string LLVOAvatar::getImageURL(")
assert "mUseServerBakes(LLGridManager::getInstance()->isInSecondLife())" in avatar

with tempfile.TemporaryDirectory(prefix="tasia-opensim-") as directory:
    source = Path(directory) / "compat.cpp"
    binary = Path(directory) / "compat"
    source.write_text(preamble + functions + dummy + tests)
    subprocess.run(shlex.split(os.environ.get("CXX", "c++")) + [
        "-std=c++17", "-Wall", "-Wextra", "-Werror", str(source), "-o", str(binary)
    ], check=True)
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    print(result.stdout, end="")

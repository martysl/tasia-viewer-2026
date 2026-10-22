#!/usr/bin/env python3
"""Execute production upload policy and legacy appearance sender with fixtures.

No real network, sessions, avatar data or GPU. This does not prove live baking.
"""
import subprocess
import tempfile
from pathlib import Path

indra = Path(__file__).resolve().parents[2]
source = (indra / 'newview/llvoavatarself.cpp').read_text()


def function(signature):
    start = source.index(signature)
    opening = source.index('{', start)
    end, depth = opening + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


fixture = r'''
#include "lltasia_bake_upload.h"
#include <cassert>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <mutex>
#include <thread>
struct LLUUID {
 int value=0;
 bool notNull() const { return value!=0; }
 static const LLUUID null;
};
const LLUUID LLUUID::null{};
using U8=std::uint8_t; using U32=std::uint32_t; using F32=float;
enum { BAKED_HEAD, BAKED_UPPER, BAKED_LOWER, BAKED_EYES, BAKED_SKIRT, BAKED_HAIR };
enum { VISUAL_PARAM_GROUP_TWEAKABLE, VISUAL_PARAM_GROUP_TRANSMIT_NOT_TWEAKABLE,
       VISUAL_PARAM_GROUP_TWEAKABLE_NO_TRANSMIT, MAX_TRANSMITTED_VISUAL_PARAMS=255 };
namespace LLWearableType { enum EType { WT_SKIRT }; }
using EBakedTextureIndex=int; using ETextureIndex=int;
struct LLVisualParam {
 int group=VISUAL_PARAM_GROUP_TWEAKABLE; float weight=1.f;
 int getGroup() const { return group; }
 float getWeight() const { return weight; }
 float getMinWeight() const { return 0.f; }
 float getMaxWeight() const { return 1.f; }
};
U8 F32_to_U8(F32 value, F32 low, F32 high) {
 return static_cast<U8>(std::clamp((value-low)/(high-low),0.f,1.f)*255.f);
}
struct Region { int bake=0; int getCentralBakeVersion() const { return bake; } } region, other;
struct LLGridManager {
 bool opensim=true;
 static LLGridManager* getInstance() { static LLGridManager manager; return &manager; }
 bool isInOpenSim() const { return opensim; }
};
const char* _PREHASH_AgentSetAppearance="AgentSetAppearance";
const char* _PREHASH_AgentData="AgentData";
const char* _PREHASH_AgentID="AgentID";
const char* _PREHASH_SessionID="SessionID";
const char* _PREHASH_Size="Size";
const char* _PREHASH_SerialNum="SerialNum";
const char* _PREHASH_ObjectData="ObjectData";
const char* _PREHASH_VisualParam="VisualParam";
const char* _PREHASH_ParamValue="ParamValue";
struct LLMessageSystem {
 std::string name; std::vector<std::string> blocks; std::vector<U8> weights;
 U32 serial=0; int size=0, uuid_fields=0;
 void newMessageFast(const char* value) { *this=LLMessageSystem{}; name=value; }
 void nextBlockFast(const char* value) { blocks.emplace_back(value); }
 void addUUIDFast(const char*, int) { ++uuid_fields; }
 void addVector3Fast(const char*, int value) { size=value; }
 void addU32Fast(const char*, U32 value) { serial=value; }
 void addU8Fast(const char*, U8 value) { weights.push_back(value); }
} message;
LLMessageSystem* gMessageSystem=&message;
struct Agent {
 Region* region_ptr=&region; int sends=0;
 Region* getRegion() const { return region_ptr; }
 int getID() const { return 1; }
 int getSessionID() const { return 2; }
 void sendReliableMessage() { ++sends; }
} gAgent;
struct { bool loaded=true; bool areWearablesLoaded() const { return loaded; } } gAgentWearables;
struct Dictionary { int bakedToLocalTextureIndex(int i) const { return i; } } dictionary;
struct LLVOAvatarSelf {
 Region* region_ptr=&region;
 bool pending=false, editing=false, skirt=false, pack_ok=true;
 bool defined[6]={true,true,true,true,false,true};
 bool mUseLocalAppearance=true;
 U32 mLegacyAppearanceSerialNum=0;
 int mBodySize=42, mesh_updates=0, te_packs=0;
 std::vector<LLVisualParam> params = std::vector<LLVisualParam>(253);
 std::size_t cursor=0;
 Region* getRegion() const { return region_ptr; }
 bool usesLegacyBakes() const;
 int getNumBakes() const { return 6; }
 bool hasPendingBakedUploads() const { return pending; }
 bool isEditingAppearance() const { return editing; }
 bool isWearingWearableType(LLWearableType::EType) const { return skirt; }
 Dictionary* getDictionary() const { return &dictionary; }
 bool isTextureDefined(int i, int) const { return defined[i]; }
 LLVisualParam* getFirstVisualParam() { cursor=0; return params.empty()?nullptr:&params[0]; }
 LLVisualParam* getNextVisualParam() { return ++cursor<params.size()?&params[cursor]:nullptr; }
 bool sendAppearanceMessage(LLMessageSystem*) { ++te_packs; return pack_ok; }
 void updateMeshTextures() { ++mesh_updates; }
 void sendLegacyAppearance();
};
std::shared_ptr<LLVOAvatarSelf> gAgentAvatarp;
bool isAgentAvatarValid() { return bool(gAgentAvatarp); }
std::ostringstream diagnostic;
#define LL_INFOS_ONCE(category) diagnostic
#define LL_ENDL "\n"
'''
tests = r'''
int main() {
 using Policy=TasiaBake::UploadPolicy;
 using Result=Policy::Result;
 Policy policy;
 assert(!policy.pending() && policy.begin()==0);
 policy.request(); auto first=policy.begin();
 assert(first && policy.inFlight() && policy.begin()==0);
 policy.request(); // Outfit changed while first upload is in flight.
 assert(policy.complete(first,true)==Result::Obsolete);
 assert(policy.needed() && !policy.inFlight());
 auto second=policy.begin();
 assert(policy.complete(first,true)==Result::Ignored);
 assert(policy.complete(second,true)==Result::Accepted && !policy.pending());
 policy.request(); first=policy.begin(); policy.cancel();
 assert(policy.complete(first,true)==Result::Ignored && !policy.pending());
 policy.request();
 for (unsigned attempt=1; attempt<=Policy::MAX_ATTEMPTS; ++attempt) {
  auto ticket=policy.begin(); assert(ticket);
  const auto outcome=policy.complete(ticket,false);
  assert(policy.failures()==attempt && policy.retryDelay()==(2u<<(attempt-1)));
  assert(outcome==(attempt<Policy::MAX_ATTEMPTS?Result::Retry:Result::Exhausted));
 }
 assert(!policy.pending() && policy.begin()==0);
 policy.request(); assert(policy.failures()==0 && policy.retryDelay()==0);

 // Exercise the actual callback mailbox after the owning composite releases
 // its reference; a late response must not touch a destroyed avatar/layerset.
 auto mailbox=std::make_shared<LLTasiaBakeCompletion>(17);
 std::weak_ptr<LLTasiaBakeCompletion> weak=mailbox;
 auto callback=mailbox; mailbox.reset();
 bool ok=false; LLUUID asset;
 assert(!callback->poll(ok,asset));
 callback->complete(true, LLUUID{9});
 std::vector<std::thread> late;
 for (int i=0; i<8; ++i)
  late.emplace_back([callback] { callback->complete(false); });
 for (auto& thread:late) thread.join();
 assert(callback->poll(ok,asset) && ok && asset.value==9);
 assert(callback->ticket==17 && !weak.expired());
 late.clear(); callback.reset(); assert(weak.expired());

 // Execute the production fully-hidden-alpha branch. It must resolve to the
 // invisible texture, while a visible layer continues to normal encoding.
 LLViewerTexLayerSetBuffer alpha_buffer;
 alpha_buffer.mUploadCompletion=std::make_shared<LLTasiaBakeCompletion>(18);
 assert(!alpha_buffer.completeInvisibleBake());
 assert(!alpha_buffer.mUploadCompletion->poll(ok,asset));
 alpha_buffer.layer.visible=false;
 assert(alpha_buffer.completeInvisibleBake());
 assert(alpha_buffer.mUploadCompletion->poll(ok,asset) && ok && asset.value==99);

 gAgentAvatarp=std::make_shared<LLVOAvatarSelf>();
 auto& self=*gAgentAvatarp;
 self.sendLegacyAppearance();
 assert(gAgent.sends==1 && message.name=="AgentSetAppearance");
 assert(message.serial==1 && message.uuid_fields==2 && message.size==42);
 assert(message.blocks[0]=="AgentData" && message.blocks[1]=="ObjectData");
 assert(message.weights.size()==253 && message.weights[0]==255);
 assert(!self.mUseLocalAppearance && self.mesh_updates==1 && self.te_packs==1);
 self.sendLegacyAppearance(); assert(message.serial==2);
 const int sent=gAgent.sends;
 LLGridManager::getInstance()->opensim=false;
 self.sendLegacyAppearance(); assert(gAgent.sends==sent); // SL isolation.
 LLGridManager::getInstance()->opensim=true;
 region.bake=1; self.sendLegacyAppearance(); assert(gAgent.sends==sent);
 region.bake=0; self.region_ptr=nullptr; self.sendLegacyAppearance(); assert(gAgent.sends==sent);
 self.region_ptr=&other; self.sendLegacyAppearance(); assert(gAgent.sends==sent);
 self.region_ptr=&region; self.pending=true; self.sendLegacyAppearance(); assert(gAgent.sends==sent);
 self.pending=false; self.editing=true; self.sendLegacyAppearance(); assert(gAgent.sends==sent);
 self.editing=false; gAgentWearables.loaded=false; self.sendLegacyAppearance(); assert(gAgent.sends==sent);
 gAgentWearables.loaded=true; self.defined[BAKED_HEAD]=false;
 self.sendLegacyAppearance(); assert(gAgent.sends==sent);
 self.defined[BAKED_HEAD]=true; self.skirt=true; self.sendLegacyAppearance(); assert(gAgent.sends==sent);
 self.skirt=false; self.params.resize(256); self.sendLegacyAppearance(); assert(gAgent.sends==sent);
 self.params.clear(); self.sendLegacyAppearance(); assert(gAgent.sends==sent);
 self.params.resize(253); self.params.back().group=VISUAL_PARAM_GROUP_TWEAKABLE_NO_TRANSMIT;
 self.mLegacyAppearanceSerialNum=0xffffffffu;
 self.sendLegacyAppearance(); assert(gAgent.sends==sent+1 && message.serial==1);
 assert(message.weights.size()==252);
 self.pack_ok=false; self.sendLegacyAppearance(); assert(gAgent.sends==sent+1);
}
'''
with tempfile.TemporaryDirectory(prefix='bake-upload-', dir='/tmp/opencode') as tmp:
    tmp = Path(tmp)
    (tmp / 'lltasia_bake_upload.h').write_bytes((indra / 'newview/lltasia_bake_upload.h').read_bytes())
    upload_source = (indra / 'newview/llviewertexlayer.cpp').read_text()
    start = upload_source.index('struct LLTasiaBakeCompletion\n')
    opening = upload_source.index('{', start)
    end, depth = opening + 1, 1
    while depth:
        depth += (upload_source[end] == '{') - (upload_source[end] == '}')
        end += 1
    mailbox = upload_source[start:end] + ';\n'
    alpha_fixture = r'''
const LLUUID IMG_INVISIBLE{99};
struct LLViewerTexLayerSet {
 bool visible=true;
 bool isVisible() const { return visible; }
};
struct LLViewerTexLayerSetBuffer {
 LLViewerTexLayerSet layer;
 std::shared_ptr<LLTasiaBakeCompletion> mUploadCompletion;
 LLViewerTexLayerSet* getViewerTexLayerSet() { return &layer; }
 bool completeInvisibleBake();
};
'''
    start = upload_source.index('bool LLViewerTexLayerSetBuffer::completeInvisibleBake()')
    opening = upload_source.index('{', start)
    end, depth = opening + 1, 1
    while depth:
        depth += (upload_source[end] == '{') - (upload_source[end] == '}')
        end += 1
    alpha_method = upload_source[start:end]
    upload = upload_source[upload_source.index('void LLViewerTexLayerSetBuffer::doUpload()'):start]
    assert upload.index('if (completeInvisibleBake()) return;') < upload.index('LLBake::readColor(')
    assert upload.index('if (completeInvisibleBake()) return;') < upload.index('compressed->encode(')
    (tmp / 'test.cpp').write_text(fixture + mailbox + alpha_fixture + alpha_method + function('bool LLVOAvatarSelf::usesLegacyBakes() const') +
                                 function('void LLVOAvatarSelf::sendLegacyAppearance()') + tests)
    subprocess.run(['g++', '-std=c++17', '-pthread', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    str(tmp / 'test.cpp'), '-o', str(tmp / 'test')], check=True)
    subprocess.run([str(tmp / 'test')], check=True)
print('PASS: production upload policy, invisible-alpha route and appearance sender: stale/cancelled callbacks, '
      'bounded retries, SL/server-bake isolation, pending/wearable/TE guards, params, serial '
      'wrap and packet shape (fixtures, ASan+UBSan; no live network).')

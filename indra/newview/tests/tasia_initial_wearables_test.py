#!/usr/bin/env python3
"""Compile production snapshot validation, callback and atomic commit with fixtures.

No real inventory, network, assets or avatar session is accessed.
"""
import subprocess
import tempfile
from pathlib import Path

indra = Path(__file__).resolve().parents[2]
src = (indra / 'newview/llagentwearables.cpp').read_text()


def extract(marker, semicolon=False):
    start = src.index(marker)
    opening = src.index('{', start)
    end, depth = opening + 1, 1
    while depth:
        depth += (src[end] == '{') - (src[end] == '}')
        end += 1
    return src[start:end] + (';\n' if semicolon else '\n')


fixture = r'''
#include "lltasia_legacy_wearables.h"
#include <cassert>
#include <algorithm>
#include <memory>
#include <cstdint>
#include <sstream>
#include <random>
#include <set>
using U8=std::uint8_t; using U32=std::uint32_t; using U64=std::uint64_t; using S32=int;
struct LLUUID {
 int value=0;
 bool isNull() const { return value==0; }
 bool notNull() const { return value!=0; }
 bool operator==(const LLUUID& other) const { return value==other.value; }
 bool operator!=(const LLUUID& other) const { return !(*this==other); }
};
using uuid_vec_t=std::vector<LLUUID>;
namespace LLAssetType {enum EType {AT_LINK=1};}
namespace LLWearableType { enum EType { WT_SHAPE, WT_SKIN, WT_HAIR, WT_EYES, WT_SHIRT, WT_COUNT=17 }; }
constexpr U32 MAX_CLOTHING_LAYERS = 60;
struct LLViewerWearable {
 LLUUID asset, item; LLWearableType::EType type; int permissions=0;
 void setItemID(LLUUID id) { item=id; }
 void setPermissions(int value) { permissions=value; }
 LLWearableType::EType getType() const { return type; }
 LLUUID getAssetID() const { return asset; }
};
struct LLVOAvatarSelf {
 bool legacy=true; LLUUID id{1}; int changes=0;
 bool usesLegacyBakes() const { return legacy; }
 LLUUID getID() const { return id; }
 void setCompositeUpdatesEnabled(bool) { ++changes; }
 void writeWearablesToAvatar() { ++changes; }
 void updateVisualParams() { ++changes; }
 void invalidateAll() { ++changes; }
 void onFirstTEMessageReceived() { ++changes; }
};
std::shared_ptr<LLVOAvatarSelf> gAgentAvatarp;
bool isAgentAvatarValid() { return bool(gAgentAvatarp); }
struct { LLUUID session{2}; LLUUID getSessionID() const { return session; } } gAgent;
struct LLGridManager {
 bool opensim=true;
 static LLGridManager* getInstance() { static LLGridManager self; return &self; }
 bool isInOpenSim() const { return opensim; }
};
struct LLViewerInventoryItem {
 int type=4;
 int getPermissions() const { return 7; }
 int getWearableType() const {return type;}
 int getActualType() const {return LLAssetType::AT_LINK;}
 LLUUID getLinkedUUID() const {return {100};}
} inventory_item;
struct LLFindWearables {};
struct LLFindBrokenLinks {};
struct LLInventoryModel {
 using cat_array_t=std::vector<int>;
 using item_array_t=std::vector<LLViewerInventoryItem*>;
 enum {EXCLUDE_TRASH};
};
struct LLViewerInventoryCategory { int version=3; int getVersion() const {return version;} } category;
struct {
 bool complete=true;
 bool broken=false;
 LLInventoryModel::item_array_t cof_items;
 LLViewerInventoryItem* getItem(LLUUID) { return &inventory_item; }
 LLViewerInventoryCategory* getCategory(LLUUID) {return &category;}
 bool isCategoryComplete(LLUUID) const {return complete;}
 void collectDescendentsIf(LLUUID,LLInventoryModel::cat_array_t&,
     LLInventoryModel::item_array_t& items,int,LLFindWearables) const {items=cof_items;}
 void collectDescendentsIf(LLUUID,LLInventoryModel::cat_array_t&,
     LLInventoryModel::item_array_t& items,int,LLFindBrokenLinks) const {
  if(broken)items.push_back(&inventory_item);
 }
} gInventory;
int storage=1, message=1;
int* gAssetStorage=&storage;
int* gMessageSystem=&message;
int fetch_requests=0, scheduled=0;
int library_fetch_requests=0;
bool requestLegacyCOFLibraryLinks(const LLInventoryModel::item_array_t&,U64) {++library_fetch_requests;return true;}
struct LLInventoryFetchItemsObserver {
 explicit LLInventoryFetchItemsObserver(const uuid_vec_t&) {}
 void startFetch() {++fetch_requests;}
};
template<class F> void doAfterInterval(F,float) {++scheduled;}
bool has_cof=false;
bool cofHasWearables(bool incomplete_is_worn=true) {
 if(!gInventory.complete)return incomplete_is_worn;
 return has_cof;
}
struct LLAppearanceMgr {
 int updates=0;
 static LLAppearanceMgr& instance() { static LLAppearanceMgr self; return self; }
  void updateAppearanceFromCOF(bool=true,bool=true) { ++updates; }
  LLUUID getCOF() const {return {9};}
};
struct LLWearableData {
 int updated=0;
 void wearableUpdated(LLViewerWearable*, bool) { ++updated; }
};
std::ostringstream diagnostic;
#define LL_WARNS(category) diagnostic
#define LL_WARNS_ONCE(category) diagnostic
#define LL_INFOS_ONCE(category) diagnostic
#define LL_ENDL "\n"
'''
manager = r'''
struct LLAgentWearables : LLWearableData {
 U64 mLegacyWearablesGeneration=1;
 bool mLegacyBodyPartRecovery=false;
 bool mLegacyCOFLinkFetchAttempted=false, mLegacyCOFLinkFetchActive=false;
 bool mLegacyCOFLibraryFetchAttempted=false;
 uuid_vec_t mLegacyBodyPartUnresolvedTargets;
  bool mLegacyBodyPartRecoveryAttempted=false, mLegacyWearablesWaiting=false;
  bool mLegacyInitialCOFFetchFailed=false;
 unsigned mLegacyWearablesRequests=0;
 static bool mInitialWearablesUpdateReceived;
 U8 mLegacyBodyPartMask=0;
 S32 mLegacyBodyPartCOFVersion=3;
 LLUUID mLegacyWearablesSession{2}, mLegacyWearablesAvatar{1};
 LLVOAvatarSelf* mAvatarAppearance=nullptr;
 bool mWearablesLoaded=false;
 int notified=0;
 std::shared_ptr<LLTasiaLegacyWearablesLoad> mLegacyWearablesLoad;
 std::vector<LLViewerWearable*> mWearableDatas;
 unsigned getWearableCount(LLWearableType::EType type) const {
  unsigned n=0; for(auto* w:mWearableDatas) if(w && w->type==type)++n; return n;
 }
 bool isLegacyInitialWearablesCurrent(U64 generation) const;
 void finishLegacyInitialWearables();
 void recoverLegacyBodyPartsFromSimulator();
 void retryLegacyCOFLinkFetch(U64,unsigned);
 void sendLegacyWearablesRequest() {++mLegacyWearablesRequests;}
 static void onLegacyWearableAssetArrived(LLViewerWearable*,void*);
 void pushWearable(LLWearableType::EType, LLViewerWearable* item, bool) { mWearableDatas.push_back(item); }
 void notifyLoadingFinished() { ++notified; }
} gAgentWearables;
bool LLAgentWearables::mInitialWearablesUpdateReceived=false;
'''
tests = r'''
using Entry=LLTasiaLegacyWearablesLoad::Entry;
std::vector<Entry> valid() { return {{0,{1},{101}},{1,{2},{102}},{2,{3},{103}},
                                    {3,{4},{104}},{4,{5},{105}},{4,{6},{106}}}; }
bool oracle(const std::vector<Entry>& entries) {
 if(entries.size()<4 || entries.size()>64) return false;
 std::set<int> items,assets; unsigned body[4]={};
 for(const auto& e:entries) {
  if(e.type>=17 || e.item.value==0 || e.asset.value==0) return false;
  if(!items.insert(e.item.value).second || !assets.insert(e.asset.value).second) return false;
  if(e.type<4) ++body[e.type];
 }
 return body[0]==1 && body[1]==1 && body[2]==1 && body[3]==1;
}
int main() {
 {
  auto entries=valid();
  auto details=TasiaWearables::diagnoseSnapshot(entries,17);
  assert(details.entries==entries.size() && details.null_items==0 && details.null_assets==0);
  assert(details.duplicate_items==0 && details.shared_assets==0 && details.invalid_types==0);
  assert(details.null_body_item_mask==0 && details.null_body_asset_mask==0);
  for(auto count:details.body)assert(count==1);
  entries[4].asset=entries[5].asset;
  details=TasiaWearables::diagnoseSnapshot(entries,17);
  assert(details.shared_assets==1 && !TasiaWearables::validSnapshot(entries,17,60));
  entries[4].item={}; entries[4].asset={}; entries[4].type=17;
  details=TasiaWearables::diagnoseSnapshot(entries,17);
  assert(details.null_items==1 && details.null_assets==1 && details.invalid_types==1);
  assert(details.null_body_asset_mask==0); // A bad clothing slot is not a missing body-part asset.
  entries[2].asset={};entries[3].asset={};
  details=TasiaWearables::diagnoseSnapshot(entries,17);
  assert(details.null_body_asset_mask==12 && details.null_body_item_mask==0);
 }
 assert(TasiaWearables::validSnapshot(valid(),17,60)); // Two shirts kept.
 std::mt19937 random(9284);
 for(int run=0;run<12000;++run) {
  auto entries=valid();
  switch(random()%6) {
   case 0: entries.erase(entries.begin()+(random()%entries.size())); break;
   case 1: entries[random()%entries.size()].item={0}; break;
   case 2: entries[random()%entries.size()].type=U8(random()%256); break;
   case 3: entries.push_back(entries[random()%entries.size()]); break;
   case 4: entries[random()%entries.size()].asset=entries[0].asset; break;
   default: break;
  }
  assert(TasiaWearables::validSnapshot(entries,17,60)==oracle(entries));
 }
 assert(!TasiaWearables::validSnapshot(valid(),3,60));
 const std::vector<Entry> part_subset={valid()[2],valid()[3]};
 assert(TasiaWearables::validBodyPartRepair(part_subset,12,uuid_vec_t{{3},{4}}));
 assert(!TasiaWearables::validBodyPartRepair(part_subset,12,uuid_vec_t{{3},{99}}));
 assert(!TasiaWearables::validBodyPartRepair(part_subset,4,uuid_vec_t{{3},{4}}));
 assert(!TasiaWearables::validBodyPartRepair(part_subset,0,uuid_vec_t{}));
 assert(!TasiaWearables::validBodyPartRepair(part_subset,16,uuid_vec_t{}));
 assert(!TasiaWearables::validBodyPartRepair(std::vector<Entry>{valid()[2],valid()[2]},12,uuid_vec_t{}));
 assert(!TasiaWearables::validBodyPartRepair(std::vector<Entry>{valid()[4]},16,uuid_vec_t{}));
 assert(!TasiaWearables::validSnapshot(valid(),17,61));
 auto max=valid();
 for(int i=7;i<=64;++i) max.push_back({4,{i},{i+100}});
  assert(TasiaWearables::validSnapshot(max,17,60));
  max.push_back({4,{65},{165}}); assert(!TasiaWearables::validSnapshot(max,17,60));

  // Incomplete COF is normally authoritative/unknown, but an explicit startup
  // fetch failure permits the legacy snapshot fallback even if partial items exist.
  has_cof=true; gInventory.complete=false;
  assert(cofHasWearables());
  assert(!cofHasWearables(false));
  gInventory.complete=true; assert(cofHasWearables(false));
  has_cof=false;

 gAgentAvatarp=std::make_shared<LLVOAvatarSelf>();
 auto& owner=gAgentWearables; owner.mAvatarAppearance=gAgentAvatarp.get();
 assert(owner.isLegacyInitialWearablesCurrent(1));
 assert(!owner.isLegacyInitialWearablesCurrent(0));
 gAgent.session={99}; assert(!owner.isLegacyInitialWearablesCurrent(1)); gAgent.session={2};
 LLGridManager::getInstance()->opensim=false;
 assert(!owner.isLegacyInitialWearablesCurrent(1)); LLGridManager::getInstance()->opensim=true;
 gAgentAvatarp->legacy=false; assert(!owner.isLegacyInitialWearablesCurrent(1)); gAgentAvatarp->legacy=true;
 owner.mAvatarAppearance=nullptr; assert(!owner.isLegacyInitialWearablesCurrent(1));
 owner.mAvatarAppearance=gAgentAvatarp.get();

 auto load=std::make_shared<LLTasiaLegacyWearablesLoad>();
 load->generation=1; load->entries=valid(); load->remaining=1;
 std::vector<LLViewerWearable> assets;
 for(auto& e:load->entries) assets.push_back({e.asset,{},LLWearableType::EType(e.type),0});
 for(auto& asset:assets) load->wearables.push_back(&asset);
 owner.mLegacyWearablesLoad=load; owner.mWearableDatas.push_back(nullptr);
 owner.finishLegacyInitialWearables(); assert(owner.mWearableDatas.size()==1); // Not all replies.
 load->remaining=0; load->failed=true;
 owner.finishLegacyInitialWearables();
 assert(owner.mWearableDatas.size()==1 && !owner.mWearablesLoaded && owner.notified==1);
 assert(gAgentAvatarp->changes==0); // No partial/default outfit applied.

 load->generation=owner.mLegacyWearablesGeneration; load->failed=false;
 owner.mLegacyWearablesLoad=load; has_cof=true;
 owner.finishLegacyInitialWearables();
 assert(owner.mWearableDatas.size()==1 && LLAppearanceMgr::instance().updates==1);
 has_cof=false; load->generation=owner.mLegacyWearablesGeneration;
 owner.mLegacyWearablesLoad=load;
 owner.finishLegacyInitialWearables();
  assert(owner.mWearablesLoaded && owner.mWearableDatas.size()==6 && owner.updated==6);
  assert(gAgentAvatarp->changes==5 && owner.notified==2);
  for(std::size_t i=0;i<assets.size();++i) assert(assets[i].item==load->entries[i].item && assets[i].permissions==7);

  // A failed startup COF fetch must not treat partial wearable links as a
  // complete outfit; the validated simulator snapshot remains the fallback.
  owner.mWearableDatas.clear(); owner.mWearablesLoaded=false;
  owner.mLegacyInitialCOFFetchFailed=true;
  gInventory.complete=false; has_cof=true;
  load->generation=owner.mLegacyWearablesGeneration; load->failed=false;
  load->remaining=0; owner.mLegacyWearablesLoad=load;
  const int updates_before_failed_cof=LLAppearanceMgr::instance().updates;
  owner.finishLegacyInitialWearables();
  assert(owner.mWearablesLoaded && owner.mWearableDatas.size()==6);
  assert(!owner.mLegacyInitialCOFFetchFailed);
  assert(LLAppearanceMgr::instance().updates==updates_before_failed_cof);
  gInventory.complete=true; has_cof=false;

  // A late callback from the just-completed generation is discarded safely.
 load->remaining=1;
 auto* callback=new LLTasiaLegacyWearableCallback{load,0};
 LLAgentWearables::onLegacyWearableAssetArrived(&assets[0],callback);
  assert(load->remaining==1 && owner.mWearableDatas.size()==6);

 // Partial COF body-part repair keeps shape, skin and clothing intact.
 owner.mWearableDatas={&assets[0],&assets[1],&assets[4],&assets[5]};
 gAgentAvatarp->legacy=false;
 owner.recoverLegacyBodyPartsFromSimulator();
 assert(!owner.mLegacyBodyPartRecoveryAttempted);
 gAgentAvatarp->legacy=true;
 inventory_item.type=2; gInventory.cof_items={&inventory_item};
 owner.recoverLegacyBodyPartsFromSimulator();
 assert(!owner.mLegacyBodyPartRecoveryAttempted); // Don't mask failed COF downloads.
 gInventory.cof_items.clear(); gInventory.broken=true;
 owner.recoverLegacyBodyPartsFromSimulator();
 assert(!owner.mLegacyBodyPartRecoveryAttempted);
 assert(owner.mLegacyCOFLinkFetchActive && fetch_requests==1 && scheduled==1);
 owner.recoverLegacyBodyPartsFromSimulator();
 assert(fetch_requests==1);
 owner.retryLegacyCOFLinkFetch(owner.mLegacyWearablesGeneration-1,0);
 assert(owner.mLegacyCOFLinkFetchActive && scheduled==1); // stale timer ignored.
  owner.retryLegacyCOFLinkFetch(owner.mLegacyWearablesGeneration,0);
  assert(owner.mLegacyCOFLinkFetchActive && scheduled==2);
  assert(library_fetch_requests==1 && owner.mLegacyCOFLibraryFetchAttempted);
 owner.retryLegacyCOFLinkFetch(owner.mLegacyWearablesGeneration,3);
 assert(!owner.mLegacyCOFLinkFetchActive && scheduled==2); // bounded stop, links intact.
 assert(owner.mLegacyBodyPartRecovery && owner.mLegacyBodyPartRecoveryAttempted);
 assert(owner.mLegacyBodyPartUnresolvedTargets==uuid_vec_t{{100}});
 owner.recoverLegacyBodyPartsFromSimulator();
 assert(!owner.mLegacyCOFLinkFetchActive && fetch_requests==1); // no repeated fetch loop.
 owner.mLegacyBodyPartRecovery=false; owner.mLegacyBodyPartRecoveryAttempted=false;
 owner.mLegacyWearablesWaiting=false; owner.mLegacyBodyPartUnresolvedTargets.clear();
 gInventory.broken=false;
 owner.mLegacyCOFLinkFetchActive=true;
 const int previous_updates=LLAppearanceMgr::instance().updates;
 owner.retryLegacyCOFLinkFetch(owner.mLegacyWearablesGeneration,0);
 assert(!owner.mLegacyCOFLinkFetchActive && LLAppearanceMgr::instance().updates==previous_updates+1);
 owner.recoverLegacyBodyPartsFromSimulator();
 assert(owner.mLegacyBodyPartRecovery && owner.mLegacyBodyPartMask==12);
 assert(owner.mLegacyWearablesRequests==1);
 owner.recoverLegacyBodyPartsFromSimulator();
 assert(owner.mLegacyWearablesRequests==1); // One repair attempt per login.
 assert(owner.isLegacyInitialWearablesCurrent(owner.mLegacyWearablesGeneration));
 category.version=4;
 assert(!owner.isLegacyInitialWearablesCurrent(owner.mLegacyWearablesGeneration));
 category.version=3; gInventory.complete=false;
 assert(!owner.isLegacyInitialWearablesCurrent(owner.mLegacyWearablesGeneration));
 gInventory.complete=true;
 owner.mWearableDatas.push_back(&assets[2]);
 assert(!owner.isLegacyInitialWearablesCurrent(owner.mLegacyWearablesGeneration));
 owner.mWearableDatas.pop_back();
 auto repair=std::make_shared<LLTasiaLegacyWearablesLoad>();
 repair->generation=owner.mLegacyWearablesGeneration;
 repair->entries={valid()[2],valid()[3]};
 repair->wearables={&assets[2],&assets[3]};
 // Do not use a real simulator asset for a different unresolved inventory link.
 owner.mLegacyBodyPartUnresolvedTargets={{99}};
 repair->generation=owner.mLegacyWearablesGeneration;
 owner.mLegacyWearablesLoad=repair;
 owner.finishLegacyInitialWearables();
 assert(owner.mWearableDatas.size()==4);
 owner.mLegacyBodyPartUnresolvedTargets={{3},{4}};
 repair->generation=owner.mLegacyWearablesGeneration;
 has_cof=true; repair->failed=true; owner.mLegacyWearablesLoad=repair;
 owner.finishLegacyInitialWearables();
 assert(owner.mWearableDatas.size()==4); // Failed asset never partially commits.
 repair->failed=false; repair->generation=owner.mLegacyWearablesGeneration;
 owner.mLegacyWearablesLoad=repair;
 owner.finishLegacyInitialWearables();
 assert(owner.mWearableDatas.size()==6 && !owner.mLegacyBodyPartRecovery);
 assert(owner.mWearableDatas[0]==&assets[0] && owner.mWearableDatas[1]==&assets[1]);
 assert(owner.mWearableDatas[2]==&assets[4] && owner.mWearableDatas[3]==&assets[5]);
 assert(owner.getWearableCount(LLWearableType::WT_HAIR)==1);
 assert(owner.getWearableCount(LLWearableType::WT_EYES)==1);
}
'''
with tempfile.TemporaryDirectory(prefix='initial-wearables-', dir='/tmp/opencode') as tmp:
    tmp = Path(tmp)
    (tmp / 'lltasia_legacy_wearables.h').write_bytes((indra / 'newview/lltasia_legacy_wearables.h').read_bytes())
    unit = (fixture + extract('struct LLTasiaLegacyWearablesLoad\n', True) +
            extract('struct LLTasiaLegacyWearableCallback\n', True) + manager +
             extract('bool LLAgentWearables::isLegacyInitialWearablesCurrent(U64 generation) const') +
             extract('void LLAgentWearables::recoverLegacyBodyPartsFromSimulator()') +
             extract('void LLAgentWearables::retryLegacyCOFLinkFetch(') +
            extract('void LLAgentWearables::finishLegacyInitialWearables()') +
            extract('void LLAgentWearables::onLegacyWearableAssetArrived(') + tests)
    (tmp / 'test.cpp').write_text(unit)
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    str(tmp / 'test.cpp'), '-o', str(tmp / 'test')], check=True)
    subprocess.run([str(tmp / 'test')], check=True)

# Verify wiring and security checks absent from fixtures are in the real source.
handler = extract('void LLAgentWearables::processAgentInitialWearablesUpdate(')
assert handler.index('session != gAgent.getSessionID()') < handler.index('getAsset(')
assert handler.index('msg->getSender()') < handler.index('getAsset(')
assert handler.index('validSnapshot(') < handler.index('getAsset(')
commit = extract('void LLAgentWearables::finishLegacyInitialWearables()')
for banned in ('createNewWearable', 'saveWearable', 'link_inventory', 'updateServer', 'AgentIsNowWearing'):
    assert banned not in '\n'.join(l for l in commit.splitlines() if not l.strip().startswith('//')), banned
asset_src = (indra / 'newview/llwearablelist.cpp').read_text()
assert asset_src.index('data->mIsCurrent && !data->mIsCurrent()') < asset_src.index('wearable->importStream(')
startup = (indra / 'newview/llstartup.cpp').read_text()
assert 'if (!gAgentWearables.startLegacyInitialWearables(cof_fetch_failed))' in startup
assert 'const bool cof_fetch_failed = !gInventory.isCategoryComplete(cof);' in startup
assert 'cof_fetch_failed && gAgentAvatarp && gAgentAvatarp->usesLegacyBakes()' in startup
assert 'LLAppearanceMgr::instance().updateAppearanceFromCOF(true, true, no_op);' in startup
assert '_PREHASH_AgentWearablesUpdate, LLAgentWearables::processAgentInitialWearablesUpdate' in startup
source = (indra / 'newview/llagentwearables.cpp').read_text()
cof_helper = extract('bool cofHasWearables(')
assert 'if (cof.isNull()) return incomplete_is_worn;' in cof_helper
assert 'if (!gInventory.isInventoryUsable())' in cof_helper
assert 'return incomplete_is_worn;' in cof_helper
deadline = source[source.index('Initial wearable download deadline exceeded')-400:source.index('Initial wearable download deadline exceeded')]
assert 'owner.mLegacyInitialCOFFetchFailed = false;' in deadline
background_fetch = (indra / 'newview/llinventorymodelbackgroundfetch.cpp').read_text()
ais_cof_callback = background_fetch[background_fetch.index('void LLInventoryModelBackgroundFetch::fetchCOF('):]
assert 'AISAPI::FetchCOF([callback](const LLUUID& id)' in ais_cof_callback
ais_impl = (indra / 'newview/llaisapi.cpp').read_text()
update_received = ais_impl.index('onUpdateReceived(result, type, body);')
completion_callback = ais_impl.index('callback(id);', update_received)
assert update_received < completion_callback
print('PASS: initial-wearable snapshot (12000 cases), actual generation/session guard, '
       'atomic commit and stale callback; COF priority, no inventory/default mutation, '
       'missing-part-only repair preserves clothing and rejects failed assets, changed COF, '
       'already-filled slots and broken links; pre-import lifetime guard; '
       'ASan+UBSan fixtures, live loading untested.')

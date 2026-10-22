/**
 * @file llagentwearables.cpp
 * @brief LLAgentWearables class implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "llviewernetwork.h"
#include "llagentwearables.h"

#include "llattachmentsmgr.h"
#include "llaccordionctrltab.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llappearancemgr.h"
#include "llcallbacklist.h"
#include "llfloatersidepanelcontainer.h"
#include "llgesturemgr.h"
#include "llinventorybridge.h"
#include "llinventoryfunctions.h"
#include "llinventoryobserver.h"
#include "llinventorymodelbackgroundfetch.h"
#include "llinventoryobserver.h"
#include "llinventorypanel.h"
#include "lllocaltextureobject.h"
#include "llnotificationsutil.h"
#include "lloutfitobserver.h"
#include "llsidepanelappearance.h"
#include "lltexlayer.h"
#include "lltooldraganddrop.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "llviewerwearable.h"
#include "llwearablelist.h"
#include "llfloaterperms.h"
#include "llviewernetwork.h"
#include "lltasia_legacy_wearables.h"
#include "message.h"
#include "message_prehash.h"

struct LLTasiaLegacyWearablesLoad
{
    struct Entry
    {
        U8 type;
        LLUUID item;
        LLUUID asset;
    };
    U64 generation = 0;
    std::vector<Entry> entries;
    std::vector<LLViewerWearable*> wearables;
    std::size_t remaining = 0;
    bool failed = false;
};

namespace
{
struct LLTasiaLegacyWearableCallback
{
    std::shared_ptr<LLTasiaLegacyWearablesLoad> load;
    std::size_t index;
};

class LLTasiaCOFLibraryFetch final : public LLInventoryModel::FetchItemHttpHandler
{
public:
    LLTasiaCOFLibraryFetch(const LLSD& request, U64 generation)
        : LLInventoryModel::FetchItemHttpHandler(request), mGeneration(generation),
          mCOF(LLAppearanceMgr::instance().getCOF())
    {
        const auto* category = gInventory.getCategory(mCOF);
        if (category) mCOFVersion = category->getVersion();
    }

    void onCompleted(LLCore::HttpHandle handle, LLCore::HttpResponse* response) override
    {
        // A lookup from a previous login/outfit must not mutate the new inventory.
        if (!gAgentWearables.isLegacyInitialWearablesCurrent(mGeneration) ||
            LLAppearanceMgr::instance().getCOF() != mCOF) return;
        const auto* category = gInventory.getCategory(mCOF);
        if (!category || !gInventory.isCategoryComplete(mCOF) || category->getVersion() != mCOFVersion) return;
        LLInventoryModel::FetchItemHttpHandler::onCompleted(handle, response);
    }
private:
    U64 mGeneration;
    LLUUID mCOF;
    S32 mCOFVersion = -1;
};

bool requestLegacyCOFLibraryLinks(const LLInventoryModel::item_array_t& broken, U64 generation)
{
    if (!gAgentWearables.isLegacyInitialWearablesCurrent(generation) || !gAgent.getRegion()) return false;
    const LLUUID library_owner = gInventory.getLibraryOwnerID();
    if (library_owner.isNull() || library_owner == gAgent.getID()) return false;
    const std::string cap = gAgent.getRegion()->getCapability("FetchLib2");
    if (cap.empty()) return false;
    uuid_vec_t targets;
    for (const auto& item : broken)
    {
        if (item->getActualType() != LLAssetType::AT_LINK) continue;
        const LLUUID target = item->getLinkedUUID();
        if (target.isNull() || std::find(targets.begin(), targets.end(), target) != targets.end()) continue;
        const auto* known = gInventory.getItem(target);
        if (known && known->isFinished()) continue;
        targets.push_back(target);
        if (targets.size() > 256) return false;
    }
    if (targets.empty() || targets.size() > 256) return false;
    LLSD request;
    request["agent_id"] = gAgent.getID();
    for (const auto& target : targets)
    {
        LLSD item;
        item["owner_id"] = library_owner; // Advertised by login, not SL's hardcoded owner.
        item["item_id"] = target;
        request["items"].append(item);
    }
    auto handler = std::make_shared<LLTasiaCOFLibraryFetch>(request, generation);
    const auto queued = gInventory.requestPost(true, cap, request, handler, "COF Library Item");
    if (queued == LLCORE_HTTP_HANDLE_INVALID) return false;
    LL_WARNS("LocalBake") << "COF library target lookup requested; targets=" << targets.size() << LL_ENDL;
    return true;
}

bool cofHasWearables(bool incomplete_is_worn = true)
{
    LLInventoryModel::cat_array_t categories;
    LLInventoryModel::item_array_t items;
    LLFindWearables predicate;
    const LLUUID cof = LLAppearanceMgr::instance().getCOF();
    if (cof.isNull()) return incomplete_is_worn;
    if (!gInventory.isInventoryUsable())
        return incomplete_is_worn; // An explicit failed fetch makes COF unusable as authority.
    if (!gInventory.isCategoryComplete(cof))
        return incomplete_is_worn; // Unknown/incomplete is normally not an empty outfit.
    gInventory.collectDescendentsIf(cof, categories, items, LLInventoryModel::EXCLUDE_TRASH, predicate);
    return !items.empty();
}
}

LLAgentWearables gAgentWearables;

bool LLAgentWearables::mInitialWearablesUpdateReceived = false;

using namespace LLAvatarAppearanceDefines;

///////////////////////////////////////////////////////////////////////////////

void set_default_permissions(LLViewerInventoryItem* item)
{
	llassert(item);
	LLPermissions perm = item->getPermissions();
	if (perm.getMaskNextOwner() != LLFloaterPerms::getNextOwnerPerms("Wearables")
		|| perm.getMaskEveryone() != LLFloaterPerms::getEveryonePerms("Wearables")
		|| perm.getMaskGroup() != LLFloaterPerms::getGroupPerms("Wearables"))
	{
		perm.setMaskNext(LLFloaterPerms::getNextOwnerPerms("Wearables"));
		perm.setMaskEveryone(LLFloaterPerms::getEveryonePerms("Wearables"));
		perm.setMaskGroup(LLFloaterPerms::getGroupPerms("Wearables"));

		item->setPermissions(perm);

		item->updateServer(false);
	}
}

// Callback to wear and start editing an item that has just been created.
void wear_and_edit_cb(const LLUUID& inv_item)
{
	if (inv_item.isNull()) return;

	LLViewerInventoryItem* item = gInventory.getItem(inv_item);
	if (!item) return;

	set_default_permissions(item);

	// item was just created, update even if permissions did not changed
	gInventory.updateItem(item);
	gInventory.notifyObservers();

	// Request editing the item after it gets worn.
	gAgentWearables.requestEditingWearable(inv_item);

	// Wear it.
	LLAppearanceMgr::instance().wearItemOnAvatar(inv_item, true);
}

void wear_cb(const LLUUID& inv_item)
{
	if (!inv_item.isNull())
	{
		LLViewerInventoryItem* item = gInventory.getItem(inv_item);
		if (item)
		{
			set_default_permissions(item);

			gInventory.updateItem(item);
			gInventory.notifyObservers();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

// HACK: For EXT-3923: Pants item shows in inventory with skin icon and messes with "current look"
// Some db items are corrupted, have inventory flags = 0, implying wearable type = shape, even though
// wearable type stored in asset is some other value.
// Calling this function whenever a wearable is added to increase visibility if this problem
// turns up in other inventories.
void checkWearableAgainstInventory(LLViewerWearable* wearable)
{
	if (wearable->getItemID().isNull())
		return;

	// Check for wearable type consistent with inventory item wearable type.
	LLViewerInventoryItem* item = gInventory.getItem(wearable->getItemID());
	if (item)
	{
		if (!item->isWearableType())
		{
			LL_WARNS() << "wearable associated with non-wearable item" << LL_ENDL;
		}
		if (item->getWearableType() != wearable->getType())
		{
			LL_WARNS() << "type mismatch: wearable " << wearable->getName()
				<< " has type " << wearable->getType()
				<< " but inventory item " << item->getName()
				<< " has type " << item->getWearableType() << LL_ENDL;
		}
	}
	else
	{
		LL_WARNS() << "wearable inventory item not found" << wearable->getName()
			<< " itemID " << wearable->getItemID().asString() << LL_ENDL;
	}
}

void LLAgentWearables::dump()
{
	LL_INFOS() << "LLAgentWearablesDump" << LL_ENDL;
	for (S32 i = 0; i < LLWearableType::WT_COUNT; i++)
	{
		U32 count = getWearableCount((LLWearableType::EType)i);
		LL_INFOS() << "Type: " << i << " count " << count << LL_ENDL;
		for (U32 j = 0; j < count; j++)
		{
			LLViewerWearable* wearable = getViewerWearable((LLWearableType::EType)i, j);
			if (wearable == NULL)
			{
				LL_INFOS() << "    " << j << " NULL wearable" << LL_ENDL;
			}
			LL_INFOS() << "    " << j << " Name " << wearable->getName()
				<< " description " << wearable->getDescription() << LL_ENDL;

		}
	}
}

struct LLAgentDumper
{
	LLAgentDumper(std::string name) :
		mName(name)
	{
		LL_INFOS() << LL_ENDL;
		LL_INFOS() << "LLAgentDumper " << mName << LL_ENDL;
		gAgentWearables.dump();
	}

	~LLAgentDumper()
	{
		LL_INFOS() << LL_ENDL;
		LL_INFOS() << "~LLAgentDumper " << mName << LL_ENDL;
		gAgentWearables.dump();
	}

	std::string mName;
};

LLAgentWearables::LLAgentWearables() :
	LLWearableData(),
	mWearablesLoaded(false)
	, mCOFChangeInProgress(false)
{
}

LLAgentWearables::~LLAgentWearables()
{
	cleanup();
}

void LLAgentWearables::cleanup()
{
    mLegacyBodyPartRecovery = false;
    mLegacyBodyPartRecoveryAttempted = false;
    mLegacyCOFLinkFetchAttempted = false;
    mLegacyCOFLibraryFetchAttempted = false;
    mLegacyCOFLinkFetchActive = false;
    mLegacyBodyPartUnresolvedTargets.clear();
    mLegacyBodyPartMask = 0;
    ++mLegacyWearablesGeneration;
    mLegacyWearablesWaiting = false;
    mLegacyInitialCOFFetchFailed = false;
    mLegacyWearablesLoad.reset();
    mLegacyWearablesSession.setNull();
    mLegacyWearablesAvatar.setNull();
    mInitialWearablesUpdateReceived = false;
    mWearablesLoaded = false;
}

// static
void LLAgentWearables::initClass()
{
	// this can not be called from constructor because its instance is global and is created too early.
	// Subscribe to "COF is Saved" signal to notify observers about this (Loading indicator for ex.).
	LLOutfitObserver::instance().addCOFSavedCallback(boost::bind(&LLAgentWearables::notifyLoadingFinished, &gAgentWearables));
}

void LLAgentWearables::setAvatarObject(LLVOAvatarSelf* avatar)
{
	llassert(avatar);
	cleanup();
	setAvatarAppearance(avatar);
}

bool LLAgentWearables::isLegacyInitialWearablesCurrent(U64 generation) const
{
    if (mLegacyBodyPartRecovery || mLegacyCOFLinkFetchActive)
    {
        const LLUUID cof = LLAppearanceMgr::instance().getCOF();
        const LLViewerInventoryCategory* category = gInventory.getCategory(cof);
        if (!category || !gInventory.isCategoryComplete(cof) ||
            category->getVersion() != mLegacyBodyPartCOFVersion) return false;
        for (U8 type = 0; type < 4; ++type)
            if (mLegacyBodyPartRecovery && (mLegacyBodyPartMask & (1u << type)) &&
                getWearableCount(static_cast<LLWearableType::EType>(type)) != 0) return false;
    }
    return generation == mLegacyWearablesGeneration && isAgentAvatarValid() &&
           LLGridManager::getInstance()->isInOpenSim() && gAgentAvatarp->usesLegacyBakes() &&
           mAvatarAppearance == gAgentAvatarp.get() &&
           mLegacyWearablesSession == gAgent.getSessionID() &&
           mLegacyWearablesAvatar == gAgentAvatarp->getID();
}

void LLAgentWearables::recoverLegacyBodyPartsFromSimulator()
{
    if (mLegacyBodyPartRecoveryAttempted || mLegacyCOFLinkFetchActive || !isAgentAvatarValid() ||
        !gAgentAvatarp->usesLegacyBakes() || !gAssetStorage || !gMessageSystem ||
        mLegacyWearablesWaiting || mLegacyWearablesLoad) return;
    const LLUUID cof = LLAppearanceMgr::instance().getCOF();
    const LLViewerInventoryCategory* category = gInventory.getCategory(cof);
    if (!category || !gInventory.isCategoryComplete(cof)) return;
    U8 missing = 0;
    for (U8 type = 0; type < 4; ++type)
        if (!getWearableCount(static_cast<LLWearableType::EType>(type))) missing |= 1u << type;
    if (!missing) return;
    mLegacyBodyPartUnresolvedTargets.clear();
    LLInventoryModel::cat_array_t broken_categories;
    LLInventoryModel::item_array_t broken_items;
    LLFindBrokenLinks broken_predicate;
    gInventory.collectDescendentsIf(cof, broken_categories, broken_items,
        LLInventoryModel::EXCLUDE_TRASH, broken_predicate);
    if (!broken_items.empty())
    {
        // A complete COF folder does not imply its linked targets were fetched.
        // Resolve real item metadata once before declaring these links missing.
        uuid_vec_t targets;
        for (const auto& item : broken_items)
        {
            if (item->getActualType() != LLAssetType::AT_LINK) continue;
            const LLUUID target = item->getLinkedUUID();
            if (target.notNull() && std::find(targets.begin(), targets.end(), target) == targets.end())
                targets.push_back(target);
        }
        if (targets.empty() || targets.size() > 256) return;
        if (mLegacyCOFLinkFetchAttempted)
        {
            // Inventory metadata may remain unavailable (for example for a
            // library item). Only accept real simulator-worn body-part assets
            // whose item IDs exactly match these unresolved COF link targets.
            mLegacyBodyPartUnresolvedTargets = targets;
        }
        else
        {
            mLegacyCOFLinkFetchAttempted = true;
            mLegacyCOFLinkFetchActive = true;
            mLegacyBodyPartCOFVersion = category->getVersion();
            ++mLegacyWearablesGeneration;
            mLegacyWearablesSession = gAgent.getSessionID();
            mLegacyWearablesAvatar = gAgentAvatarp->getID();
            // This observer only issues the standard item fetch here; no observer
            // pointer is registered or retained by the asynchronous HTTP request.
            LLInventoryFetchItemsObserver fetcher(targets);
            fetcher.startFetch();
            LL_WARNS("LocalBake") << "COF broken-link target fetch requested; targets=" << targets.size() << LL_ENDL;
            const U64 generation = mLegacyWearablesGeneration;
            doAfterInterval([generation]() { gAgentWearables.retryLegacyCOFLinkFetch(generation, 0); }, 8.f);
            return;
        }
    }
    // Never hide a failed COF asset download. Recover only body parts absent
    // from the complete COF, using real simulator worn-item assets, not defaults.
    LLInventoryModel::cat_array_t categories;
    LLInventoryModel::item_array_t items;
    LLFindWearables predicate;
    gInventory.collectDescendentsIf(cof, categories, items, LLInventoryModel::EXCLUDE_TRASH, predicate);
    for (const auto& item : items)
    {
        const S32 type = item->getWearableType();
        if (type >= 0 && type < 4 && (missing & (1u << type)))
        {
            LL_WARNS_ONCE("LocalBake") << "Required body part referenced in COF but not loaded; type="
                << type << LL_ENDL;
            return;
        }
    }
    mLegacyBodyPartRecoveryAttempted = true;
    mLegacyBodyPartRecovery = true;
    mLegacyBodyPartMask = missing;
    mLegacyBodyPartCOFVersion = category->getVersion();
    ++mLegacyWearablesGeneration;
    mLegacyWearablesSession = gAgent.getSessionID();
    mLegacyWearablesAvatar = gAgentAvatarp->getID();
    mInitialWearablesUpdateReceived = false;
    mLegacyWearablesWaiting = true;
    mLegacyWearablesRequests = 0;
    LL_WARNS("LocalBake") << "COF missing required body-part references; simulator repair requested; mask="
        << static_cast<U32>(missing) << LL_ENDL;
    sendLegacyWearablesRequest();
}

void LLAgentWearables::retryLegacyCOFLinkFetch(U64 generation, unsigned attempt)
{
    if (!mLegacyCOFLinkFetchActive || !isLegacyInitialWearablesCurrent(generation)) return;
    LLInventoryModel::cat_array_t categories;
    LLInventoryModel::item_array_t broken;
    LLFindBrokenLinks predicate;
    gInventory.collectDescendentsIf(LLAppearanceMgr::instance().getCOF(), categories, broken,
        LLInventoryModel::EXCLUDE_TRASH, predicate);
    if (broken.empty())
    {
        mLegacyCOFLinkFetchActive = false;
        LL_WARNS("LocalBake") << "COF link targets resolved; reloading existing outfit" << LL_ENDL;
        LLAppearanceMgr::instance().updateAppearanceFromCOF(false, false);
        return;
    }
    if (!mLegacyCOFLibraryFetchAttempted)
    {
        mLegacyCOFLibraryFetchAttempted = true;
        // Unknown targets initially went to personal inventory. Try the grid's
        // read-only library once before interpreting unresolved links as missing.
        requestLegacyCOFLibraryLinks(broken, generation);
    }
    if (attempt >= 3)
    {
        mLegacyCOFLinkFetchActive = false;
        LL_WARNS("LocalBake") << "COF link targets still unavailable after bounded fetch; links preserved; remaining="
            << broken.size() << LL_ENDL;
        recoverLegacyBodyPartsFromSimulator();
        return;
    }
    doAfterInterval([generation, attempt]()
        { gAgentWearables.retryLegacyCOFLinkFetch(generation, attempt + 1); }, 8.f);
}

bool LLAgentWearables::startLegacyInitialWearables(bool cof_fetch_failed)
{
    // Called after COF fetch completion, or with an explicit failed-fetch result.
    if (!isAgentAvatarValid() || !gAssetStorage || !gAgentAvatarp->usesLegacyBakes()) return false;
    if (!mLegacyBodyPartRecovery && cofHasWearables(!cof_fetch_failed))
    {
        mLegacyInitialCOFFetchFailed = false;
        LL_INFOS_ONCE("LocalBake") << "Initial outfit uses COF; legacy snapshot fallback not selected" << LL_ENDL;
        return false;
    }
    mLegacyInitialCOFFetchFailed = cof_fetch_failed;
    if (mLegacyWearablesWaiting || mLegacyWearablesLoad || mWearablesLoaded) return true;
    ++mLegacyWearablesGeneration;
    mLegacyWearablesSession = gAgent.getSessionID();
    mLegacyWearablesAvatar = gAgentAvatarp->getID();
    mInitialWearablesUpdateReceived = false;
    mLegacyWearablesWaiting = true;
    mLegacyWearablesRequests = 0;
    notifyLoadingStarted();
    sendLegacyWearablesRequest();
    return true;
}

void LLAgentWearables::sendLegacyWearablesRequest()
{
    if (!isLegacyInitialWearablesCurrent(mLegacyWearablesGeneration) || !gMessageSystem) return;
    gMessageSystem->newMessageFast(_PREHASH_AgentWearablesRequest);
    gMessageSystem->nextBlockFast(_PREHASH_AgentData);
    gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
    gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
    gAgent.sendReliableMessage();
    ++mLegacyWearablesRequests;
    const U64 generation = mLegacyWearablesGeneration;
    doAfterInterval([generation]() { gAgentWearables.retryLegacyWearablesRequest(generation); }, 8.f);
}

void LLAgentWearables::retryLegacyWearablesRequest(U64 generation)
{
    if (!isLegacyInitialWearablesCurrent(generation) || !mLegacyWearablesWaiting) return;
    if (!mLegacyBodyPartRecovery && cofHasWearables(!mLegacyInitialCOFFetchFailed))
    {
        mLegacyWearablesWaiting = false;
        mLegacyInitialCOFFetchFailed = false;
        ++mLegacyWearablesGeneration;
        LLAppearanceMgr::instance().updateAppearanceFromCOF();
        return;
    }
    if (mLegacyWearablesRequests < 3)
    {
        sendLegacyWearablesRequest();
        return;
    }
    mLegacyWearablesWaiting = false;
    mLegacyInitialCOFFetchFailed = false;
    ++mLegacyWearablesGeneration;
    notifyLoadingFinished();
    LL_WARNS("LocalBake") << "Initial worn-item request timed out; existing outfit preserved" << LL_ENDL;
}

void LLAgentWearables::processAgentInitialWearablesUpdate(LLMessageSystem* msg, void**)
{
    LLAgentWearables& self = gAgentWearables;
    if (!self.mLegacyWearablesWaiting || mInitialWearablesUpdateReceived ||
        !self.isLegacyInitialWearablesCurrent(self.mLegacyWearablesGeneration) ||
        !gAgent.getRegion() || msg->getSender() != gAgent.getRegion()->getHost()) return;
    LLUUID agent, session;
    msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent);
    msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_SessionID, session);
    if (agent != gAgent.getID() || session != gAgent.getSessionID()) return;

    const S32 blocks = msg->getNumberOfBlocksFast(_PREHASH_WearableData);
    if (blocks < 4 || blocks > static_cast<S32>(MAX_CLOTHING_LAYERS + LLWearableType::WT_COUNT)) return;
    auto load = std::make_shared<LLTasiaLegacyWearablesLoad>();
    load->generation = self.mLegacyWearablesGeneration;
    for (S32 i = 0; i < blocks; ++i)
    {
        LLTasiaLegacyWearablesLoad::Entry entry{};
        msg->getU8Fast(_PREHASH_WearableData, _PREHASH_WearableType, entry.type, i);
        msg->getUUIDFast(_PREHASH_WearableData, _PREHASH_ItemID, entry.item, i);
        msg->getUUIDFast(_PREHASH_WearableData, _PREHASH_AssetID, entry.asset, i);
        // Some simulators include explicit unworn slots.
        if (entry.item.isNull() && entry.asset.isNull()) continue;
        load->entries.push_back(entry);
    }
    static_assert(LLWearableType::WT_SHAPE == 0 && LLWearableType::WT_SKIN == 1 &&
                  LLWearableType::WT_HAIR == 2 && LLWearableType::WT_EYES == 3,
                   "Legacy body-part validation must follow the wire types");
    if (self.mLegacyBodyPartRecovery)
    {
        std::array<U32, 4> counts{};
        for (const auto& entry : load->entries) if (entry.type < 4) ++counts[entry.type];
        LL_WARNS("LocalBake") << "Simulator worn body-part counts; shape=" << counts[0]
            << "; skin=" << counts[1] << "; hair=" << counts[2] << "; eyes=" << counts[3] << LL_ENDL;
    }
    if (!TasiaWearables::validSnapshot(load->entries, LLWearableType::WT_COUNT, MAX_CLOTHING_LAYERS))
    {
        const auto details = TasiaWearables::diagnoseSnapshot(load->entries, LLWearableType::WT_COUNT);
        LL_WARNS_ONCE("LocalBake") << "Rejected incomplete or ambiguous initial worn-item snapshot"
            << "; entries=" << details.entries << "; repair=" << self.mLegacyBodyPartRecovery
            << "; repair_mask=" << static_cast<unsigned>(self.mLegacyBodyPartMask)
            << "; null_items=" << details.null_items << "; null_assets=" << details.null_assets
            << "; null_body_item_mask=" << details.null_body_item_mask
            << "; null_body_asset_mask=" << details.null_body_asset_mask
            << "; invalid_types=" << details.invalid_types << "; duplicate_items=" << details.duplicate_items
            << "; shared_assets=" << details.shared_assets
            << "; shape=" << details.body[0] << "; skin=" << details.body[1]
            << "; hair=" << details.body[2] << "; eyes=" << details.body[3] << LL_ENDL;
        return;
    }
    if (!self.mLegacyBodyPartRecovery && cofHasWearables(!self.mLegacyInitialCOFFetchFailed))
    {
        self.mLegacyWearablesWaiting = false;
        self.mLegacyInitialCOFFetchFailed = false;
        ++self.mLegacyWearablesGeneration;
        LLAppearanceMgr::instance().updateAppearanceFromCOF();
        return;
    }
    if (self.mLegacyBodyPartRecovery)
    {
        auto& entries = load->entries;
        entries.erase(std::remove_if(entries.begin(), entries.end(), [&self](const LLTasiaLegacyWearablesLoad::Entry& entry)
        {
            return entry.type >= 4 || !(self.mLegacyBodyPartMask & (1u << entry.type));
        }), entries.end());
        if (!TasiaWearables::validBodyPartRepair(entries, self.mLegacyBodyPartMask,
                                                self.mLegacyBodyPartUnresolvedTargets))
        {
            self.mLegacyWearablesWaiting = false;
            self.mLegacyInitialCOFFetchFailed = false;
            ++self.mLegacyWearablesGeneration;
            self.mLegacyBodyPartRecovery = false;
            LL_WARNS("LocalBake") << "Simulator body-part snapshot does not match required COF targets; repair refused" << LL_ENDL;
            return;
        }
        if (!self.mLegacyBodyPartUnresolvedTargets.empty())
        {
            LL_WARNS("LocalBake") << "Simulator body-part items match unresolved COF targets; loading real assets; parts="
                << entries.size() << LL_ENDL;
        }
    }
    mInitialWearablesUpdateReceived = true;
    self.mLegacyWearablesWaiting = false;
    self.mLegacyWearablesLoad = load;
    LL_INFOS_ONCE("LocalBake") << "Initial worn-item snapshot accepted; entries=" << load->entries.size() << LL_ENDL;
    load->remaining = load->entries.size();
    load->wearables.resize(load->remaining, nullptr);
    const U64 generation = load->generation;
    // Asset callbacks are bounded by the session and a deadline as well as the
    // asset-storage retry limit. Never create default body parts on failure.
    doAfterInterval([generation]()
    {
        LLAgentWearables& owner = gAgentWearables;
        if (owner.isLegacyInitialWearablesCurrent(generation) && owner.mLegacyWearablesLoad)
        {
            ++owner.mLegacyWearablesGeneration;
            owner.mLegacyWearablesLoad.reset();
            owner.mLegacyInitialCOFFetchFailed = false;
            owner.notifyLoadingFinished();
            LL_WARNS("LocalBake") << "Initial wearable download deadline exceeded; outfit not replaced" << LL_ENDL;
        }
    }, 60.f);
    for (std::size_t i = 0; i < load->entries.size(); ++i)
    {
        const auto& entry = load->entries[i];
        const auto type = static_cast<LLWearableType::EType>(entry.type);
        auto* callback = new LLTasiaLegacyWearableCallback{load, i};
        LLWearableList::instance().getAsset(entry.asset, std::string(), gAgentAvatarp.get(),
            LLWearableType::getInstance()->getAssetType(type), onLegacyWearableAssetArrived, callback,
            [generation]() { return gAgentWearables.isLegacyInitialWearablesCurrent(generation); });
    }
}

void LLAgentWearables::onLegacyWearableAssetArrived(LLViewerWearable* wearable, void* userdata)
{
    std::unique_ptr<LLTasiaLegacyWearableCallback> callback(static_cast<LLTasiaLegacyWearableCallback*>(userdata));
    LLAgentWearables& self = gAgentWearables;
    const auto& load = callback->load;
    if (!self.isLegacyInitialWearablesCurrent(load->generation) || self.mLegacyWearablesLoad != load) return;
    const auto& entry = load->entries[callback->index];
    if (!wearable || wearable->getType() != static_cast<LLWearableType::EType>(entry.type) ||
        wearable->getAssetID() != entry.asset)
    {
        load->failed = true;
    }
    else
    {
        load->wearables[callback->index] = wearable;
    }
    if (--load->remaining == 0) self.finishLegacyInitialWearables();
}

void LLAgentWearables::finishLegacyInitialWearables()
{
    const auto load = mLegacyWearablesLoad;
    if (!load || load->remaining != 0 || !isLegacyInitialWearablesCurrent(load->generation)) return;
    mLegacyWearablesLoad.reset();
    ++mLegacyWearablesGeneration;
    if (!mLegacyBodyPartRecovery && cofHasWearables(!mLegacyInitialCOFFetchFailed))
    {
        mLegacyInitialCOFFetchFailed = false;
        LLAppearanceMgr::instance().updateAppearanceFromCOF();
        return;
    }
    mLegacyInitialCOFFetchFailed = false;
    if (load->failed)
    {
        notifyLoadingFinished();
        LL_WARNS("LocalBake") << "Initial wearable asset unavailable; outfit not partially applied" << LL_ENDL;
        return;
    }
    if (mLegacyBodyPartRecovery && !TasiaWearables::validBodyPartRepair(
            load->entries, mLegacyBodyPartMask, mLegacyBodyPartUnresolvedTargets))
    {
        notifyLoadingFinished();
        LL_WARNS("LocalBake") << "Body-part repair target validation failed before commit; outfit preserved" << LL_ENDL;
        return;
    }
    // Validate exactly one of each required body part before committing.
    std::array<unsigned, 4> body{};
    for (const auto& entry : load->entries)
    {
        if (entry.type < 4) ++body[entry.type];
    }
    for (U8 type = 0; type < 4; ++type) if (body[type] !=
        (mLegacyBodyPartRecovery ? static_cast<unsigned>((mLegacyBodyPartMask >> type) & 1u) : 1u))
    {
        LL_WARNS("LocalBake") << "Legacy snapshot missing required body part; aborting commit" << LL_ENDL;
        notifyLoadingFinished();
        return;
    }
    if ((!mLegacyBodyPartRecovery && load->entries.size() < 4) ||
        load->entries.empty() || load->entries.size() > 4 + MAX_CLOTHING_LAYERS)
    {
        LL_WARNS("LocalBake") << "Legacy snapshot size out of bounds; aborting commit" << LL_ENDL;
        notifyLoadingFinished();
        return;
    }
    // All assets passed validation. Replace only client-side worn state, with
    // no inventory creation, COF link edits, autosave or AgentIsNowWearing.
    if (!mLegacyBodyPartRecovery) mWearableDatas.clear();
    for (std::size_t i = 0; i < load->entries.size(); ++i)
    {
        const auto& entry = load->entries[i];
        LLViewerWearable* wearable = load->wearables[i];
        wearable->setItemID(entry.item);
        if (LLViewerInventoryItem* item = gInventory.getItem(entry.item))
        {
            wearable->setPermissions(item->getPermissions());
        }
        pushWearable(static_cast<LLWearableType::EType>(entry.type), wearable, false);
    }
    for (LLViewerWearable* wearable : load->wearables) LLWearableData::wearableUpdated(wearable, false);
    mWearablesLoaded = true;
    gAgentAvatarp->setCompositeUpdatesEnabled(true);
    gAgentAvatarp->writeWearablesToAvatar();
    gAgentAvatarp->updateVisualParams();
    gAgentAvatarp->invalidateAll();
    gAgentAvatarp->onFirstTEMessageReceived();
    notifyLoadingFinished();
    LL_INFOS_ONCE("LocalBake") << "Initial legacy wearables loaded; local bake requested" << LL_ENDL;
    if (mLegacyBodyPartRecovery)
    {
        LL_WARNS("LocalBake") << "Simulator body-part repair applied; mask="
            << static_cast<U32>(mLegacyBodyPartMask) << LL_ENDL;
        mLegacyBodyPartRecovery = false;
    }
}

/**
 * @brief Construct a callback for dealing with the wearables.
 *
 * Would like to pass the agent in here, but we can't safely
 * count on it being around later.  Just use gAgent directly.
 * @param cb callback to execute on completion (? unused ?)
 * @param type Type for the wearable in the agent
 * @param wearable The wearable data.
 * @param todo Bitmask of actions to take on completion.
 */
LLAgentWearables::AddWearableToAgentInventoryCallback::AddWearableToAgentInventoryCallback(
	LLPointer<LLRefCount> cb, LLWearableType::EType type, U32 index, LLViewerWearable* wearable, U32 todo, const std::string description) :
	mType(type),
	mIndex(index),
	mWearable(wearable),
	mTodo(todo),
	mCB(cb),
	mDescription(description)
{
	LL_INFOS() << "constructor" << LL_ENDL;
}

void LLAgentWearables::AddWearableToAgentInventoryCallback::fire(const LLUUID& inv_item)
{
	if (inv_item.isNull())
		return;

	gAgentWearables.addWearabletoAgentInventoryDone(mType, mIndex, inv_item, mWearable);

	/*
	 * Do this for every one in the loop
	 */
	if (mTodo & CALL_MAKENEWOUTFITDONE)
	{
		gAgentWearables.makeNewOutfitDone(mType, mIndex);
	}
	if (mTodo & CALL_WEARITEM)
	{
		LLAppearanceMgr::instance().addCOFItemLink(inv_item,
			new LLUpdateAppearanceAndEditWearableOnDestroy(inv_item), mDescription);
		editWearable(inv_item);
	}
}

void LLAgentWearables::addWearabletoAgentInventoryDone(const LLWearableType::EType type,
	const U32 index,
	const LLUUID& item_id,
	LLViewerWearable* wearable)
{
	LL_INFOS() << "type " << type << " index " << index << " item " << item_id.asString() << LL_ENDL;

	if (item_id.isNull())
		return;

	LLUUID old_item_id = getWearableItemID(type, index);

	if (wearable)
	{
		wearable->setItemID(item_id);

		if (old_item_id.notNull())
		{
			gInventory.addChangedMask(LLInventoryObserver::LABEL, old_item_id);
			setWearable(type, index, wearable);
		}
		else
		{
			pushWearable(type, wearable);
		}
	}

	gInventory.addChangedMask(LLInventoryObserver::LABEL, item_id);

	LLViewerInventoryItem* item = gInventory.getItem(item_id);
	if (item && wearable)
	{
		// We're changing the asset id, so we both need to set it
		// locally via setAssetUUID() and via setTransactionID() which
		// will be decoded on the server. JC
		item->setAssetUUID(wearable->getAssetID());
		item->setTransactionID(wearable->getTransactionID());
		gInventory.addChangedMask(LLInventoryObserver::INTERNAL, item_id);
		item->updateServer(false);
	}
	gInventory.notifyObservers();
}

void LLAgentWearables::saveWearable(const LLWearableType::EType type, const U32 index,
	const std::string new_name)
{
	LLViewerWearable* old_wearable = getViewerWearable(type, index);
	if (!old_wearable) return;
	bool name_changed = !new_name.empty() && (new_name != old_wearable->getName());
	if (name_changed || old_wearable->isDirty() || old_wearable->isOldVersion())
	{
		LLUUID old_item_id = old_wearable->getItemID();
		LLViewerWearable* new_wearable = LLWearableList::instance().createCopy(old_wearable);
		new_wearable->setItemID(old_item_id); // should this be in LLViewerWearable::copyDataFrom()?
		setWearable(type, index, new_wearable);

		// old_wearable may still be referred to by other inventory items. Revert
		// unsaved changes so other inventory items aren't affected by the changes
		// that were just saved.
		old_wearable->revertValues();

		LLInventoryItem* item = gInventory.getItem(old_item_id);
		if (item)
		{
			std::string item_name = item->getName();
			if (name_changed)
			{
				LL_INFOS() << "saveWearable changing name from " << item->getName() << " to " << new_name << LL_ENDL;
				item_name = new_name;
			}
			// Update existing inventory item
			LLPointer<LLViewerInventoryItem> template_item =
				new LLViewerInventoryItem(item->getUUID(),
					item->getParentUUID(),
					item->getPermissions(),
					new_wearable->getAssetID(),
					new_wearable->getAssetType(),
					item->getInventoryType(),
					item_name,
					item->getDescription(),
					item->getSaleInfo(),
					item->getFlags(),
					item->getCreationDate());
			template_item->setTransactionID(new_wearable->getTransactionID());
			update_inventory_item(template_item, gAgentAvatarp->mEndCustomizeCallback);
		}
		else
		{
			// Add a new inventory item (shouldn't ever happen here)
			U32 todo = AddWearableToAgentInventoryCallback::CALL_NONE;
			LLPointer<LLInventoryCallback> cb =
				new AddWearableToAgentInventoryCallback(
					LLPointer<LLRefCount>(NULL),
					type,
					index,
					new_wearable,
					todo);
			addWearableToAgentInventory(cb, new_wearable);
			return;
		}

		gAgentAvatarp->wearableUpdated(type);
	}
}

void LLAgentWearables::saveWearableAs(const LLWearableType::EType type,
	const U32 index,
	const std::string& new_name,
	const std::string& description,
	bool save_in_lost_and_found)
{
	if (!isWearableCopyable(type, index))
	{
		LL_WARNS() << "LLAgent::saveWearableAs() not copyable." << LL_ENDL;
		return;
	}
	LLViewerWearable* old_wearable = getViewerWearable(type, index);
	if (!old_wearable)
	{
		LL_WARNS() << "LLAgent::saveWearableAs() no old wearable." << LL_ENDL;
		return;
	}

	LLInventoryItem* item = gInventory.getItem(getWearableItemID(type, index));
	if (!item)
	{
		LL_WARNS() << "LLAgent::saveWearableAs() no inventory item." << LL_ENDL;
		return;
	}
	std::string trunc_name(new_name);
	LLStringUtil::truncate(trunc_name, DB_INV_ITEM_NAME_STR_LEN);
	LLViewerWearable* new_wearable = LLWearableList::instance().createCopy(
		old_wearable,
		trunc_name);

	LLPointer<LLInventoryCallback> cb =
		new AddWearableToAgentInventoryCallback(
			LLPointer<LLRefCount>(NULL),
			type,
			index,
			new_wearable,
			AddWearableToAgentInventoryCallback::CALL_WEARITEM,
			description
		);
	LLUUID category_id;
	if (save_in_lost_and_found)
	{
		category_id = gInventory.findCategoryUUIDForType(
			LLFolderType::FT_LOST_AND_FOUND);
	}
	else
	{
		// put in same folder as original
		category_id = item->getParentUUID();
	}

	copy_inventory_item(
		gAgent.getID(),
		item->getPermissions().getOwner(),
		item->getUUID(),
		category_id,
		new_name,
		cb);

	// old_wearable may still be referred to by other inventory items. Revert
	// unsaved changes so other inventory items aren't affected by the changes
	// that were just saved.
	old_wearable->revertValuesWithoutUpdate();
}

void LLAgentWearables::revertWearable(const LLWearableType::EType type, const U32 index)
{
	LLViewerWearable* wearable = getViewerWearable(type, index);
	llassert(wearable);
	if (wearable)
	{
		wearable->revertValues();
	}
}

void LLAgentWearables::saveAllWearables()
{
	//if (!gInventory.isLoaded())
	//{
	//  return;
	//}

	for (S32 i = 0; i < LLWearableType::WT_COUNT; i++)
	{
		for (U32 j = 0; j < getWearableCount((LLWearableType::EType)i); j++)
			saveWearable((LLWearableType::EType)i, j);
	}
}

// Called when the user changes the name of a wearable inventory item that is currently being worn.
void LLAgentWearables::setWearableName(const LLUUID& item_id, const std::string& new_name)
{
	for (S32 i = 0; i < LLWearableType::WT_COUNT; i++)
	{
		for (U32 j = 0; j < getWearableCount((LLWearableType::EType)i); j++)
		{
			LLUUID curr_item_id = getWearableItemID((LLWearableType::EType)i, j);
			if (curr_item_id == item_id)
			{
				LLViewerWearable* old_wearable = getViewerWearable((LLWearableType::EType)i, j);
				llassert(old_wearable);
				if (!old_wearable) continue;

				std::string old_name = old_wearable->getName();
				old_wearable->setName(new_name);
				LLViewerWearable* new_wearable = LLWearableList::instance().createCopy(old_wearable);
				new_wearable->setItemID(item_id);
				LLInventoryItem* item = gInventory.getItem(item_id);
				if (item)
				{
					new_wearable->setPermissions(item->getPermissions());
				}
				old_wearable->setName(old_name);

				setWearable((LLWearableType::EType)i, j, new_wearable);
				break;
			}
		}
	}
}

bool LLAgentWearables::isWearableModifiable(LLWearableType::EType type, U32 index) const
{
	LLUUID item_id = getWearableItemID(type, index);
	return item_id.notNull() ? isWearableModifiable(item_id) : false;
}

bool LLAgentWearables::isWearableModifiable(const LLUUID& item_id) const
{
	const LLUUID& linked_id = gInventory.getLinkedItemID(item_id);
	if (linked_id.notNull())
	{
		LLInventoryItem* item = gInventory.getItem(linked_id);
		if (item && item->getPermissions().allowModifyBy(gAgent.getID(),
			gAgent.getGroupID()))
		{
			return true;
		}
	}
	return false;
}

bool LLAgentWearables::isWearableCopyable(LLWearableType::EType type, U32 index) const
{
	LLUUID item_id = getWearableItemID(type, index);
	if (!item_id.isNull())
	{
		LLInventoryItem* item = gInventory.getItem(item_id);
		if (item && item->getPermissions().allowCopyBy(gAgent.getID(),
			gAgent.getGroupID()))
		{
			return true;
		}
	}
	return false;
}

LLInventoryItem* LLAgentWearables::getWearableInventoryItem(LLWearableType::EType type, U32 index)
{
	LLUUID item_id = getWearableItemID(type, index);
	LLInventoryItem* item = NULL;
	if (item_id.notNull())
	{
		item = gInventory.getItem(item_id);
	}
	return item;
}

const S32 LLAgentWearables::getWearableIdxFromItem(const LLViewerInventoryItem* item) const
{
    if (!item) return -1;
    if (!item->isWearableType()) return -1;

    LLWearableType::EType type = item->getWearableType();
    U32 wearable_count = getWearableCount(type);
    if (0 == wearable_count) return -1;

    // Match by linked inventory item id so the correct copy is found when several
    // worn wearables of this type share the same asset.
    const LLUUID item_id = gInventory.getLinkedItemID(item->getUUID());
    for (U32 i = 0; i < wearable_count; ++i)
    {
        const LLViewerWearable* wearable = getViewerWearable(type, i);
        if (!wearable) continue;
        if (wearable->getItemID() == item_id) return i;
    }

    // Fall back to asset id for items whose inventory id can't be resolved.
    const LLUUID& asset_id = item->getAssetUUID();
    for (U32 i = 0; i < wearable_count; ++i)
    {
        const LLViewerWearable* wearable = getViewerWearable(type, i);
        if (!wearable) continue;
        if (wearable->getAssetID() == asset_id) return i;
    }

    return -1;
}
const LLViewerWearable* LLAgentWearables::getWearableFromItemID(const LLUUID& item_id) const
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	for (S32 i = 0; i < LLWearableType::WT_COUNT; i++)
	{
		for (U32 j = 0; j < getWearableCount((LLWearableType::EType)i); j++)
		{
			const LLViewerWearable* curr_wearable = getViewerWearable((LLWearableType::EType)i, j);
			if (curr_wearable && (curr_wearable->getItemID() == base_item_id))
			{
				return curr_wearable;
			}
		}
	}
	return NULL;
}

LLViewerWearable* LLAgentWearables::getWearableFromItemID(const LLUUID& item_id)
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	for (S32 i = 0; i < LLWearableType::WT_COUNT; i++)
	{
		for (U32 j = 0; j < getWearableCount((LLWearableType::EType)i); j++)
		{
			LLViewerWearable* curr_wearable = getViewerWearable((LLWearableType::EType)i, j);
			if (curr_wearable && (curr_wearable->getItemID() == base_item_id))
			{
				return curr_wearable;
			}
		}
	}
	return NULL;
}

LLViewerWearable* LLAgentWearables::getWearableFromAssetID(const LLUUID& asset_id)
{
	for (S32 i = 0; i < LLWearableType::WT_COUNT; i++)
	{
		for (U32 j = 0; j < getWearableCount((LLWearableType::EType)i); j++)
		{
			LLViewerWearable* curr_wearable = getViewerWearable((LLWearableType::EType)i, j);
			if (curr_wearable && (curr_wearable->getAssetID() == asset_id))
			{
				return curr_wearable;
			}
		}
	}
	return NULL;
}

LLViewerWearable* LLAgentWearables::getViewerWearable(const LLWearableType::EType type, U32 index /*= 0*/)
{
	return dynamic_cast<LLViewerWearable*> (getWearable(type, index));
}

const LLViewerWearable* LLAgentWearables::getViewerWearable(const LLWearableType::EType type, U32 index /*= 0*/) const
{
	return dynamic_cast<const LLViewerWearable*> (getWearable(type, index));
}

// static
bool LLAgentWearables::selfHasWearable(LLWearableType::EType type)
{
	return (gAgentWearables.getWearableCount(type) > 0);
}

// virtual
void LLAgentWearables::wearableUpdated(LLWearable* wearable, bool removed)
{
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->wearableUpdated(wearable->getType());
	}

	LLWearableData::wearableUpdated(wearable, removed);

	if (!removed)
	{
		LLViewerWearable* viewer_wearable = dynamic_cast<LLViewerWearable*>(wearable);
		viewer_wearable->refreshName();

		// Hack pt 2. If the wearable we just loaded has definition version 24,
		// then force a re-save of this wearable after slamming the version number to 22.
		// This number was incorrectly incremented for internal builds before release, and
		// this fix will ensure that the affected wearables are re-saved with the right version number.
		// the versions themselves are compatible. This code can be removed before release.
		if (wearable->getDefinitionVersion() == 24)
		{
			U32 index;
			if (getWearableIndex(wearable, index))
			{
				LL_INFOS() << "forcing wearable type " << wearable->getType() << " to version 22 from 24" << LL_ENDL;
				wearable->setDefinitionVersion(22);
				saveWearable(wearable->getType(), index);
			}
		}

		checkWearableAgainstInventory(viewer_wearable);
	}
}

const LLUUID LLAgentWearables::getWearableItemID(LLWearableType::EType type, U32 index) const
{
	const LLViewerWearable* wearable = getViewerWearable(type, index);
	if (wearable)
		return wearable->getItemID();
	else
		return LLUUID();
}

const LLUUID LLAgentWearables::getWearableAssetID(LLWearableType::EType type, U32 index) const
{
	const LLViewerWearable* wearable = getViewerWearable(type, index);
	if (wearable)
		return wearable->getAssetID();
	else
		return LLUUID();
}

bool LLAgentWearables::isWearingItem(const LLUUID& item_id) const
{
	return getWearableFromItemID(item_id) != nullptr;
}

void LLAgentWearables::addLocalTextureObject(const LLWearableType::EType wearable_type, const LLAvatarAppearanceDefines::ETextureIndex texture_type, U32 wearable_index)
{
	LLViewerWearable* wearable = getViewerWearable((LLWearableType::EType)wearable_type, wearable_index);
	if (!wearable)
	{
		LL_ERRS() << "Tried to add local texture object to invalid wearable with type " << wearable_type << " and index " << wearable_index << LL_ENDL;
		return;
	}
	LLLocalTextureObject lto;
	wearable->setLocalTextureObject(texture_type, lto);
}

class OnWearableItemCreatedCB : public LLInventoryCallback
{
public:
	OnWearableItemCreatedCB() :
		mWearablesAwaitingItems(LLWearableType::WT_COUNT, NULL)
	{
		LL_INFOS() << "created callback" << LL_ENDL;
	}
	/* virtual */ void fire(const LLUUID& inv_item)
	{
		LL_INFOS() << "One item created " << inv_item.asString() << LL_ENDL;
		LLConstPointer<LLInventoryObject> item = gInventory.getItem(inv_item);
		mItemsToLink.push_back(item);
		updatePendingWearable(inv_item);
	}
	~OnWearableItemCreatedCB()
	{
		LL_INFOS() << "All items created" << LL_ENDL;
		LLPointer<LLInventoryCallback> link_waiter = new LLUpdateAppearanceOnDestroy;
		link_inventory_array(LLAppearanceMgr::instance().getCOF(),
			mItemsToLink,
			link_waiter);
	}
	void addPendingWearable(LLViewerWearable* wearable)
	{
		if (!wearable)
		{
			LL_WARNS() << "no wearable" << LL_ENDL;
			return;
		}
		LLWearableType::EType type = wearable->getType();
		if (type < LLWearableType::WT_COUNT)
		{
			mWearablesAwaitingItems[type] = wearable;
		}
		else
		{
			LL_WARNS() << "invalid type " << type << LL_ENDL;
		}
	}
	void updatePendingWearable(const LLUUID& inv_item)
	{
		LLViewerInventoryItem* item = gInventory.getItem(inv_item);
		if (!item)
		{
			LL_WARNS() << "no item found" << LL_ENDL;
			return;
		}
		if (!item->isWearableType())
		{
			LL_WARNS() << "non-wearable item found" << LL_ENDL;
			return;
		}
		if (item && item->isWearableType())
		{
			LLWearableType::EType type = item->getWearableType();
			if (type < LLWearableType::WT_COUNT)
			{
				LLViewerWearable* wearable = mWearablesAwaitingItems[type];
				if (wearable)
					wearable->setItemID(inv_item);
			}
			else
			{
				LL_WARNS() << "invalid wearable type " << type << LL_ENDL;
			}
		}
	}

private:
	LLInventoryObject::const_object_list_t mItemsToLink;
	std::vector<LLViewerWearable*> mWearablesAwaitingItems;
};

void LLAgentWearables::createStandardWearables()
{
	LL_WARNS() << "Creating standard wearables" << LL_ENDL;

	if (!isAgentAvatarValid()) return;

	constexpr bool create[LLWearableType::WT_COUNT] =
	{
		true,  //LLWearableType::WT_SHAPE
		true,  //LLWearableType::WT_SKIN
		true,  //LLWearableType::WT_HAIR
		true,  //LLWearableType::WT_EYES
		true,  //LLWearableType::WT_SHIRT
		true,  //LLWearableType::WT_PANTS
		true,  //LLWearableType::WT_SHOES
		true,  //LLWearableType::WT_SOCKS
		false, //LLWearableType::WT_JACKET
		false, //LLWearableType::WT_GLOVES
		true,  //LLWearableType::WT_UNDERSHIRT
		true,  //LLWearableType::WT_UNDERPANTS
		false  //LLWearableType::WT_SKIRT
	};

	LLPointer<LLInventoryCallback> cb = new OnWearableItemCreatedCB;
	for (S32 i = 0; i < LLWearableType::WT_COUNT; i++)
	{
		if (create[i])
		{
			llassert(getWearableCount((LLWearableType::EType)i) == 0);
			LLViewerWearable* wearable = LLWearableList::instance().createNewWearable((LLWearableType::EType)i, gAgentAvatarp);
			((OnWearableItemCreatedCB*)(&(*cb)))->addPendingWearable(wearable);
			// no need to update here...
			LLUUID category_id = LLUUID::null;
			create_inventory_wearable(gAgent.getID(),
				gAgent.getSessionID(),
				category_id,
				wearable->getTransactionID(),
				wearable->getName(),
				wearable->getDescription(),
				wearable->getAssetType(),
				wearable->getType(),
				wearable->getPermissions().getMaskNextOwner(),
				cb);
		}
	}
}

// We no longer need this message in the current viewer, but send
// it for now to maintain compatibility with release viewers. Can
// remove this function once the SH-3455 changesets are universally deployed.
void LLAgentWearables::sendDummyAgentWearablesUpdate()
{
	// Linden's compatibility placeholders are not real inventory items.
	// OpenSim persists AgentIsNowWearing, so never send dummy IDs there.
	if (LLGridManager::getInstance()->isInOpenSim())
	{
		return;
	}
	LL_DEBUGS("Avatar") << "sendAgentWearablesUpdate()" << LL_ENDL;

	// Send the AgentIsNowWearing
	gMessageSystem->newMessageFast(_PREHASH_AgentIsNowWearing);

	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());

	// Send 4 standardized nonsense item ids (same as returned by the modified sim, not that it especially matters).
	gMessageSystem->nextBlockFast(_PREHASH_WearableData);
	gMessageSystem->addU8Fast(_PREHASH_WearableType, U8(1));
	gMessageSystem->addUUIDFast(_PREHASH_ItemID, LLUUID("db5a4e5f-9da3-44c8-992d-1181c5795498"));

	gMessageSystem->nextBlockFast(_PREHASH_WearableData);
	gMessageSystem->addU8Fast(_PREHASH_WearableType, U8(2));
	gMessageSystem->addUUIDFast(_PREHASH_ItemID, LLUUID("6969c7cc-f72f-4a76-a19b-c293cce8ce4f"));

	gMessageSystem->nextBlockFast(_PREHASH_WearableData);
	gMessageSystem->addU8Fast(_PREHASH_WearableType, U8(3));
	gMessageSystem->addUUIDFast(_PREHASH_ItemID, LLUUID("7999702b-b291-48f9-8903-c91dfb828408"));

	gMessageSystem->nextBlockFast(_PREHASH_WearableData);
	gMessageSystem->addU8Fast(_PREHASH_WearableType, U8(4));
	gMessageSystem->addUUIDFast(_PREHASH_ItemID, LLUUID("566cb59e-ef60-41d7-bfa6-e0f293fbea40"));

	gAgent.sendReliableMessage();
}

void LLAgentWearables::makeNewOutfitDone(S32 type, U32 index)
{
	LLUUID first_item_id = getWearableItemID((LLWearableType::EType)type, index);
	// Open the inventory and select the first item we added.
	if (first_item_id.notNull())
	{
		LLInventoryPanel* active_panel = LLInventoryPanel::getActiveInventoryPanel();
		if (active_panel)
		{
			active_panel->setSelection(first_item_id, TAKE_FOCUS_NO);
		}
	}
}

void LLAgentWearables::addWearableToAgentInventory(LLPointer<LLInventoryCallback> cb,
	LLViewerWearable* wearable,
	const LLUUID& category_id,
	bool notify)
{
	create_inventory_wearable(gAgent.getID(),
		gAgent.getSessionID(),
		category_id,
		wearable->getTransactionID(),
		wearable->getName(),
		wearable->getDescription(),
		wearable->getAssetType(),
		wearable->getType(),
		wearable->getPermissions().getMaskNextOwner(),
		cb);
}

void LLAgentWearables::removeWearable(const LLWearableType::EType type, bool do_remove_all, U32 index)
{
	if (getWearableCount(type) == 0)
	{
		// no wearables to remove
		return;
	}

	if (do_remove_all)
	{
		removeWearableFinal(type, do_remove_all, index);
	}
	else
	{
		LLViewerWearable* old_wearable = getViewerWearable(type, index);

		if (old_wearable)
		{
			if (old_wearable->isDirty())
			{
				LLSD payload;
				payload["wearable_type"] = (S32)type;
				payload["wearable_index"] = (S32)index;
				// Bring up view-modal dialog: Save changes? Yes, No, Cancel
				LLNotificationsUtil::add("WearableSave", LLSD(), payload, &LLAgentWearables::onRemoveWearableDialog);
				return;
			}
			else
			{
				removeWearableFinal(type, do_remove_all, index);
			}
		}
	}
}

// static
bool LLAgentWearables::onRemoveWearableDialog(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLWearableType::EType type = (LLWearableType::EType)notification["payload"]["wearable_type"].asInteger();
	S32 index = (S32)notification["payload"]["wearable_index"].asInteger();
	switch (option)
	{
	case 0:  // "Save"
		gAgentWearables.saveWearable(type, index);
		gAgentWearables.removeWearableFinal(type, false, index);
		break;

	case 1:  // "Don't Save"
		gAgentWearables.removeWearableFinal(type, false, index);
		break;

	case 2: // "Cancel"
		break;

	default:
		llassert(0);
		break;
	}
	return false;
}

// Called by removeWearable() and onRemoveWearableDialog() to actually do the removal.
void LLAgentWearables::removeWearableFinal(const LLWearableType::EType type, bool do_remove_all, U32 index)
{
	//LLAgentDumper dumper("removeWearable");
	if (do_remove_all)
	{
		S32 max_entry = getWearableCount(type) - 1;
		for (S32 i = max_entry; i >= 0; i--)
		{
			LLViewerWearable* old_wearable = getViewerWearable(type, i);
			if (old_wearable)
			{
				eraseWearable(old_wearable);
				old_wearable->removeFromAvatar(true);
			}
		}
		clearWearableType(type);
	}
	else
	{
		LLViewerWearable* old_wearable = getViewerWearable(type, index);

		if (old_wearable)
		{
			eraseWearable(old_wearable);
			old_wearable->removeFromAvatar(true);
		}
	}

	gInventory.notifyObservers();
}

// Assumes existing wearables are not dirty.
void LLAgentWearables::setWearableOutfit(const LLInventoryItem::item_array_t& items,
	const std::vector< LLViewerWearable* >& wearables)
{
    // A newer COF outfit supersedes any asynchronous missing-part repair.
    if (mLegacyBodyPartRecovery || mLegacyCOFLinkFetchActive || mLegacyInitialCOFFetchFailed)
    {
        ++mLegacyWearablesGeneration;
        mLegacyWearablesWaiting = false;
        mLegacyWearablesLoad.reset();
        mLegacyBodyPartRecovery = false;
        mLegacyCOFLinkFetchActive = false;
        mLegacyInitialCOFFetchFailed = false;
    }
	LL_INFOS() << "setWearableOutfit() start" << LL_ENDL;

	auto count = wearables.size();
	llassert(items.size() == count);

	// Check for whether outfit already matches the one requested
	S32 matched = 0, mismatched = 0;
	const S32 arr_size = LLWearableType::WT_COUNT;
	S32 type_counts[arr_size];
	bool update_inventory{ false };
	std::fill(type_counts, type_counts + arr_size, 0);
	for (size_t i = 0; i < count; i++)
	{
		LLViewerWearable* new_wearable = wearables[i];
		LLPointer<LLInventoryItem> new_item = items[i];

		const LLWearableType::EType type = new_wearable->getType();
		if (type < 0 || type >= LLWearableType::WT_COUNT)
		{
			LL_WARNS() << "invalid type " << type << LL_ENDL;
			mismatched++;
			continue;
		}
		S32 index = type_counts[type];
		type_counts[type]++;

		LLViewerWearable* curr_wearable = dynamic_cast<LLViewerWearable*>(getWearable(type, index));
		if (!new_wearable || !curr_wearable ||
			new_wearable->getAssetID() != curr_wearable->getAssetID())
		{
			LL_DEBUGS("Avatar") << "mismatch, type " << type << " index " << index
				<< " names " << (curr_wearable ? curr_wearable->getName() : "NONE") << ","
				<< " names " << (new_wearable ? new_wearable->getName() : "NONE") << LL_ENDL;
			mismatched++;
			continue;
		}

		// Update only inventory in this case - ordering of wearables with the same asset id has no effect.
		// Updating wearables in this case causes the two-alphas error in MAINT-4158.
		// We should actually disallow wearing two wearables with the same asset id.
		if (curr_wearable->getName() != new_item->getName() ||
			curr_wearable->getItemID() != new_item->getUUID())
		{
			LL_DEBUGS("Avatar") << "mismatch on name or inventory id, names "
				<< curr_wearable->getName() << " vs " << new_item->getName()
				<< " item ids " << curr_wearable->getItemID() << " vs " << new_item->getUUID()
				<< LL_ENDL;
			update_inventory = true;
			continue;
		}
		// If we got here, everything matches.
		matched++;
	}
	LL_DEBUGS("Avatar") << "matched " << matched << " mismatched " << mismatched << LL_ENDL;
	for (S32 j = 0; j < LLWearableType::WT_COUNT; j++)
	{
		LLWearableType::EType type = (LLWearableType::EType)j;
		if (getWearableCount(type) != type_counts[j])
		{
			LL_DEBUGS("Avatar") << "count mismatch for type " << j << " current " << getWearableCount(j) << " requested " << type_counts[j] << LL_ENDL;
			mismatched++;
		}
	}
	if (mismatched == 0 && !update_inventory)
	{
		LL_DEBUGS("Avatar") << "no changes, bailing out" << LL_ENDL;
		notifyLoadingFinished();
		recoverLegacyBodyPartsFromSimulator();
		return;
	}

	// updating inventory
	LLWearableType* wearable_type_inst = LLWearableType::getInstance();

	// TODO: Removed check for ensuring that teens don't remove undershirt and underwear. Handle later
	// note: shirt is the first non-body part wearable item. Update if wearable order changes.
	// This loop should remove all clothing, but not any body parts
	for (S32 j = 0; j < (S32)LLWearableType::WT_COUNT; j++)
	{
		if (wearable_type_inst->getAssetType((LLWearableType::EType)j) == LLAssetType::AT_CLOTHING)
		{
			removeWearable((LLWearableType::EType)j, true, 0);
		}
	}

	for (S32 i = 0; i < count; i++)
	{
		LLViewerWearable* new_wearable = wearables[i];
		LLPointer<LLInventoryItem> new_item = items[i];

		llassert(new_wearable);
		if (new_wearable)
		{
			const LLWearableType::EType type = new_wearable->getType();

			LLUUID old_wearable_id = new_wearable->getItemID();
			new_wearable->setName(new_item->getName());
			new_wearable->setItemID(new_item->getUUID());

			if (wearable_type_inst->getAssetType(type) == LLAssetType::AT_BODYPART)
			{
				// exactly one wearable per body part
				setWearable(type, 0, new_wearable);
				if (old_wearable_id.notNull())
				{
					// we changed id before setting wearable, update old item manually
					// to complete the swap.
					gInventory.addChangedMask(LLInventoryObserver::LABEL, old_wearable_id);
				}
			}
			else
			{
				pushWearable(type, new_wearable);
			}

			constexpr bool removed = false;
			wearableUpdated(new_wearable, removed);
		}
	}

	gInventory.notifyObservers();

	if (mismatched == 0)
	{
		LL_DEBUGS("Avatar") << "inventory updated, wearable assets not changed, bailing out" << LL_ENDL;
		notifyLoadingFinished();
		recoverLegacyBodyPartsFromSimulator();
		return;
	}

	// updating agent avatar

	if (isAgentAvatarValid())
	{
		gAgentAvatarp->setCompositeUpdatesEnabled(true);

		// If we have not yet declouded, we may want to use
		// baked texture UUIDs sent from the first objectUpdate message
		// don't overwrite these. If we have already declouded, we've saved
		// these ids as the last known good textures and can invalidate without
		// re-clouding.
        if (!gAgentAvatarp->getHasMissingParts())
		{
			gAgentAvatarp->invalidateAll();
		}
	}

	// Start rendering & update the server
	mWearablesLoaded = true;

	notifyLoadingFinished();

	// Copy wearable params to avatar.
	gAgentAvatarp->writeWearablesToAvatar();

	// Then update the avatar based on the copied params.
	gAgentAvatarp->updateVisualParams();

	gAgentAvatarp->dumpAvatarTEs("setWearableOutfit");
	recoverLegacyBodyPartsFromSimulator();

	LL_DEBUGS("Avatar") << "setWearableOutfit() end" << LL_ENDL;
}


// User has picked "wear on avatar" from a menu.
void LLAgentWearables::setWearableItem(LLInventoryItem* new_item, LLViewerWearable* new_wearable, bool do_append)
{
	//LLAgentDumper dumper("setWearableItem");
	if (isWearingItem(new_item->getUUID()))
	{
		LL_WARNS() << "wearable " << new_item->getUUID() << " is already worn" << LL_ENDL;
		return;
	}

	const LLWearableType::EType type = new_wearable->getType();

	if (!do_append)
	{
		// Remove old wearable, if any
		// MULTI_WEARABLE: hardwired to 0
		LLViewerWearable* old_wearable = getViewerWearable(type, 0);
		if (old_wearable)
		{
			const LLUUID& old_item_id = old_wearable->getItemID();
			if ((old_wearable->getAssetID() == new_wearable->getAssetID()) &&
				(old_item_id == new_item->getUUID()))
			{
				LL_DEBUGS() << "No change to wearable asset and item: " << LLWearableType::getInstance()->getTypeName(type) << LL_ENDL;
				return;
			}

			if (old_wearable->isDirty())
			{
				// Bring up modal dialog: Save changes? Yes, No, Cancel
				LLSD payload;
				payload["item_id"] = new_item->getUUID();
				LLNotificationsUtil::add("WearableSave", LLSD(), payload, boost::bind(onSetWearableDialog, _1, _2, new_wearable));
				return;
			}
		}
	}

	setWearableFinal(new_item, new_wearable, do_append);
}

// static
bool LLAgentWearables::onSetWearableDialog(const LLSD& notification, const LLSD& response, LLViewerWearable* wearable)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLInventoryItem* new_item = gInventory.getItem(notification["payload"]["item_id"].asUUID());
	U32 index;
	if (!gAgentWearables.getWearableIndex(wearable, index))
	{
		LL_WARNS() << "Wearable not found" << LL_ENDL;
		delete wearable;
		return false;
	}
	if (!new_item)
	{
		delete wearable;
		return false;
	}

	switch (option)
	{
	case 0:  // "Save"
		gAgentWearables.saveWearable(wearable->getType(), index);
		gAgentWearables.setWearableFinal(new_item, wearable);
		break;

	case 1:  // "Don't Save"
		gAgentWearables.setWearableFinal(new_item, wearable);
		break;

	case 2: // "Cancel"
		break;

	default:
		llassert(0);
		break;
	}

	delete wearable;
	return false;
}

// Called from setWearableItem() and onSetWearableDialog() to actually set the wearable.
// MULTI_WEARABLE: unify code after null objects are gone.
void LLAgentWearables::setWearableFinal(LLInventoryItem* new_item, LLViewerWearable* new_wearable, bool do_append)
{
	const LLWearableType::EType type = new_wearable->getType();

	if (do_append && getWearableItemID(type, 0).notNull())
	{
		new_wearable->setItemID(new_item->getUUID());
		const bool trigger_updated = false;
		pushWearable(type, new_wearable, trigger_updated);
		LL_INFOS() << "Added additional wearable for type " << type
			<< " size is now " << getWearableCount(type) << LL_ENDL;
		checkWearableAgainstInventory(new_wearable);
	}
	else
	{
		// Replace the old wearable with a new one.
		llassert(new_item->getAssetUUID() == new_wearable->getAssetID());

		LLViewerWearable* old_wearable = getViewerWearable(type, 0);
		LLUUID old_item_id;
		if (old_wearable)
		{
			old_item_id = old_wearable->getItemID();
		}
		new_wearable->setItemID(new_item->getUUID());
		setWearable(type, 0, new_wearable);

		if (old_item_id.notNull())
		{
			gInventory.addChangedMask(LLInventoryObserver::LABEL, old_item_id);
			gInventory.notifyObservers();
		}
		LL_INFOS() << "Replaced current element 0 for type " << type
			<< " size is now " << getWearableCount(type) << LL_ENDL;
	}
}

// User has picked "remove from avatar" from a menu.
// static
void LLAgentWearables::userRemoveWearable(const LLWearableType::EType& type, const U32& index)
{
	if (!(type == LLWearableType::WT_SHAPE || type == LLWearableType::WT_SKIN || type == LLWearableType::WT_HAIR || type == LLWearableType::WT_EYES)) //&&
		//!((!gAgent.isTeen()) && (type==LLWearableType::WT_UNDERPANTS || type==LLWearableType::WT_UNDERSHIRT)))
	{
		gAgentWearables.removeWearable(type, false, index);
	}
}

//static
void LLAgentWearables::userRemoveWearablesOfType(const LLWearableType::EType& type)
{
	if (!(type == LLWearableType::WT_SHAPE || type == LLWearableType::WT_SKIN || type == LLWearableType::WT_HAIR || type == LLWearableType::WT_EYES)) //&&
		//!((!gAgent.isTeen()) && (type==LLWearableType::WT_UNDERPANTS || type==LLWearableType::WT_UNDERSHIRT)))
	{
		gAgentWearables.removeWearable(type, true, 0);
	}
}

// Given a desired set of attachments, find what objects need to be
// removed, and what additional inventory items need to be added.
void LLAgentWearables::findAttachmentsAddRemoveInfo(LLInventoryModel::item_array_t& obj_item_array,
	llvo_vec_t& objects_to_remove,
	llvo_vec_t& objects_to_retain,
	LLInventoryModel::item_array_t& items_to_add)
{
	// Possible cases:
	// already wearing but not in request set -> take off.
	// already wearing and in request set -> leave alone.
	// not wearing and in request set -> put on.

	if (!isAgentAvatarValid()) return;

	std::set<LLUUID> requested_item_ids;
	std::set<LLUUID> current_item_ids;
	for (S32 i = 0; i < obj_item_array.size(); i++)
	{
		const LLUUID& requested_id = obj_item_array[i].get()->getLinkedUUID();
		requested_item_ids.insert(requested_id);
	}

	// Build up list of objects to be removed and items currently attached.
	LLVOAvatar::attachment_map_t::iterator iter = gAgentAvatarp->mAttachmentPoints.begin();
	LLVOAvatar::attachment_map_t::iterator end = gAgentAvatarp->mAttachmentPoints.end();
	while (iter != end)
	{
		LLVOAvatar::attachment_map_t::iterator curiter = iter++;
		LLViewerJointAttachment* attachment = curiter->second;
		for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
			attachment_iter != attachment->mAttachedObjects.end();
			++attachment_iter)
		{
			LLViewerObject* objectp = attachment_iter->get();
			if (objectp)
			{
				LLUUID object_item_id = objectp->getAttachmentItemID();

				bool remove_attachment = true;
				if (requested_item_ids.find(object_item_id) != requested_item_ids.end())
				{   // Object currently worn, was requested to keep it
					// Flag as currently worn so we won't have to add it again.
					remove_attachment = false;
				}
				else if (objectp->isTempAttachment())
				{   // Check if we should keep this temp attachment
					remove_attachment = LLAppearanceMgr::instance().shouldRemoveTempAttachment(objectp->getID());
				}

				if (remove_attachment)
				{
					objects_to_remove.push_back(objectp);
				}
				else
				{
					current_item_ids.insert(object_item_id);
					objects_to_retain.push_back(objectp);
				}
			}
		}
	}

	for (LLInventoryModel::item_array_t::iterator it = obj_item_array.begin();
		it != obj_item_array.end();
		++it)
	{
		LLUUID linked_id = (*it).get()->getLinkedUUID();
		if (current_item_ids.find(linked_id) != current_item_ids.end())
		{
			// Requested attachment is already worn.
		}
		else
		{
			// Requested attachment is not worn yet.
			items_to_add.push_back(*it);
		}
	}
	// S32 remove_count = objects_to_remove.size();
	// S32 add_count = items_to_add.size();
}

std::vector<LLViewerObject*> LLAgentWearables::getTempAttachments()
{
	llvo_vec_t temp_attachs;
	if (isAgentAvatarValid())
	{
		for (LLVOAvatar::attachment_map_t::iterator iter = gAgentAvatarp->mAttachmentPoints.begin(); iter != gAgentAvatarp->mAttachmentPoints.end();)
		{
			LLVOAvatar::attachment_map_t::iterator curiter = iter++;
			LLViewerJointAttachment* attachment = curiter->second;
			for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
				attachment_iter != attachment->mAttachedObjects.end();
				++attachment_iter)
			{
				LLViewerObject* objectp = attachment_iter->get();
				if (objectp && objectp->isTempAttachment())
				{
					temp_attachs.push_back(objectp);
				}
			}
		}
	}
	return temp_attachs;
}

void LLAgentWearables::userRemoveMultipleAttachments(llvo_vec_t& objects_to_remove)
{
	if (!isAgentAvatarValid()) return;

	if (objects_to_remove.empty())
		return;

	LL_DEBUGS("Avatar") << "ATT [ObjectDetach] removing " << objects_to_remove.size() << " objects" << LL_ENDL;
	gMessageSystem->newMessage("ObjectDetach");
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());

	for (llvo_vec_t::iterator it = objects_to_remove.begin();
		it != objects_to_remove.end();
		++it)
	{
		LLViewerObject* objectp = *it;
		//gAgentAvatarp->resetJointPositionsOnDetach(objectp);
		gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
		gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, objectp->getLocalID());
		const LLUUID& item_id = objectp->getAttachmentItemID();
		LLViewerInventoryItem* item = gInventory.getItem(item_id);
		LL_DEBUGS("Avatar") << "ATT removing object, item is " << (item ? item->getName() : "UNKNOWN") << " " << item_id << LL_ENDL;
		LLAttachmentsMgr::instance().onDetachRequested(item_id);
	}
	gMessageSystem->sendReliable(gAgent.getRegionHost());
}

void LLAgentWearables::userAttachMultipleAttachments(LLInventoryModel::item_array_t& obj_item_array)
{
	// Build a compound message to send all the objects that need to be rezzed.
	auto obj_count = obj_item_array.size();
	if (obj_count > 0)
	{
		LL_DEBUGS("Avatar") << "ATT attaching multiple, total obj_count " << obj_count << LL_ENDL;
	}

	for (LLInventoryModel::item_array_t::const_iterator it = obj_item_array.begin();
		it != obj_item_array.end();
		++it)
	{
		const LLInventoryItem* item = *it;
		LLAttachmentsMgr::instance().addAttachmentRequest(item->getLinkedUUID(), 0, true);
	}
}

// Returns false if the given wearable is already topmost/bottommost
// (depending on closer_to_body parameter).
bool LLAgentWearables::canMoveWearable(const LLUUID& item_id, bool closer_to_body) const
{
	const LLWearable* wearable = getWearableFromItemID(item_id);
	if (!wearable) return false;

	LLWearableType::EType wtype = wearable->getType();
	const LLWearable* marginal_wearable = closer_to_body ? getBottomWearable(wtype) : getTopWearable(wtype);
	if (!marginal_wearable) return false;

	return wearable != marginal_wearable;
}

bool LLAgentWearables::areWearablesLoaded() const
{
	return mWearablesLoaded;
}

bool LLAgentWearables::canWearableBeRemoved(const LLViewerWearable* wearable) const
{
	if (!wearable) return false;

	LLWearableType::EType type = wearable->getType();
	// Make sure the user always has at least one shape, skin, eyes, and hair type currently worn.
	return !(((type == LLWearableType::WT_SHAPE) || (type == LLWearableType::WT_SKIN) || (type == LLWearableType::WT_HAIR) || (type == LLWearableType::WT_EYES))
		&& (getWearableCount(type) <= 1));
}
// <FS:Ansariel> [Legacy Bake]
//void LLAgentWearables::animateAllWearableParams(F32 delta)
void LLAgentWearables::animateAllWearableParams(F32 delta, bool upload_bake)
// </FS:Ansariel> [Legacy Bake]
{
	for (S32 type = 0; type < LLWearableType::WT_COUNT; ++type)
	{
		for (S32 count = 0; count < (S32)getWearableCount((LLWearableType::EType)type); ++count)
		{
			LLViewerWearable* wearable = getViewerWearable((LLWearableType::EType)type, count);
			llassert(wearable);
			if (wearable)
			{
				// <FS:Ansariel> [Legacy Bake]
                //wearable->animateParams(delta);
                wearable->animateParams(delta, upload_bake);
                // </FS:Ansariel> [Legacy Bake]
			}
		}
	}
}

bool LLAgentWearables::moveWearable(const LLViewerInventoryItem* item, bool closer_to_body)
{
	if (!item) return false;
	if (!item->isWearableType()) return false;

	LLWearableType::EType type = item->getWearableType();
	U32 wearable_count = getWearableCount(type);
    if (wearable_count < 2) return false;

	const LLUUID& asset_id = item->getAssetUUID();

	//nowhere to move if the wearable is already on any boundary (closest to the body/furthest from the body)
	if (closer_to_body)
	{
		LLViewerWearable* bottom_wearable = dynamic_cast<LLViewerWearable*>(getBottomWearable(type));
		if (bottom_wearable->getAssetID() == asset_id)
		{
			return false;
		}
	}
	else // !closer_to_body
	{
		LLViewerWearable* top_wearable = dynamic_cast<LLViewerWearable*>(getTopWearable(type));
		if (top_wearable->getAssetID() == asset_id)
		{
			return false;
		}
	}

	for (U32 i = 0; i < wearable_count; ++i)
	{
		LLViewerWearable* wearable = getViewerWearable(type, i);
		if (!wearable) continue;
		if (wearable->getAssetID() != asset_id) continue;

		//swapping wearables
		U32 swap_i = closer_to_body ? i - 1 : i + 1;
		swapWearables(type, i, swap_i);
		return true;
	}

	return false;
}

bool LLAgentWearables::moveWearableToIndex(const LLViewerInventoryItem* item, U32 new_index)
{
    if (!item) return false;
    if (!item->isWearableType()) return false;

    LLWearableType::EType type = item->getWearableType();
    U32 wearable_count = getWearableCount(type);
    if (wearable_count < 2) return false;

    if (new_index >= wearable_count) new_index = wearable_count - 1;

    S32 cur = getWearableIdxFromItem(item);
    if (cur < 0) return false;
    if ((U32)cur == new_index) return true; // already in place

    // step the wearable to its new slot one swap at a time
    U32 pos = (U32)cur;
    while (pos < new_index)
    {
        if (!swapWearables(type, pos, pos + 1)) return false;
        ++pos;
    }
    while (pos > new_index)
    {
        if (!swapWearables(type, pos, pos - 1)) return false;
        --pos;
    }

    return true;
}

// static
void LLAgentWearables::createWearable(LLWearableType::EType type, bool wear, const LLUUID& parent_id, std::function<void(const LLUUID&)> created_cb)
{
	if (type == LLWearableType::WT_INVALID || type == LLWearableType::WT_NONE) return;

	if (type == LLWearableType::WT_UNIVERSAL && !gAgent.getRegion()->bakesOnMeshEnabled())
	{
		LL_WARNS("Inventory") << "Can't create WT_UNIVERSAL type " << LL_ENDL;
		return;
	}

	LLViewerWearable* wearable = LLWearableList::instance().createNewWearable(type, gAgentAvatarp);
	LLAssetType::EType asset_type = wearable->getAssetType();
	LLPointer<LLBoostFuncInventoryCallback> cb;
	if (wear)
	{
		cb = new LLBoostFuncInventoryCallback(wear_and_edit_cb);
	}
	else
	{
		cb = new LLBoostFuncInventoryCallback(wear_cb);
	}
	if (created_cb != NULL)
	{
		cb->addOnFireFunc(created_cb);
	}

	LLUUID folder_id;

	if (parent_id.notNull())
	{
		folder_id = parent_id;
	}
	else
	{
		LLFolderType::EType folder_type = LLFolderType::assetTypeToFolderType(asset_type);
		folder_id = gInventory.findCategoryUUIDForType(folder_type);
	}

	create_inventory_wearable(gAgent.getID(),
		gAgent.getSessionID(),
		folder_id,
		wearable->getTransactionID(),
		wearable->getName(),
		wearable->getDescription(),
		asset_type,
		wearable->getType(),
		LLFloaterPerms::getNextOwnerPerms("Wearables"),
		cb);
}

// static
void LLAgentWearables::editWearable(const LLUUID& item_id)
{
	LLViewerInventoryItem* item = gInventory.getLinkedItem(item_id);
	if (!item)
	{
		LL_WARNS() << "Failed to get linked item" << LL_ENDL;
		return;
	}

	if (!item->isFinished())
	{
		LL_WARNS() << "Tried to edit wearable that isn't loaded" << LL_ENDL;
		// Restart fetch or put item to the front
		LLInventoryModelBackgroundFetch::instance().start(item->getUUID(), false);
		return;
	}

	LLViewerWearable* wearable = gAgentWearables.getWearableFromItemID(item_id);
	if (!wearable)
	{
		LL_WARNS() << "Cannot get wearable" << LL_ENDL;
		return;
	}

	if (!gAgentWearables.isWearableModifiable(item->getUUID()))
	{
		LL_WARNS() << "Cannot modify wearable" << LL_ENDL;
		return;
	}

	S32 shape_count = gAgentWearables.getWearableCount(LLWearableType::WT_SHAPE);
	S32 hair_count = gAgentWearables.getWearableCount(LLWearableType::WT_HAIR);
	S32 eye_count = gAgentWearables.getWearableCount(LLWearableType::WT_EYES);
	S32 skin_count = gAgentWearables.getWearableCount(LLWearableType::WT_SKIN);
	if (!shape_count || !hair_count || !eye_count || !skin_count)
	{
		// Don't let user edit wearables if avatar is cloud due to missing parts.
		// Let user edit wearables if avatar is cloud due to missing textures.
		LL_WARNS() << "Cannot modify wearable. Avatar is cloud and missing parts." << LL_ENDL;
		return;
	}

	const bool disable_camera_switch = LLWearableType::getInstance()->getDisableCameraSwitch(wearable->getType());
	LLPanel* panel = LLFloaterSidePanelContainer::getPanel("appearance");
	LLSidepanelAppearance::editWearable(wearable, panel, disable_camera_switch);
}

// Request editing the item after it gets worn.
void LLAgentWearables::requestEditingWearable(const LLUUID& item_id)
{
	mItemToEdit = gInventory.getLinkedItemID(item_id);
}

// Start editing the item if previously requested.
void LLAgentWearables::editWearableIfRequested(const LLUUID& item_id)
{
	if (mItemToEdit.notNull() &&
		mItemToEdit == gInventory.getLinkedItemID(item_id))
	{
		LLAgentWearables::editWearable(item_id);
		mItemToEdit.setNull();
	}
}

boost::signals2::connection LLAgentWearables::addLoadingStartedCallback(loading_started_callback_t cb)
{
	return mLoadingStartedSignal.connect(cb);
}

boost::signals2::connection LLAgentWearables::addLoadedCallback(loaded_callback_t cb)
{
	return mLoadedSignal.connect(cb);
}

bool LLAgentWearables::changeInProgress() const
{
	return mCOFChangeInProgress;
}

void LLAgentWearables::notifyLoadingStarted()
{
	mCOFChangeInProgress = true;
	mCOFChangeTimer.reset();
	mLoadingStartedSignal();
}

void LLAgentWearables::notifyLoadingFinished()
{
	mCOFChangeInProgress = false;
	mLoadedSignal();
}
// EOF

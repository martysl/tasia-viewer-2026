/**
 * @file llviewertexlayer.cpp
 * @brief Viewer texture layer. Used for avatars.
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
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

#include "llviewertexlayer.h"
#include "llavatarappearancedefines.h"
#include "indra_constants.h"

#include "llagent.h"
#include "llimagej2c.h"
#include "llnotificationsutil.h"
#include "llviewerregion.h"
#include "llhlslshader.h"
#include "llvoavatarself.h"
#include "pipeline.h"
#include "llviewercontrol.h"
#include "llagentwearables.h"
#include "llbakereadback.h"
#include "llfilesystem.h"
#include "llviewerassetstorage.h"
#include "llviewerassetupload.h"
#include "llviewernetwork.h"
#include <array>
#include <limits>
#include <mutex>

// Upload callbacks hold only this result mailbox, never a layerset/avatar
// pointer. Destroying a composite or logging out cannot leave a dangling
// callback. A revision ticket prevents an old outfit's response being applied.
struct LLTasiaBakeCompletion
{
    explicit LLTasiaBakeCompletion(std::uint64_t value) : ticket(value) {}
    void complete(bool ok, const LLUUID& id = LLUUID::null)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (ready) return;
        success = ok && id.notNull();
        asset = id;
        ready = true;
    }
    bool poll(bool& ok, LLUUID& id)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!ready) return false;
        ok = success;
        id = asset;
        return true;
    }
    const std::uint64_t ticket;
private:
    std::mutex mutex;
    bool ready = false;
    bool success = false;
    LLUUID asset;
};

namespace
{
class LLTasiaBakeUpload final : public LLBufferedAssetUploadInfo
{
public:
    LLTasiaBakeUpload(const LLUUID& id, const std::string& data,
                     std::shared_ptr<LLTasiaBakeCompletion> completion)
        : LLBufferedAssetUploadInfo(LLUUID::null, LLAssetType::AT_TEXTURE, data, nullptr, nullptr),
          mCompletion(std::move(completion))
    {
        setAssetId(id);
    }

    ~LLTasiaBakeUpload() override
    {
        // Also covers queue cancellation/shutdown before either callback.
        mCompletion->complete(false);
        LLFileSystem::removeFile(getAssetId(), LLAssetType::AT_TEXTURE);
    }

    LLSD prepareUpload() override
    {
        LLFileSystem file(getAssetId(), LLAssetType::AT_TEXTURE, LLFileSystem::WRITE);
        const std::string& data = getContents();
        if (!file.write(reinterpret_cast<const U8*>(data.data()), static_cast<S32>(data.size())))
        {
            return LLSD().with("error", "Unable to stage local bake");
        }
        return LLSD().with("success", true);
    }

    void logPreparedUpload() override {} // No capability URLs or asset IDs.

    LLUUID finishUpload(LLSD& result) override
    {
        const LLUUID id = result["new_asset"].asUUID();
        mCompletion->complete(result["state"].asString() == "complete", id);
        return id;
    }

    bool failedUpload(LLSD&, std::string&) override
    {
        mCompletion->complete(false);
        return true;
    }
private:
    std::shared_ptr<LLTasiaBakeCompletion> mCompletion;
};
}

// runway consolidate
extern std::string self_av_string();

//-----------------------------------------------------------------------------
// LLViewerTexLayerSetBuffer
// The composite image that a LLViewerTexLayerSet writes to.  Each LLViewerTexLayerSet has one.
//-----------------------------------------------------------------------------

// static
S32 LLViewerTexLayerSetBuffer::sGLByteCount = 0;

LLViewerTexLayerSetBuffer::LLViewerTexLayerSetBuffer(LLTexLayerSet* const owner,
                                         S32 width, S32 height) :
    // ORDER_LAST => must render these after the hints are created.
    LLTexLayerSetBuffer(owner),
    LLViewerDynamicTexture(width, height, 4, LLViewerDynamicTexture::ORDER_LAST, false),
    mNeedsUpdate(true),
    mNumLowresUpdates(0)
{
    mGLTexturep->setNeedsAlphaAndPickMask(false);

    LLViewerTexLayerSetBuffer::sGLByteCount += getSize();
    mNeedsUpdateTimer.start();
}

LLViewerTexLayerSetBuffer::~LLViewerTexLayerSetBuffer()
{
    LLViewerTexLayerSetBuffer::sGLByteCount -= getSize();
    destroyGLTexture();
    for( S32 order = 0; order < ORDER_COUNT; order++ )
    {
        LLViewerDynamicTexture::sInstances[order].erase(this);  // will fail in all but one case.
    }
}

//virtual
S8 LLViewerTexLayerSetBuffer::getType() const
{
    return LLViewerDynamicTexture::LL_TEX_LAYER_SET_BUFFER ;
}

//virtual
void LLViewerTexLayerSetBuffer::restoreGLTexture()
{
    LLViewerDynamicTexture::restoreGLTexture() ;
}

//virtual
void LLViewerTexLayerSetBuffer::destroyGLTexture()
{
    LLViewerDynamicTexture::destroyGLTexture() ;
}

// static
void LLViewerTexLayerSetBuffer::dumpTotalByteCount()
{
    LL_INFOS() << "Composite System GL Buffers: " << (LLViewerTexLayerSetBuffer::sGLByteCount/1024) << "KB" << LL_ENDL;
}

void LLViewerTexLayerSetBuffer::requestUpdate()
{
    restartUpdateTimer();
    mNeedsUpdate = true;
    mNumLowresUpdates = 0;
    if (usesLegacyUpload()) requestUpload();
}

void LLViewerTexLayerSetBuffer::restartUpdateTimer()
{
    mNeedsUpdateTimer.reset();
    mNeedsUpdateTimer.start();
}

// virtual
bool LLViewerTexLayerSetBuffer::needsRender()
{
    llassert(mTexLayerSet->getAvatarAppearance() == gAgentAvatarp);
    if (!isAgentAvatarValid()) return false;

    processUploadCompletion();
    if (!usesLegacyUpload() && uploadPending()) cancelUpload();

    const bool update_now = mNeedsUpdate && isReadyToUpdate();
    const bool upload_now = mUploadPolicy.needed() && isReadyToUpload();

    // Don't render if we don't want to (or aren't ready to) update.
    if (!(update_now || upload_now))
    {
        return false;
    }

    // Don't render if we're animating our appearance.
    if (gAgentAvatarp->getIsAppearanceAnimating())
    {
        return false;
    }

    // Don't render if we are trying to create a skirt texture but aren't wearing a skirt.
    if (gAgentAvatarp->getBakedTE(getViewerTexLayerSet()) == LLAvatarAppearanceDefines::TEX_SKIRT_BAKED &&
        !gAgentAvatarp->isWearingWearableType(LLWearableType::WT_SKIRT))
    {
        cancelUpload();
        return false;
    }

    // Render if we have at least minimal level of detail for each local texture.
    return getViewerTexLayerSet()->isLocalTextureDataAvailable();
}

// virtual
void LLViewerTexLayerSetBuffer::preRenderTexLayerSet()
{
    LLTexLayerSetBuffer::preRenderTexLayerSet();

    // keep depth buffer, we don't need to clear it
    LLViewerDynamicTexture::preRender(false);
}

// virtual
void LLViewerTexLayerSetBuffer::postRenderTexLayerSet(bool success)
{

    LLTexLayerSetBuffer::postRenderTexLayerSet(success);
    LLViewerDynamicTexture::postRender(success);
}

// virtual
void LLViewerTexLayerSetBuffer::midRenderTexLayerSet(bool success)
{
    if (!success && usesLegacyUpload()) return;
    if (success && mUploadPolicy.needed() && isReadyToUpload()) doUpload();
    const bool update_now = mNeedsUpdate && isReadyToUpdate();
    if (update_now)
    {
        doUpdate();
    }

    // *TODO: Old logic does not check success before setGLTextureCreated
    // we have valid texture data now
    mGLTexturep->setGLTextureCreated(true);
}

bool LLViewerTexLayerSetBuffer::usesLegacyUpload() const
{
    const LLVOAvatarSelf* avatar = getViewerTexLayerSet()->getAvatar();
    return LLGridManager::getInstance()->isInOpenSim() && avatar &&
           avatar->getRegion() && avatar->getRegion()->getCentralBakeVersion() == 0 &&
           getViewerTexLayerSet()->getBakedTexIndex() < avatar->getNumBakes();
}

void LLViewerTexLayerSetBuffer::requestUpload()
{
    if (usesLegacyUpload()) mUploadPolicy.request();
}

void LLViewerTexLayerSetBuffer::cancelUpload()
{
    mUploadPolicy.cancel();
    mUploadCompletion.reset();
}

bool LLViewerTexLayerSetBuffer::isReadyToUpload() const
{
    return usesLegacyUpload() && !mUploadPolicy.inFlight() &&
           gAgentWearables.areWearablesLoaded() && !getViewerTexLayerSet()->getAvatar()->isEditingAppearance() &&
           getViewerTexLayerSet()->isLocalTextureDataFinal() &&
           (mUploadPolicy.failures() == 0 ||
            mUploadRetryTimer.getElapsedTimeF32() >= mUploadPolicy.retryDelay());
}

void LLViewerTexLayerSetBuffer::doUpload()
{
    const std::uint64_t ticket = mUploadPolicy.begin();
    if (!ticket) return;
    mUploadCompletion = std::make_shared<LLTasiaBakeCompletion>(ticket);
    auto completion = mUploadCompletion;
    LLViewerTexLayerSet* layer = getViewerTexLayerSet();
    // A fully invisible alpha mask is a semantic texture, not an empty J2C.
    // Match the reference's IMG_INVISIBLE path before reading/encoding pixels.
    if (completeInvisibleBake()) return;
    std::vector<std::uint8_t> rgba;
    if (!LLBake::readColor(mBoundTarget, mOrigin.mX, mOrigin.mY, mFullWidth, mFullHeight, rgba))
    {
        LL_WARNS_ONCE("LocalBake") << "Local bake color readback failed" << LL_ENDL;
        completion->complete(false);
        return;
    }
    const S32 bake_slot = static_cast<S32>(layer->getBakedTexIndex());
    static std::array<U32, LLAvatarAppearanceDefines::BAKED_NUM_INDICES> pixel_summary_samples{};
    const bool head_bake = bake_slot == LLAvatarAppearanceDefines::BAKED_HEAD;
    const bool log_pixel_sample = bake_slot >= 0 &&
        bake_slot < LLAvatarAppearanceDefines::BAKED_NUM_INDICES &&
        pixel_summary_samples[bake_slot] < 2;
    LLBake::PixelSummary summary;
    const bool have_pixel_summary = (head_bake || log_pixel_sample) &&
        LLBake::summarizeRGBA(rgba.data(), rgba.size(), mFullWidth, mFullHeight, summary);
    if (have_pixel_summary && log_pixel_sample)
    {
        ++pixel_summary_samples[bake_slot];
        // LL_INFOS is compiled out in this Release build (LL_DISABLE_INFOS).
        LL_VLOGS(LLError::LEVEL_INFO, "LocalBake")
            << "Local bake pixels; sample=" << pixel_summary_samples[bake_slot]
            << "; slot=" << bake_slot
            << "; region=" << layer->getBodyRegionName()
            << "; pixels=" << summary.pixels
            << "; non_black=" << summary.non_black
            << "; nontransparent_black=" << summary.nontransparent_black
            << "; transparent=" << summary.transparent << LL_ENDL;
    }
    if (head_bake && have_pixel_summary && LLBake::isAllBlackNonTransparent(summary))
    {
        // Never replace the current head bake with the empty compositor clear.
        LL_WARNS("LocalBake") << "Rejecting all-black nontransparent BAKED_HEAD composite before upload" << LL_ENDL;
        // The aggregate readback identifies the symptom, not its source. Emit a
        // bounded, identifier-free snapshot of the head bake's local inputs so
        // a later failure can distinguish unavailable-input clues from cases
        // where all inputs appear available, without exposing UUIDs or names.
        static U32 black_head_diagnostic_samples = 0;
        if (black_head_diagnostic_samples < 2)
        {
            ++black_head_diagnostic_samples;
            constexpr std::array<LLAvatarAppearanceDefines::ETextureIndex, 4> head_inputs = {
                LLAvatarAppearanceDefines::TEX_HEAD_BODYPAINT,
                LLAvatarAppearanceDefines::TEX_HEAD_TATTOO,
                LLAvatarAppearanceDefines::TEX_HEAD_ALPHA,
                LLAvatarAppearanceDefines::TEX_HEAD_UNIVERSAL_TATTOO
            };
            constexpr U32 MAX_DIAGNOSTIC_WEARABLES = 16;
            const LLVOAvatarSelf* avatar = layer->getAvatar();
            for (const auto texture_index : head_inputs)
            {
                U32 defined = 0;
                U32 unavailable = 0;
                const LLWearableType::EType wearable_type =
                    LLAvatarAppearance::getDictionary()->getTEWearableType(texture_index);
                const U32 wearable_count = gAgentWearables.getWearableCount(wearable_type);
                const U32 scan_count = llmin(wearable_count, MAX_DIAGNOSTIC_WEARABLES);
                S32 min_discard = std::numeric_limits<S32>::max();
                S32 max_discard = std::numeric_limits<S32>::min();
                for (U32 wearable_index = 0; wearable_index < scan_count; ++wearable_index)
                {
                    if (!avatar->isTextureDefined(texture_index, wearable_index)) continue;
                    ++defined;
                    const S32 discard = avatar->getLocalDiscardLevel(texture_index, wearable_index);
                    if (discard < 0) ++unavailable;
                    min_discard = llmin(min_discard, discard);
                    max_discard = llmax(max_discard, discard);
                }
                LL_WARNS("LocalBake") << "BAKED_HEAD input state; sample="
                    << black_head_diagnostic_samples << "; texture_index="
                    << static_cast<S32>(texture_index) << "; defined=" << defined
                    << "; unavailable=" << unavailable << "; min_discard="
                    << (defined ? min_discard : -1) << "; max_discard="
                    << (defined ? max_discard : -1)
                    << "; wearable_scan_capped=" << (wearable_count > MAX_DIAGNOSTIC_WEARABLES)
                    << LL_ENDL;
            }
        }
        completion->complete(false);
        return;
    }
    LLPointer<LLImageRaw> mask = new LLImageRaw(mFullWidth, mFullHeight, 1);
    LLPointer<LLImageRaw> baked = new LLImageRaw(mFullWidth, mFullHeight, 5);
    if (!mask->getData() || !baked->getData())
    {
        completion->complete(false);
        return;
    }
    layer->gatherMorphMaskAlpha(mask->getData(), mOrigin.mX, mOrigin.mY,
                               mFullWidth, mFullHeight, mBoundTarget);
    if (!LLBake::packFiveChannels(rgba.data(), rgba.size(), mask->getData(), mask->getDataSize(),
                                 baked->getData(), baked->getDataSize(), mFullWidth, mFullHeight))
    {
        completion->complete(false);
        return;
    }
    LLPointer<LLImageJ2C> compressed = new LLImageJ2C;
    if (!compressed->encode(baked, LINDEN_J2C_COMMENT_PREFIX "RGBHM") || compressed->getDataSize() <= 0 ||
        !compressed->updateData() || compressed->getComponents() != 5 ||
        compressed->getWidth() != mFullWidth || compressed->getHeight() != mFullHeight)
    {
        LL_WARNS_ONCE("LocalBake") << "Local bake J2C encoding or metadata validation failed" << LL_ENDL;
        completion->complete(false);
        return;
    }
    LL_INFOS_ONCE("LocalBake") << "Local bake encoded; components=" << static_cast<S32>(compressed->getComponents())
                             << "; bytes=" << compressed->getDataSize() << LL_ENDL;
    LLTransactionID transaction;
    transaction.generate();
    const LLUUID asset_id = transaction.makeAssetID(gAgent.getSecureSessionID());
    const std::string cap = gAgent.getRegionCapability("UploadBakedTexture");
    if (mUploadPolicy.useHttp(!cap.empty()))
    {
        const std::string data(reinterpret_cast<const char*>(compressed->getData()), compressed->getDataSize());
        LLResourceUploadInfo::ptr_t upload(new LLTasiaBakeUpload(asset_id, data, completion));
        LLViewerAssetUpload::EnqueueInventoryUpload(cap, upload);
    }
    else if (gAssetStorage)
    {
        LLFileSystem file(asset_id, LLAssetType::AT_TEXTURE, LLFileSystem::WRITE);
        if (!file.write(compressed->getData(), compressed->getDataSize()))
        {
            completion->complete(false);
            return;
        }
        auto* userdata = new std::shared_ptr<LLTasiaBakeCompletion>(completion);
        gAssetStorage->storeAssetData(transaction, LLAssetType::AT_TEXTURE,
            [](const LLUUID& id, void* context, S32 result, LLExtStat)
            {
                std::unique_ptr<std::shared_ptr<LLTasiaBakeCompletion>> saved(
                    static_cast<std::shared_ptr<LLTasiaBakeCompletion>*>(context));
                (*saved)->complete(result >= 0, id);
            }, userdata, true, true, true);
    }
    else
    {
        completion->complete(false);
    }
}

bool LLViewerTexLayerSetBuffer::completeInvisibleBake()
{
    if (getViewerTexLayerSet()->isVisible()) return false;
    mUploadCompletion->complete(true, IMG_INVISIBLE);
    return true;
}

void LLViewerTexLayerSetBuffer::processUploadCompletion()
{
    if (!mUploadCompletion) return;
    bool success = false;
    LLUUID id;
    if (!mUploadCompletion->poll(success, id)) return;
    const auto result = mUploadPolicy.complete(mUploadCompletion->ticket, success);
    mUploadCompletion.reset();
    if (!usesLegacyUpload())
    {
        cancelUpload();
        return;
    }
    if (result == TasiaBake::UploadPolicy::Result::Accepted)
    {
        if (id == IMG_INVISIBLE)
        {
            LL_INFOS_ONCE("LocalBake") << "Invisible alpha bake assigned without upload" << LL_ENDL;
        }
        else
        {
            LL_INFOS_ONCE("LocalBake") << "Local bake upload completed; assigning baked texture" << LL_ENDL;
        }
        getViewerTexLayerSet()->getAvatar()->setNewBakedTexture(
            getViewerTexLayerSet()->getBakedTexIndex(), id);
        // Refresh attachment BOM textures: attachments using baked texture
        // magic IDs (IMG_USE_BAKED_*) need to rebind to the newly uploaded
        // bake, otherwise surfaces like Genus Eyebrow Tintable A stay black.
        LLVOAvatarSelf* avatar = getViewerTexLayerSet()->getAvatar();
        if (avatar)
        {
            for (const auto& [attachment_point_id, attachment] : avatar->mAttachmentPoints)
            {
                if (attachment)
                {
                    for (auto& objectp : attachment->mAttachedObjects)
                    {
                        if (!objectp.isNull())
                        {
                            objectp->refreshBakeTexture();
                        }
                    }
                }
            }
        }
    }
    else if (result == TasiaBake::UploadPolicy::Result::Retry)
    {
        mUploadRetryTimer.start();
    }
    else if (result == TasiaBake::UploadPolicy::Result::Exhausted)
    {
        LL_WARNS("LocalBake") << "Local bake upload exhausted bounded retries" << LL_ENDL;
        LLNotificationsUtil::add("TasiaAppearanceUploadFailed");
    }
}

bool LLViewerTexLayerSetBuffer::isInitialized(void) const
{
    return mGLTexturep.notNull() && mGLTexturep->isGLTextureCreated();
}

bool LLViewerTexLayerSetBuffer::isReadyToUpdate() const
{
    // If we requested an update and have the final LOD ready, then update.
    if (getViewerTexLayerSet()->isLocalTextureDataFinal()) return true;

    // If we haven't done an update yet, then just do one now regardless of state of textures.
    if (mNumLowresUpdates == 0) return true;

    // Update if we've hit a timeout.  Unlike for uploads, we can make this timeout fairly small
    // since render unnecessarily doesn't cost much.
    const U32 TEXTURE_TIMEOUT = 10;
    if (TEXTURE_TIMEOUT != 0)
    {
        // If we hit our timeout and have textures available at even lower resolution, then update.
        const bool is_update_textures_timeout = mNeedsUpdateTimer.getElapsedTimeF32() >= TEXTURE_TIMEOUT;
        const bool has_lower_lod = getViewerTexLayerSet()->isLocalTextureDataAvailable();
        if (has_lower_lod && is_update_textures_timeout) return true;
    }

    return false;
}

bool LLViewerTexLayerSetBuffer::requestUpdateImmediate()
{
    mNeedsUpdate = true;
    bool result = false;

    if (needsRender())
    {
        preRender(false);
        result = render();
        postRender(result);
    }

    return result;
}

// Mostly bookkeeping; don't need to actually "do" anything since
// render() will actually do the update.
void LLViewerTexLayerSetBuffer::doUpdate()
{
    LLViewerTexLayerSet* layer_set = getViewerTexLayerSet();
    const bool highest_lod = layer_set->isLocalTextureDataFinal();
    if (highest_lod)
    {
        mNeedsUpdate = false;
    }
    else
    {
        mNumLowresUpdates++;
    }

    restartUpdateTimer();

    // need to switch to using this layerset if this is the first update
    // after getting the lowest LOD
    layer_set->getAvatar()->updateMeshTextures();

    // Print out notification that we updated this texture.
    if (gSavedSettings.getBOOL("DebugAvatarRezTime"))
    {
        const bool highest_lod = layer_set->isLocalTextureDataFinal();
        const std::string lod_str = highest_lod ? "HighRes" : "LowRes";
        LLSD args;
        args["EXISTENCE"] = llformat("%d",(U32)layer_set->getAvatar()->debugGetExistenceTimeElapsedF32());
        args["TIME"] = llformat("%d",(U32)mNeedsUpdateTimer.getElapsedTimeF32());
        args["BODYREGION"] = layer_set->getBodyRegionName();
        args["RESOLUTION"] = lod_str;
        LLNotificationsUtil::add("AvatarRezSelfBakedTextureUpdateNotification",args);
        LL_DEBUGS("Avatar") << self_av_string() << "Locally updating [ name: " << layer_set->getBodyRegionName() << " res:" << lod_str << " time:" << (U32)mNeedsUpdateTimer.getElapsedTimeF32() << " ]" << LL_ENDL;
    }
}

//-----------------------------------------------------------------------------
// LLViewerTexLayerSet
// An ordered set of texture layers that get composited into a single texture.
//-----------------------------------------------------------------------------

LLViewerTexLayerSet::LLViewerTexLayerSet(LLAvatarAppearance* const appearance) :
    LLTexLayerSet(appearance),
    mUpdatesEnabled( false )
{
}

// virtual
LLViewerTexLayerSet::~LLViewerTexLayerSet()
{
}

// Returns true if at least one packet of data has been received for each of the textures that this layerset depends on.
bool LLViewerTexLayerSet::isLocalTextureDataAvailable() const
{
    if (!mAvatarAppearance->isSelf()) return false;
    return getAvatar()->isLocalTextureDataAvailable(this);
}


// Returns true if all of the data for the textures that this layerset depends on have arrived.
bool LLViewerTexLayerSet::isLocalTextureDataFinal() const
{
    if (!mAvatarAppearance->isSelf()) return false;
    return getAvatar()->isLocalTextureDataFinal(this);
}

// virtual
void LLViewerTexLayerSet::requestUpdate()
{
    if( mUpdatesEnabled )
    {
        createComposite();
        getViewerComposite()->requestUpdate();
    }
}

void LLViewerTexLayerSet::requestUpload()
{
    createComposite();
    getViewerComposite()->requestUpload();
}

void LLViewerTexLayerSet::cancelUpload()
{
    if (mComposite) getViewerComposite()->cancelUpload();
}

void LLViewerTexLayerSet::updateComposite()
{
    createComposite();
    getViewerComposite()->requestUpdateImmediate();
}

// virtual
void LLViewerTexLayerSet::createComposite()
{
    if(!mComposite)
    {
        S32 width = mInfo->getWidth();
        S32 height = mInfo->getHeight();
        // Composite other avatars at reduced resolution
        if( !mAvatarAppearance->isSelf() )
        {
            LL_ERRS() << "composites should not be created for non-self avatars!" << LL_ENDL;
        }
        mComposite = new LLViewerTexLayerSetBuffer( this, width, height );
    }
}

void LLViewerTexLayerSet::setUpdatesEnabled( bool b )
{
    mUpdatesEnabled = b;
}

LLVOAvatarSelf* LLViewerTexLayerSet::getAvatar()
{
    return dynamic_cast<LLVOAvatarSelf*> (mAvatarAppearance);
}

const LLVOAvatarSelf* LLViewerTexLayerSet::getAvatar() const
{
    return dynamic_cast<const LLVOAvatarSelf*> (mAvatarAppearance);
}

LLViewerTexLayerSetBuffer* LLViewerTexLayerSet::getViewerComposite()
{
    return dynamic_cast<LLViewerTexLayerSetBuffer*> (getComposite());
}

const LLViewerTexLayerSetBuffer* LLViewerTexLayerSet::getViewerComposite() const
{
    return dynamic_cast<const LLViewerTexLayerSetBuffer*> (getComposite());
}


const std::string LLViewerTexLayerSetBuffer::dumpTextureInfo() const
{
    if (!isAgentAvatarValid()) return "";

    const bool is_high_res = true;
    const U32 num_low_res = 0;
    const std::string local_texture_info = gAgentAvatarp->debugDumpLocalTextureDataInfo(getViewerTexLayerSet());

    std::string text = llformat("[HiRes:%d LoRes:%d] %s",
                                is_high_res, num_low_res,
                                local_texture_info.c_str());
    return text;
}

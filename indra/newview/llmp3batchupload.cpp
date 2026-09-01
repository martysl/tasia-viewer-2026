/**
 * @file llmp3batchupload.cpp
 * @brief MP3 conversion/segmentation and normal-cost sound uploads.
 */
#include "llviewerprecompiledheaders.h"
#include "llmp3batchupload.h"

#include "llagent.h"
#include "llagentbenefits.h"
#include "llagentcamera.h"
#include "llfilepicker.h"
#include "llfilesystem.h"
#include "llfloaterperms.h"
#include "llfloaterreg.h"
#include "llprogressbar.h"
#include "llviewernetwork.h"
#include "llinventorymodel.h"
#include "llnotecard.h"
#include "llpreviewnotecard.h"
#include "llnotificationsutil.h"
#include "llevents.h"
#include "llprocess.h"
#include "llstatusbar.h"
#include "lluploaddialog.h"
#include "llviewerassetupload.h"
#include "llviewermenufile.h"
#include "llviewerinventory.h"
#include "llviewerregion.h"
#include "llvorbisencode.h"

#include <algorithm>
#include <fstream>
#include <memory>
#include <sstream>
#include <vector>

void create_new_item(const std::string& name, const LLUUID& parent_id,
                     LLAssetType::EType asset_type, LLInventoryType::EType inv_type,
                     U32 next_owner_perm, std::function<void(const LLUUID&)> created_cb);

namespace
{
const F32 MP3_SEGMENT_MARGIN_SECONDS = 1.0f;

void show_batch_progress(const std::string& status, S32 completed, S32 total)
{
    LLFloater* floater = LLFloaterReg::showInstance("mp3_batch_progress");
    floater->getChild<LLUICtrl>("status_text")->setValue(status);
    floater->getChild<LLUICtrl>("progress_count")->setValue(llformat("%d / %d", completed, total));
    floater->getChild<LLProgressBar>("progress_bar")->setValue(total ? (F32)completed / total : 0.f);
}

void close_batch_progress()
{
    LLFloaterReg::hideInstance("mp3_batch_progress");
}

struct BatchPart
{
    S32 ordinal;
    std::string filename;
    LLUUID asset_id;
};

class MP3BatchContext;
typedef std::shared_ptr<MP3BatchContext> MP3BatchContextPtr;

class MP3BatchSoundUploadInfo final : public LLNewFileResourceUploadInfo
{
public:
    MP3BatchSoundUploadInfo(const BatchPart& part, const MP3BatchContextPtr& context, S32 cost);
    LLUUID finishUpload(LLSD& result) override;
    bool failedUpload(LLSD& result, std::string& reason) override;
private:
    BatchPart mPart;
    MP3BatchContextPtr mContext;
};

class MP3BatchContext : public std::enable_shared_from_this<MP3BatchContext>
{
public:
    explicit MP3BatchContext(std::vector<BatchPart> parts) : mParts(std::move(parts)), mPending((S32)mParts.size()) {}
    ~MP3BatchContext() { cleanup(); }

    void start(S32 cost)
    {
        show_batch_progress("Uploading MP3 sound parts…", 0, (S32)mParts.size());
        for (const BatchPart& part : mParts)
        {
            LLResourceUploadInfo::ptr_t info = std::make_shared<MP3BatchSoundUploadInfo>(part, shared_from_this(), cost);
            upload_new_resource(info);
        }
    }

    void succeeded(const BatchPart& part, const LLSD& result)
    {
        // Only server-confirmed asset IDs belong in the report.
        const LLUUID inventory_id = result["new_inventory_item"].asUUID();
        const LLUUID asset_id = result["new_asset"].asUUID();
        if (inventory_id.notNull() && asset_id.notNull())
        {
            BatchPart confirmed(part);
            confirmed.asset_id = asset_id;
            mSucceeded.push_back(confirmed);
        }
        completeOne();
    }

    void failed(const std::string& reason)
    {
        ++mFailed;
        LL_WARNS("MP3BatchUpload") << "Part upload failed: " << reason << LL_ENDL;
        completeOne();
    }

private:
    void completeOne()
    {
        const S32 completed = (S32)mParts.size() - mPending + 1;
        show_batch_progress("Uploading MP3 sound parts…", completed, (S32)mParts.size());
        if (--mPending == 0)
        {
            close_batch_progress();
            if (!mSucceeded.empty()) createReportNotecard();
            LLSD args;
            args["SUCCESS"] = (S32)mSucceeded.size();
            args["FAILED"] = mFailed;
            LLNotificationsUtil::add("MP3BatchSoundUploadFinished", args);
            cleanup();
        }
    }

    void createReportNotecard()
    {
        std::sort(mSucceeded.begin(), mSucceeded.end(), [](const BatchPart& a, const BatchPart& b) { return a.ordinal < b.ordinal; });
        std::ostringstream text;
        text << "MP3 batch sound upload results (server-confirmed assets only)\n\n";
        for (const BatchPart& part : mSucceeded)
            text << "Part " << (part.ordinal + 1) << ": " << part.asset_id.asString() << "\n";
        LLNotecard card(LLNotecard::MAX_SIZE);
        card.setText(text.str());
        std::stringstream serialized;
        card.exportStream(serialized);
        const std::string contents = serialized.str();
        const MP3BatchContextPtr self = shared_from_this();
        create_new_item("MP3 Upload Results", gInventory.findCategoryUUIDForType(LLFolderType::FT_NOTECARD),
            LLAssetType::AT_NOTECARD, LLInventoryType::IT_NOTECARD, 0,
            [self, contents](const LLUUID& item_id)
            {
                LLViewerRegion* region = gAgent.getRegion();
                if (!region || item_id.isNull()) return;
                const std::string url = region->getCapability("UpdateNotecardAgentInventory");
                if (url.empty())
                {
                    LL_WARNS("MP3BatchUpload") << "Notecard capability unavailable; results item was not populated." << LL_ENDL;
                    return;
                }
                LLResourceUploadInfo::ptr_t info = std::make_shared<LLBufferedAssetUploadInfo>(item_id, LLAssetType::AT_NOTECARD,
                    contents,
                    [](LLUUID saved_item_id, LLUUID new_asset_id, LLUUID new_item_id, LLSD)
                    {
                        LLPreviewNotecard::finishInventoryUpload(saved_item_id, new_asset_id, new_item_id);
                    },
                    nullptr);
                LLViewerAssetUpload::EnqueueInventoryUpload(url, info);
            });
    }

    void cleanup()
    {
        for (const BatchPart& part : mParts)
            if (!part.filename.empty()) LLFile::remove(part.filename);
        mParts.clear();
    }

    std::vector<BatchPart> mParts;
    std::vector<BatchPart> mSucceeded;
    S32 mPending;
    S32 mFailed = 0;
};

MP3BatchSoundUploadInfo::MP3BatchSoundUploadInfo(const BatchPart& part, const MP3BatchContextPtr& context, S32 cost)
    : LLNewFileResourceUploadInfo(part.filename, "MP3 part " + std::to_string(part.ordinal + 1), "Converted MP3 segment", 0,
        LLFolderType::FT_SOUND, LLInventoryType::IT_SOUND,
        LLFloaterPerms::getNextOwnerPerms("Uploads"), LLFloaterPerms::getGroupPerms("Uploads"),
        LLFloaterPerms::getEveryonePerms("Uploads"), cost), mPart(part), mContext(context) {}

LLUUID MP3BatchSoundUploadInfo::finishUpload(LLSD& result)
{
    LLUUID item_id = LLNewFileResourceUploadInfo::finishUpload(result);
    mContext->succeeded(mPart, result);
    return item_id;
}

bool MP3BatchSoundUploadInfo::failedUpload(LLSD& result, std::string& reason)
{
    mContext->failed(reason);
    return false;
}

void begin_confirmed_batch(const MP3BatchContextPtr& context, S32 cost, const LLSD& notification, const LLSD& response)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) == 0) context->start(cost);
}

void enumerate_and_confirm_parts(const std::string& prefix, F32 maximum)
{
    std::vector<BatchPart> parts;
    for (S32 ordinal = 0; ; ++ordinal)
    {
        const std::string part = prefix + llformat("%03d.wav", ordinal);
        if (!gDirUtilp->fileExists(part)) break;
        std::string error;
        if (check_for_invalid_wav_formats(part, error, LLGridManager::instance().isInSecondLife()))
        {
            LLFile::remove(part);
            for (const BatchPart& accepted : parts) LLFile::remove(accepted.filename);
            close_batch_progress();
            LLSD args; args["FILE"] = part; args["MAX_LENGTH"] = llformat("%.0f", maximum);
            LLNotificationsUtil::add(error, args);
            return;
        }
        parts.push_back({ ordinal, part, LLUUID::null });
    }
    if (parts.empty())
    {
        close_batch_progress();
        LLNotificationsUtil::add("MP3BatchSoundConversionFailed");
        return;
    }

    S32 unit_cost = 0;
    LLAssetType::EType sound_type = LLAssetType::AT_SOUND;
    if (!LLAgentBenefitsMgr::current().findUploadCost(sound_type, unit_cost))
    {
        for (const BatchPart& part : parts) LLFile::remove(part.filename);
        close_batch_progress();
        LLNotificationsUtil::add("MP3BatchSoundCostUnavailable");
        return;
    }
    const S32 total_cost = unit_cost * (S32)parts.size();
    if (total_cost > gStatusBar->getBalance())
    {
        for (const BatchPart& part : parts) LLFile::remove(part.filename);
        close_batch_progress();
        LLSD args; args["COST"] = total_cost; args["COUNT"] = (S32)parts.size(); args["BALANCE"] = gStatusBar->getBalance();
        LLNotificationsUtil::add("NotEnoughMoneyForBulkUpload", args);
        return;
    }

    close_batch_progress();
    MP3BatchContextPtr context = std::make_shared<MP3BatchContext>(parts);
    LLSD args; args["COUNT"] = (S32)parts.size(); args["UNIT_COST"] = unit_cost; args["TOTAL_COST"] = total_cost;
    LLNotificationsUtil::add("ConfirmMP3BatchSoundUpload", args, LLSD(), boost::bind(&begin_confirmed_batch, context, unit_cost, _1, _2));
}

class MP3ConversionContext;
typedef std::shared_ptr<MP3ConversionContext> MP3ConversionContextPtr;

class MP3ConversionContext : public std::enable_shared_from_this<MP3ConversionContext>
{
public:
    MP3ConversionContext(const std::string& input, const std::string& prefix, F32 maximum)
        : mInput(input),
          mPrefix(prefix),
          mMaximum(maximum),
          mPostendPump("MP3BatchConversion", true),
          mPostendListener(mPostendPump.listen("MP3BatchConversion", boost::bind(&MP3ConversionContext::conversionEnded, this, _1)))
    {
    }

    void start()
    {
#if LL_LINUX || LL_WINDOWS
        std::string executable = gDirUtilp->getExecutableDir();
#if LL_WINDOWS
        const std::string converter_name = "tasia-ffmpeg.exe";
#else
        const std::string converter_name = "tasia-ffmpeg";
#endif
        gDirUtilp->append(executable, converter_name);
        llstat st;
        if (LLFile::stat(executable, &st) != 0)
        {
            // The launcher normally runs from the package root while the binary
            // lives in bin/. Use that package-relative fallback as well.
            executable = gDirUtilp->getWorkingDir();
            gDirUtilp->append(executable, "bin");
            gDirUtilp->append(executable, converter_name);
            if (LLFile::stat(executable, &st) != 0)
            {
                failed("Bundled converter not found: " + executable, "MP3BatchSoundFfmpegMissing");
                return;
            }
        }
        LL_INFOS("MP3BatchUpload") << "Converting '" << mInput << "' with " << executable << LL_ENDL;
        LLProcess::Params params;
        params.executable = executable;
        params.args.add("-y");
        params.args.add("-v");
        params.args.add("error");
        params.args.add("-i");
        params.args.add(mInput);
        params.args.add("-ar");
        params.args.add("44100");
        params.args.add("-ac");
        params.args.add("2");
        params.args.add("-c:a");
        params.args.add("pcm_s16le");
        params.args.add("-f");
        params.args.add("segment");
        params.args.add("-segment_time");
        params.args.add(llformat("%.2f", mMaximum - MP3_SEGMENT_MARGIN_SECONDS));
        params.args.add(mPrefix + "%03d.wav");
        params.postend = mPostendPump.getName();
        params.desc = "MP3 batch converter";

        // Keep this context (and therefore the LLProcess and listener) alive
        // until LLProcess posts its terminal state on the main loop.
        MP3ConversionContextPtr keep_alive = shared_from_this();
        mSelf = keep_alive;
        mProcess = LLProcess::create(params);
        if (!mProcess)
        {
            // LLProcess posts UNSTARTED synchronously on launch failure.
            // conversionEnded() has already reported it in that case.
            return;
        }
#else
        failed("Bundled converter is only supported on Linux", "MP3BatchSoundFfmpegMissing");
#endif
    }

private:
    bool conversionEnded(const LLSD& event)
    {
        // Linux viewer builds define LL_IGNORE_SIGCHLD, so LLProcess receives
        // EXITED with data=-1 even when FFmpeg completed successfully. The
        // generated WAVs are validated below; only a non-EXITED process state
        // is a converter failure here.
        const bool exited = event["state"].asInteger() == LLProcess::EXITED;
        if (exited)
        {
            enumerate_and_confirm_parts(mPrefix, mMaximum);
        }
        else
        {
            failed("Bundled converter " + event["string"].asString(), "MP3BatchSoundConversionFailed");
        }
        // Do not destroy the LLProcess or its event pump while LLProcess is
        // still posting this event. Release them on the next main-loop tick.
        mReleaseListener = LLEventPumps::instance().obtain("mainloop").listen(
            "MP3BatchConversionRelease", boost::bind(&MP3ConversionContext::release, this, _1));
        return false;
    }

    bool release(const LLSD&)
    {
        mProcess.reset();
        mSelf.reset();
        return false;
    }

    void failed(const std::string& message, const std::string& notification)
    {
        LL_WARNS("MP3BatchUpload") << message << LL_ENDL;
        close_batch_progress();
        LLNotificationsUtil::add(notification);
    }

    const std::string mInput;
    const std::string mPrefix;
    const F32 mMaximum;
    LLEventStream mPostendPump;
    LLTempBoundListener mPostendListener;
    LLTempBoundListener mReleaseListener;
    LLProcessPtr mProcess;
    MP3ConversionContextPtr mSelf;
};

void convert_and_confirm(const std::vector<std::string>& filenames)
{
    if (filenames.empty()) return;
    const std::string input = filenames.front();
    std::string extension = gDirUtilp->getExtension(input);
    LLStringUtil::toLower(extension);
    if (extension != "mp3") { LLNotificationsUtil::add("MP3BatchSoundNotMp3"); return; }

    const F32 maximum = LLGridManager::instance().isInSecondLife() ? LLVORBIS_CLIP_MAX_TIME : LLVORBIS_CLIP_MAX_TIME_OPENSIM;
    const std::string prefix = gDirUtilp->getTempFilename() + "_mp3part_";
    show_batch_progress("Converting MP3 into upload parts…", 0, 0);
    MP3ConversionContextPtr context = std::make_shared<MP3ConversionContext>(input, prefix, maximum);
    context->start();
}
} // namespace

void mp3_batch_sound_file_picked(const std::vector<std::string>& filenames, LLFilePicker::ELoadFilter)
{
    LL_INFOS("MP3BatchUpload") << "MP3 picker returned " << filenames.size() << " file(s)" << LL_ENDL;
    if (filenames.empty())
    {
        LLNotificationsUtil::add("MP3BatchSoundConversionFailed");
        return;
    }
    convert_and_confirm(filenames);
}
void start_mp3_batch_sound_upload()
{
    if (gAgentCamera.cameraMouselook()) gAgentCamera.changeCameraToDefault();
    LLFilePickerReplyThread::startPicker(boost::bind(&mp3_batch_sound_file_picked, _1, _2), LLFilePicker::FFLOAD_ALL, false);
}

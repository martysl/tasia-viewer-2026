/** @file llmp3batchupload.cpp
 * MP3 conversion and cost-confirmed sound upload, adapted from the old Tasia viewer.
 */
#include "llviewerprecompiledheaders.h"
#include "llmp3batchupload.h"
#include "llagent.h"
#include "llapp.h"
#include "llagentbenefits.h"
#include "llagentcamera.h"
#include "llcallbacklist.h"
#include "lldiriterator.h"
#include "lldir.h"
#include "llfile.h"
#include "lltimer.h"
#include "llfilepicker.h"
#include "llfloaterperms.h"
#include "llfloaterreg.h"
#include "llnotificationsutil.h"
#include "llprocess.h"
#include "llprogressbar.h"
#include "llstatusbar.h"
#include "lltrans.h"
#include "llviewerassetupload.h"
#include "llviewermenufile.h"
#include "llviewernetwork.h"
#include "llvorbisencode.h"
#include <limits>
#include <memory>
#include <vector>

namespace
{
constexpr size_t MAX_PARTS = 512;
class Batch;
std::weak_ptr<Batch> active_batch;

class PartUpload final : public LLNewFileResourceUploadInfo
{
public:
    PartUpload(const std::string& file, const std::string& name, S32 cost,
               std::shared_ptr<Batch> batch);
    LLUUID finishUpload(LLSD& result) override;
    bool failedUpload(LLSD&, std::string&) override;
private:
    std::shared_ptr<Batch> mBatch;
};

class Batch : public std::enable_shared_from_this<Batch>
{
public:
    Batch() : mAgent(gAgent.getID()), mSession(gAgent.getSessionID()),
              mGrid(LLGridManager::getInstance()->getGrid()), mDirectory(gDirUtilp->getTempFilename()) {}
    ~Batch()
    {
        mProcess.reset();
        if (!mOwnDirectory) return;
        LLDirIterator files(mDirectory, "*.wav");
        std::string file;
        while (files.next(file)) LLFile::remove(gDirUtilp->add(mDirectory, file));
        LLFile::rmdir(mDirectory);
    }

    bool valid() const
    {
        return !gDisconnected && !LLApp::isQuitting() &&
            gAgent.getID() == mAgent && gAgent.getSessionID() == mSession &&
            LLGridManager::getInstance()->getGrid() == mGrid &&
            !gAgent.getRegionCapability("NewFileAgentInventory").empty();
    }

    void progress(const std::string& key)
    {
        if (LLFloater* f = LLFloaterReg::showInstance("mp3_batch_progress"))
        {
            f->getChild<LLUICtrl>("status_text")->setValue(LLTrans::getString(key));
            f->getChild<LLUICtrl>("progress_count")->setValue(
                llformat("%d / %d", static_cast<int>(mNext), static_cast<int>(mParts.size())));
            f->getChild<LLProgressBar>("progress_bar")->setValue(
                mParts.empty() ? 0.f : static_cast<F32>(mNext) / mParts.size());
        }
    }

    void fail(const std::string& notification)
    {
        LLFloaterReg::hideInstance("mp3_batch_progress");
        LLNotificationsUtil::add(notification);
    }

    void convert(const std::string& input)
    {
        if (!valid()) { fail("MP3BatchUnavailable"); return; }
        std::string extension = gDirUtilp->getExtension(input);
        LLStringUtil::toLower(extension);
        if (extension != "mp3") { fail("MP3BatchNotMP3"); return; }
        if (LLFile::mkdir(mDirectory) != 0) { fail("MP3BatchConversionFailed"); return; }
        mOwnDirectory = true;
        mName = gDirUtilp->getBaseFileName(input, true);
        LLStringUtil::replaceNonstandardASCII(mName, '?');
        LLStringUtil::replaceChar(mName, '|', '?');
        LLStringUtil::stripNonprintable(mName);
        mName = mName.substr(0, 40);
        std::string converter = gDirUtilp->getExecutableDir();
#if LL_WINDOWS
        gDirUtilp->append(converter, "tasia-ffmpeg.exe");
#else
        gDirUtilp->append(converter, "tasia-ffmpeg");
#endif
        if (!gDirUtilp->fileExists(converter)) { fail("MP3BatchFfmpegMissing"); return; }
        LLProcess::Params params;
        params.executable = converter;
        for (const std::string& arg : std::vector<std::string>{
                 "-nostdin", "-hide_banner", "-v", "error", "-y", "-i", input,
                 "-map", "0:a:0", "-vn", "-ar", "44100", "-ac", "2",
                 "-c:a", "pcm_s16le", "-f", "segment", "-segment_time",
                 llformat("%.2f", LLVORBIS_CLIP_MAX_TIME - 1.f),
                 gDirUtilp->add(mDirectory, "part_%05d.wav")})
            params.args.add(arg);
        params.desc = "Tasia MP3 conversion";
        mProcess = LLProcess::create(params);
        if (!mProcess) { fail("MP3BatchConversionFailed"); return; }
        mTimer.reset();
        progress("MP3BatchConverting");
        auto self = shared_from_this();
        doOnIdleRepeating([self]() { return self->poll(); });
    }

    bool poll()
    {
        const bool too_many_parts = gDirUtilp->fileExists(gDirUtilp->add(mDirectory,
            llformat("part_%05d.wav", static_cast<int>(MAX_PARTS))));
        if (mCancelled || !valid() || too_many_parts || mTimer.getElapsedTimeF32() > 600.f)
        {
            mProcess->kill("Tasia MP3 cancellation/timeout");
            mProcess.reset();
            fail(mCancelled ? "MP3BatchCancelled" : "MP3BatchConversionFailed");
            return true;
        }
        if (mProcess->isRunning()) return false;
        const auto status = mProcess->getStatus();
        bool success = status.mState == LLProcess::EXITED && status.mData == 0;
#if LL_LINUX
        // LL_IGNORE_SIGCHLD can hide the Linux exit code; validate every WAV below.
        success = success || (status.mState == LLProcess::EXITED && status.mData == -1);
#endif
        mProcess.reset();
        if (!success) { fail("MP3BatchConversionFailed"); return true; }
        for (size_t i = 0; i <= MAX_PARTS; ++i)
        {
            const std::string part = gDirUtilp->add(mDirectory, llformat("part_%05d.wav", static_cast<int>(i)));
            if (!gDirUtilp->fileExists(part)) break;
            std::string error;
            if (i == MAX_PARTS || check_for_invalid_wav_formats(part, error))
            { fail("MP3BatchConversionFailed"); return true; }
            mParts.push_back(part);
        }
        LLAssetType::EType sound_type = LLAssetType::AT_SOUND;
        if (mParts.empty() || !gStatusBar ||
            !LLAgentBenefitsMgr::current().findUploadCost(sound_type, mCost) || mCost < 0)
        { fail("MP3BatchCostUnavailable"); return true; }
        const S64 total = static_cast<S64>(mCost) * mParts.size();
        if (total > gStatusBar->getBalance() || total > std::numeric_limits<S32>::max())
        { fail("MP3BatchCannotAfford"); return true; }
        LLFloaterReg::hideInstance("mp3_batch_progress");
        LLSD args;
        args["COUNT"] = static_cast<S32>(mParts.size());
        args["UNIT_COST"] = mCost;
        args["TOTAL_COST"] = static_cast<S32>(total);
        auto self = shared_from_this();
        LLNotificationsUtil::add("ConfirmMP3BatchUpload", args, LLSD(),
            [self](const LLSD& notification, const LLSD& response)
            {
                if (!self->mCancelled && LLNotificationsUtil::getSelectedOption(notification, response) == 0)
                    self->next();
            });
        return true;
    }

    void next()
    {
        if (mCancelled || !valid()) { fail("MP3BatchCancelled"); return; }
        if (mNext == mParts.size())
        {
            LLFloaterReg::hideInstance("mp3_batch_progress");
            LLSD args; args["COUNT"] = static_cast<S32>(mNext);
            LLNotificationsUtil::add("MP3BatchFinished", args);
            return;
        }
        // Recheck the quote and remaining balance after the confirmation delay.
        S32 current_cost = -1;
        LLAssetType::EType sound_type = LLAssetType::AT_SOUND;
        if (!gStatusBar || !LLAgentBenefitsMgr::current().findUploadCost(sound_type, current_cost) ||
            current_cost != mCost || static_cast<S64>(mParts.size() - mNext) * mCost > gStatusBar->getBalance())
        { fail("MP3BatchCostUnavailable"); return; }
        progress("MP3BatchUploading");
        LLResourceUploadInfo::ptr_t info = std::make_shared<PartUpload>(mParts[mNext],
            mName + llformat(" - %03d", static_cast<int>(mNext + 1)), mCost, shared_from_this());
        LLViewerAssetUpload::EnqueueInventoryUpload(gAgent.getRegionCapability("NewFileAgentInventory"), info);
    }

    void completed(bool success)
    {
        if (!success) { fail("MP3BatchUploadFailed"); return; }
        ++mNext;
        // Leave the current upload callback before queuing the next upload.
        auto self = shared_from_this();
        doOnIdleOneTime([self]() { self->next(); });
    }

    bool mCancelled = false;
private:
    LLUUID mAgent, mSession;
    std::string mGrid, mDirectory, mName;
    LLProcessPtr mProcess;
    LLTimer mTimer;
    std::vector<std::string> mParts;
    size_t mNext = 0;
    S32 mCost = 0;
    bool mOwnDirectory = false;
};

PartUpload::PartUpload(const std::string& file, const std::string& name, S32 cost, std::shared_ptr<Batch> batch)
    : LLNewFileResourceUploadInfo(file, name, "Converted MP3 segment", 0,
          LLFolderType::FT_SOUND, LLInventoryType::IT_SOUND,
          LLFloaterPerms::getNextOwnerPerms("Uploads"), LLFloaterPerms::getGroupPerms("Uploads"),
          LLFloaterPerms::getEveryonePerms("Uploads"), cost), mBatch(std::move(batch)) {}

LLUUID PartUpload::finishUpload(LLSD& result)
{
    LLUUID item = LLNewFileResourceUploadInfo::finishUpload(result);
    mBatch->completed(item.notNull() && result["new_asset"].asUUID().notNull());
    return item;
}

bool PartUpload::failedUpload(LLSD&, std::string&)
{
    mBatch->completed(false);
    return true; // A localized batch error is shown; do not expose server responses.
}
} // namespace

void start_mp3_batch_sound_upload()
{
    if (!active_batch.expired()) { LLNotificationsUtil::add("MP3BatchBusy"); return; }
    if (gAgentCamera.cameraMouselook()) gAgentCamera.changeCameraToDefault();
    LLFilePickerReplyThread::startPicker(
        [](const std::vector<std::string>& files, LLFilePicker::ELoadFilter, LLFilePicker::ESaveFilter)
        {
            if (files.size() != 1) return; // Picker cancellation is not a conversion failure.
            if (!active_batch.expired()) { LLNotificationsUtil::add("MP3BatchBusy"); return; }
            auto batch = std::make_shared<Batch>();
            active_batch = batch;
            batch->convert(files.front());
        }, LLFilePicker::FFLOAD_ALL, false);
}

void cancel_mp3_batch_sound_upload()
{
    if (auto batch = active_batch.lock()) batch->mCancelled = true;
}

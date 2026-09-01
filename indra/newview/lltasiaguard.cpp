#include "llviewerprecompiledheaders.h"
#include "lltasiaguard.h"

#include "llagent.h"
#include "llbutton.h"
#include "llcorehttputil.h"
#include "llcoros.h"
#include "llfloaterreg.h"
#include "llnotificationsutil.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "lltextbox.h"
#include "lluicolortable.h"
#include "llvoavatarself.h"
#include "llviewercontrol.h"

const std::string LLTasiaGuardFloater::SELF_UNBAN_URL = "https://api.tasiaviewer.work/api/v1/unban/app.php";
const std::string LLTasiaGuardFloater::MOM_UUID = "43827618-1993-43d8-bf6b-fc966a943381";

LLTasiaGuardFloater::LLTasiaGuardFloater(const LLSD& seed)
    : LLFloater(seed)
{
    mCommitCallbackRegistrar.add("TasiaGuard.Unban", boost::bind(&LLTasiaGuardFloater::onUnban, this));
}

bool LLTasiaGuardFloater::postBuild()
{
    mIP = getChild<LLLineEditor>("unban_ip");
    mFirstName = getChild<LLLineEditor>("unban_first_name");
    mLastName = getChild<LLLineEditor>("unban_last_name");
    mStatus = getChild<LLTextBox>("unban_status");
    mUnbanBtn = getChild<LLButton>("unban_btn");
    return true;
}

void LLTasiaGuardFloater::onOpen(const LLSD& key)
{
    std::string ip = gSavedSettings.getString("ExternalIP");
    if (ip.empty()) ip = "auto-detected";

    mIP->setText(ip);
    mIP->setEnabled(false);

    std::string full_name = gAgentAvatarp->getFullname();
    std::string::size_type pos = full_name.find(' ');
    if (pos != std::string::npos)
    {
        mFirstName->setText(full_name.substr(0, pos));
        mLastName->setText(full_name.substr(pos + 1));
    }
    else
    {
        mFirstName->setText(full_name);
        mLastName->setText(LLSD().asString());
    }

    setStatus("");
    mUnbanBtn->setEnabled(true);
}

void LLTasiaGuardFloater::setStatus(const std::string& text, bool is_error)
{
    if (mStatus)
    {
        mStatus->setText(text);
        mStatus->setColor(is_error ? LLUIColorTable::instance().getColor("Red") : LLUIColorTable::instance().getColor("LtGray_75"));
    }
}

// Coroutine for HTTP unban request
static void unbanCoro(std::string url, LLSD post_data)
{
    LLCore::HttpRequest::ptr_t httpRequest(new LLCore::HttpRequest);
    LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t httpAdapter(
        new LLCoreHttpUtil::HttpCoroutineAdapter("TasiaGuardUnban", LLCore::HttpRequest::DEFAULT_POLICY_ID));
    LLCore::HttpHeaders::ptr_t httpHeaders(new LLCore::HttpHeaders);
    LLCore::HttpOptions::ptr_t httpOptions(new LLCore::HttpOptions);

    httpHeaders->append("Content-Type", "application/json");
    httpOptions->setTimeout(15);

    LLSD result = httpAdapter->postJsonAndSuspend(httpRequest, url, post_data, httpOptions, httpHeaders);

    LLSD http_results = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
    LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(http_results);

    if (status)
    {
        LLSD data = result["data"];
        std::string msg = data["message"].asString();
        LLNotificationsUtil::add("TasiaGuardUnbanComplete");
    }
    else
    {
        LL_WARNS("TasiaGuard") << "Unban request failed: " << status.toString() << LL_ENDL;
        LLNotificationsUtil::add("TasiaGuardUnbanComplete");
    }
}

void LLTasiaGuardFloater::onUnban()
{
    std::string first_name = mFirstName->getText();
    std::string last_name = mLastName->getText();

    if (first_name.empty() || last_name.empty())
    {
        setStatus("Please fill in avatar name", true);
        return;
    }

    mUnbanBtn->setEnabled(false);
    setStatus("Requesting...");

    LLSD post_data;
    post_data["ip"] = gSavedSettings.getString("ExternalIP");
    post_data["uuid"] = MOM_UUID;
    post_data["first_name"] = first_name;
    post_data["last_name"] = last_name;
    post_data["auth_name"] = "Tasia";

    // Start coroutine
    LLCoros::instance().launch("TasiaGuardUnbanCoro",
        boost::bind(&unbanCoro, SELF_UNBAN_URL, post_data));

    setStatus("Request sent. Check notifications.");
    mUnbanBtn->setEnabled(true);
}

void registerTasiaGuardFloater()
{
    LLFloaterReg::add("tasiaguard", "floater_tasiaguard.xml",
        (LLFloaterBuildFunc)&LLFloaterReg::build<LLTasiaGuardFloater>);
}

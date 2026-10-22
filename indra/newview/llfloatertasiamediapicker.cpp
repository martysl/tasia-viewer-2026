#include "llviewerprecompiledheaders.h"
#include "llfloatertasiamediapicker.h"
#include "llagent.h"
#include "llbutton.h"
#include "llcallbacklist.h"
#include "lldir.h"
#include "llfloaterreg.h"
#include "lllineeditor.h"
#include "llmediactrl.h"
#include "llpluginclassmedia.h"
#include "lltasia_giphy_key.h"
#include "lltasia_klipy_key.h"
#include "lluri.h"
#include "llviewercontrol.h"
#include <boost/json.hpp>

namespace
{
bool validSelection(const std::string& url, const std::string& provider)
{
    const auto space = url.find(' ');
    if (space != std::string::npos)
    {
        if (!validSelection(url.substr(0, space), provider)) return false;
        const auto asset = url.substr(space + 1);
        if (asset.size() > 2048 || asset.find_first_of("\r\n\t ") != std::string::npos) return false;
        LLURI uri(asset);
        const auto host = uri.hostName();
        const std::string suffix = provider == "giphy" ? ".giphy.com" : ".klipy.com";
        return uri.scheme() == "https" && uri.userName().empty() && uri.password().empty() &&
            uri.defaultPort() && host.size() > suffix.size() &&
            host.compare(host.size() - suffix.size(), suffix.size(), suffix) == 0 &&
            host != "api.giphy.com" && host != "api.klipy.com" &&
            uri.escapedQuery().find("api_key") == std::string::npos &&
            uri.escapedQuery().find("apikey") == std::string::npos;
    }
    if (url.empty() || url.size() > 2048 || url.find_first_of("\r\n\t ") != std::string::npos)
        return false;
    LLURI uri(url);
    const auto host = uri.hostName();
    return uri.scheme() == "https" && uri.userName().empty() && uri.password().empty() &&
        uri.defaultPort() && uri.escapedQuery().empty() && url.find('#') == std::string::npos &&
        ((provider == "giphy" && (host == "giphy.com" || host == "www.giphy.com")) ||
         (provider == "klipy" && host == "klipy.com"));
}
}

LLFloaterTasiaMediaPicker::LLFloaterTasiaMediaPicker(const LLSD& key) : LLFloater(key) {}

LLFloaterTasiaMediaPicker::~LLFloaterTasiaMediaPicker()
{
    if (mBrowser) mBrowser->remObserver(this);
}

bool LLFloaterTasiaMediaPicker::postBuild()
{
    mBrowser = getChild<LLMediaCtrl>("media_picker");
    mBrowser->setTrustedContent(false);
    mBrowser->setAllowFileDownload(false);
    mBrowser->addObserver(this);
    mKeyEditor = getChild<LLLineEditor>("session_key");
    getChild<LLButton>("apply_key")->setClickedCallback([this](LLUICtrl*, const LLSD&)
    {
        const std::string value = mKeyEditor->getText();
        if (value.size() > 256) return;
        gSavedSettings.setString(mProvider == "klipy" ? "TasiaKlipyAPIKey" : "TasiaGiphyAPIKey", value);
        mKeyEditor->clear();
        load();
    });
    return true;
}

void LLFloaterTasiaMediaPicker::show(const std::string& provider,
    std::function<void(const std::string&)> select)
{
    if (provider != "giphy" && provider != "klipy") return;
    if (!gSavedSettings.getBOOL(provider == "giphy" ? "TasiaGiphyEnabled" : "TasiaKlipyEnabled")) return;
    auto* floater = LLFloaterReg::getTypedInstance<LLFloaterTasiaMediaPicker>("tasia_media_picker");
    if (!floater) return;
    floater->mProvider = provider;
    floater->mSelect = std::move(select);
    floater->mSession = gAgent.getSessionID();
    floater->setTitle(provider == "giphy" ? "GIPHY Web SDK" : "KLIPY GIFs / Clips");
    floater->openFloater();
    floater->load();
}

void LLFloaterTasiaMediaPicker::load()
{
    mInitialized = false;
    mNonce.generate();
    mKeyEditor->clear();
    // Unload the previous context, including credentials and in-flight fetches.
    mBrowser->unloadMediaSource();
    std::string path = gDirUtilp->findSkinnedFilename("html", "tasia_media/index.html");
    if (path.empty()) return;
    std::replace(path.begin(), path.end(), '\\', '/');
    const std::string allowed = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~/:";
    mPageURL = (path[0] == '/' ? "file://" : "file:///") + LLURI::escape(path, allowed);
    mBrowser->navigateTo(mPageURL);
}

void LLFloaterTasiaMediaPicker::initializePage()
{
    if (mInitialized || !mBrowser->getMediaPlugin()) return;
    const std::string key = mProvider == "klipy" ? LLTasiaKlipyKey::getConfiguredAPIKey() :
        LLTasiaGiphyKey::getConfiguredAPIKey();
    boost::json::object config{{"provider", mProvider}, {"key", key}, {"nonce", mNonce.asString()},
        {"language", gSavedSettings.getString("Language") == "pl" ? "pl" : "en"}};
    // Credentials are passed in memory, never in a page URL or generated HTML.
    mBrowser->getMediaPlugin()->executeJavaScript("window.tasiaMediaInit(" + boost::json::serialize(config) + ");");
    mInitialized = true;
}

void LLFloaterTasiaMediaPicker::handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event)
{
    if (!getVisible() || self != mBrowser->getMediaPlugin()) return;
    if (event == MEDIA_EVENT_NAVIGATE_COMPLETE && self->getNavigateURI() == mPageURL)
    {
        initializePage();
    }
    else if (event == MEDIA_EVENT_LOCATION_CHANGED)
    {
        const std::string location = self->getLocation();
        const std::string prefix = mPageURL + "#select=" + mNonce.asString() + ":";
        if (!mInitialized || location.compare(0, prefix.size(), prefix) != 0 ||
            gAgent.getSessionID() != mSession || gDisconnected ||
            !gSavedSettings.getBOOL(mProvider == "giphy" ? "TasiaGiphyEnabled" : "TasiaKlipyEnabled")) return;
        const std::string url = LLURI::unescape(location.substr(prefix.size()));
        if (!validSelection(url, mProvider) || !mSelect) return;
        auto select = std::move(mSelect);
        const auto handle = getHandle();
        const auto nonce = mNonce;
        const auto session = mSession;
        mInitialized = false; // Accept at most one selection from this page.
        // Closing unloads LLViewerMediaImpl. Its emitEvent() is still on the
        // stack here and accesses mObjectList AFTER notifying observers. Never
        // destroy that emitter from its own observer callback (AV at emitEvent).
        doOnIdleOneTime([handle, nonce, session, select = std::move(select), url]() mutable
        {
            auto* picker = dynamic_cast<LLFloaterTasiaMediaPicker*>(handle.get());
            if (!picker || !picker->getVisible() || picker->mNonce != nonce ||
                picker->mSession != session || gAgent.getSessionID() != session || gDisconnected ||
                !gSavedSettings.getBOOL(picker->mProvider == "giphy" ? "TasiaGiphyEnabled" : "TasiaKlipyEnabled"))
                return;
            picker->closeFloater();
            select(url); // Originating weak editor callback; never auto-send.
        });
    }
}

void LLFloaterTasiaMediaPicker::onClose(bool app_quitting)
{
    mSelect = {};
    mNonce.setNull();
    mInitialized = false;
    mKeyEditor->clear();
    mBrowser->unloadMediaSource();
}

#include "llviewerprecompiledheaders.h"
#include "lltasia_config_fetch.h"
#include "llcorehttputil.h"
#include "llcoros.h"
#include "lluri.h"
#include "llapp.h"

void tasiaFetchConfig(const std::string& url, size_t limit, TasiaConfigCallback callback, bool allow_http)
{
    const LLURI uri(url);
    if ((uri.scheme() != "https" && !(allow_http && uri.scheme() == "http")) ||
        uri.hostName().empty() || url.size() > 2048 ||
        url.find_first_of("@?#\\\r\n\t ") != std::string::npos || limit == 0 || limit > 1024 * 1024)
    {
        callback(false, "Invalid configuration URL.");
        return;
    }
    LLCoros::instance().launch("TasiaConfigFetch", [url, limit, callback]()
    {
        auto options = std::make_shared<LLCore::HttpOptions>();
        options->setSSLVerifyPeer(true);
        options->setSSLVerifyHost(true);
        options->setFollowRedirects(false);
        options->setRetries(0);
        options->setTimeout(10);
        options->setTransferTimeout(10);
        options->setBodyLimit(limit);
        auto request = std::make_shared<LLCore::HttpRequest>();
        auto adapter = std::make_shared<LLCoreHttpUtil::HttpCoroutineAdapter>(
            "TasiaConfigFetch", LLCore::HttpRequest::DEFAULT_POLICY_ID);
        const LLSD result = adapter->getRawAndSuspend(request, url, options);
        if (LLApp::isQuitting()) return;
        const auto status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(
            result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS]);
        if (!status || status.getType() != 200)
        {
            callback(false, "Configuration request failed or returned partial data.");
            return;
        }
        const auto& bytes = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_RAW].asBinary();
        if (bytes.empty() || bytes.size() > limit)
        {
            callback(false, "Configuration response is empty or too large.");
            return;
        }
        callback(true, std::string(bytes.begin(), bytes.end()));
    });
}

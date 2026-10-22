/** @file llfloatertasiamediapicker.h
 * Local, untrusted CEF picker using the official GIPHY Web fetch SDK and KLIPY.
 */
#pragma once
#include "llfloater.h"
#include "llviewermedia.h"
#include <functional>

class LLMediaCtrl;
class LLLineEditor;

class LLFloaterTasiaMediaPicker final : public LLFloater, public LLViewerMediaObserver
{
public:
    explicit LLFloaterTasiaMediaPicker(const LLSD& key);
    ~LLFloaterTasiaMediaPicker() override;
    bool postBuild() override;
    void onClose(bool app_quitting) override;
    void handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event) override;
    static void show(const std::string& provider, std::function<void(const std::string&)> select);
private:
    void load();
    void initializePage();
    LLMediaCtrl* mBrowser = nullptr;
    LLLineEditor* mKeyEditor = nullptr;
    std::string mProvider;
    std::string mPageURL;
    LLUUID mNonce;
    LLUUID mSession;
    bool mInitialized = false;
    std::function<void(const std::string&)> mSelect;
};

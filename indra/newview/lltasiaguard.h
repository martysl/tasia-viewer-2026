#ifndef LL_TASIAGUARD_H
#define LL_TASIAGUARD_H

#include "llfloater.h"
#include "lllineeditor.h"
#include "lltextbox.h"

class LLTasiaGuardFloater : public LLFloater
{
public:
    LLTasiaGuardFloater(const LLSD& seed);
    ~LLTasiaGuardFloater() = default;

    bool postBuild() override;
    void onOpen(const LLSD& key) override;

private:
    void onUnban();
    void setStatus(const std::string& text, bool is_error = false);

    LLLineEditor* mIP;
    LLLineEditor* mFirstName;
    LLLineEditor* mLastName;
    LLTextBox* mStatus;
    LLButton* mUnbanBtn;

    static const std::string SELF_UNBAN_URL;
    static const std::string MOM_UUID;
};

#endif

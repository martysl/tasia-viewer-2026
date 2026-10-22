#ifndef LL_FLOATER_UPLOAD_CLIPBOARD_H
#define LL_FLOATER_UPLOAD_CLIPBOARD_H
#include "llfloater.h"

// Only releases files registered by native clipboard capture, never arbitrary paths.
void tasia_release_clipboard_image(const std::string& filename);

class LLFloaterUploadClipboard final : public LLFloater
{
public:
    explicit LLFloaterUploadClipboard(const LLSD& key) : LLFloater(key) {}
    bool postBuild() override;
    bool handleKeyHere(KEY key, MASK mask) override;
private:
    void paste();
};
#endif

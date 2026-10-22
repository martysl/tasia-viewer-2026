/** @file llfloateruploadclipboard.cpp
 * Native clipboard capture; nothing is uploaded until the normal preview confirms it.
 */
#include "llviewerprecompiledheaders.h"
#include "llfloateruploadclipboard.h"
#include "llfloaterreg.h"
#include "lldir.h"
#include "llfile.h"
#include "llimagepng.h"
#include "lltrans.h"
#include <set>
#if LL_WINDOWS
#include "llwin32headers.h"
#include <cstring>
#include <vector>
#endif

namespace
{
std::set<std::string> captured_files;
#if LL_WINDOWS
// No shell or subprocess is used; clipboard handles remain owned by Windows.
bool capture_clipboard_png(const std::string& filename)
{
    if (!OpenClipboard(nullptr)) return false;
    struct Close { ~Close() { CloseClipboard(); } } close;
    const UINT png_format = RegisterClipboardFormatW(L"PNG");
    if (HANDLE handle = GetClipboardData(png_format))
    {
        const SIZE_T size = GlobalSize(handle);
        if (size >= 24 && size <= 64 * 1024 * 1024)
        {
            const void* data = GlobalLock(handle);
            if (data)
            {
                const unsigned char signature[] = {137,80,78,71,13,10,26,10};
                const U8* bytes = static_cast<const U8*>(data);
                const auto read32 = [](const U8* p) -> U32 {
                    return (U32(p[0]) << 24) | (U32(p[1]) << 16) | (U32(p[2]) << 8) | U32(p[3]);
                };
                const U32 width = read32(bytes + 16), height = read32(bytes + 20);
                const bool bounded = width && height && width <= 8192 && height <= 8192 &&
                    static_cast<U64>(width) * height <= 16 * 1024 * 1024;
                LLFILE* file = bounded && std::memcmp(data, signature, 8) == 0 &&
                    std::memcmp(bytes + 12, "IHDR", 4) == 0 ? LLFile::fopen(filename, "wb") : nullptr;
                bool ok = file && fwrite(data, 1, size, file) == size;
                if (file) { if (fclose(file) != 0) ok = false; }
                GlobalUnlock(handle);
                if (ok) return true;
                LLFile::remove(filename);
            }
        }
    }

    // CF_BITMAP covers screenshots and applications without the PNG clipboard format.
    HBITMAP bitmap = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
    if (!bitmap) return false;
    BITMAP info{};
    if (!GetObject(bitmap, sizeof(info), &info) || info.bmWidth <= 0 || info.bmHeight <= 0 ||
        info.bmWidth > 8192 || info.bmHeight > 8192 ||
        static_cast<U64>(info.bmWidth) * info.bmHeight > 16 * 1024 * 1024) return false;
    BITMAPINFO dib{};
    dib.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    dib.bmiHeader.biWidth = info.bmWidth;
    dib.bmiHeader.biHeight = info.bmHeight; // bottom-up, matching LLImageRaw
    dib.bmiHeader.biPlanes = 1;
    dib.bmiHeader.biBitCount = 32;
    dib.bmiHeader.biCompression = BI_RGB;
    std::vector<U8> pixels(static_cast<size_t>(info.bmWidth) * info.bmHeight * 4);
    HDC dc = GetDC(nullptr);
    if (!dc) return false;
    const int rows = GetDIBits(dc, bitmap, 0, info.bmHeight, pixels.data(), &dib, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    if (rows != info.bmHeight) return false;
    LLPointer<LLImageRaw> raw = new LLImageRaw(info.bmWidth, info.bmHeight, 4);
    U8* dst = raw->getData();
    if (!dst) return false;
    // GDI's BI_RGB alpha byte is undefined. Preserve alpha through PNG above;
    // make the bitmap fallback opaque rather than turning screenshots invisible.
    for (size_t i = 0; i < pixels.size(); i += 4)
    {
        dst[i] = pixels[i + 2]; dst[i + 1] = pixels[i + 1];
        dst[i + 2] = pixels[i]; dst[i + 3] = 255;
    }
    LLPointer<LLImagePNG> png = new LLImagePNG;
    return png->encode(raw, 0.f) && png->save(filename);
}
#endif
}

void tasia_release_clipboard_image(const std::string& filename)
{
    auto owned = captured_files.find(filename);
    if (owned != captured_files.end())
    {
        LLFile::remove(*owned);
        captured_files.erase(owned);
    }
}

bool LLFloaterUploadClipboard::postBuild()
{
    getChild<LLUICtrl>("paste_btn")->setCommitCallback([this](LLUICtrl*, const LLSD&) { paste(); });
    getChild<LLUICtrl>("close_btn")->setCommitCallback([this](LLUICtrl*, const LLSD&) { closeFloater(false); });
    return true;
}

bool LLFloaterUploadClipboard::handleKeyHere(KEY key, MASK mask)
{
    if (key == 'V' && mask == MASK_CONTROL) { paste(); return true; }
    return LLFloater::handleKeyHere(key, mask);
}

void LLFloaterUploadClipboard::paste()
{
#if LL_WINDOWS
    const std::string filename = gDirUtilp->getTempFilename() + ".png";
    if (capture_clipboard_png(filename))
    {
        captured_files.insert(filename);
        LLSD args;
        args["filename"] = filename;
        args["temporary_clipboard_image"] = true;
        if (LLFloaterReg::showInstance("upload_image", args))
        {
            tasia_release_clipboard_image(filename);
            getChild<LLUICtrl>("status_text")->setValue(LLTrans::getString("ClipboardPreviewOpened"));
            return;
        }
    }
    tasia_release_clipboard_image(filename);
    LLFile::remove(filename);
    getChild<LLUICtrl>("status_text")->setValue(LLTrans::getString("ClipboardImageMissing"));
#else
    getChild<LLUICtrl>("status_text")->setValue(LLTrans::getString("ClipboardPlatformUnsupported"));
#endif
}

#ifndef LL_TASIA_TRANSLATION_POLICY_H
#define LL_TASIA_TRANSLATION_POLICY_H
#include <string>

namespace TasiaTranslation
{
// Conservative text gate. Network chat does not mark unknown remote gestures.
inline bool isOrdinaryText(const std::string& text)
{
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos || text.size() > 8192) return false;
    if (text[begin] == '/' || text.find('\n', begin) != std::string::npos) return false;
    for (const char* prefix : {"http://", "https://", "secondlife://", "hop://"})
        if (text.compare(begin, std::char_traits<char>::length(prefix), prefix) == 0) return false;
    bool has_letter = false;
    for (size_t i = 0; i < text.size();)
    {
        unsigned int c = static_cast<unsigned char>(text[i++]);
        if (c < 0x80)
        {
            if (c == '\r' || c == '\n') return false;
            has_letter = has_letter || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
            continue;
        }
        const unsigned int first = c;
        const unsigned int extra = first >= 0xf0 ? 3 : first >= 0xe0 ? 2 : first >= 0xc2 ? 1 : 0;
        if (!extra || first > 0xf4 || i + extra > text.size()) return false;
        c &= (1u << (6 - extra)) - 1;
        for (unsigned int n = 0; n < extra; ++n)
        {
            unsigned int next = static_cast<unsigned char>(text[i++]);
            if ((next & 0xc0) != 0x80) return false;
            c = (c << 6) | (next & 0x3f);
        }
        if ((extra == 1 && c < 0x80) || (extra == 2 && c < 0x800) ||
            (extra == 3 && c < 0x10000) || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) return false;
        if (c == 0x2028 || c == 0x2029) return false;
        // Conservative common writing-system ranges; emoji/box drawing alone
        // must not count as ordinary text. This is not gesture provenance.
        has_letter = has_letter || (c >= 0xc0 && c <= 0x2af) ||
            (c >= 0x370 && c <= 0x52f) || (c >= 0x590 && c <= 0x6ff) ||
            (c >= 0x900 && c <= 0x97f) || (c >= 0xe00 && c <= 0xe7f) ||
            (c >= 0x3040 && c <= 0x30ff) || (c >= 0x3400 && c <= 0x9fff) ||
            (c >= 0xac00 && c <= 0xd7af);
    }
    return has_letter;
}
}
#endif

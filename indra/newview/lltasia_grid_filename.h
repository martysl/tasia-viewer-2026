/** Grid identifiers are namespace keys, not filesystem path components. */
#ifndef LL_TASIA_GRID_FILENAME_H
#define LL_TASIA_GRID_FILENAME_H

#include <cstdio>
#include <istream>
#include <string>
#include "llmd5.h"

namespace TasiaGridFilename
{
inline std::string component(const std::string& key)
{
    if (key.empty()) return "~empty";
    static constexpr char hex[] = "0123456789abcdef";
    std::string result;
    for (std::size_t i = 0; i < key.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(key[i]);
        const bool safe = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || (c == '.' && i + 1 < key.size());
        if (safe) result += static_cast<char>(c);
        else
        {
            result += '~';
            result += hex[c >> 4];
            result += hex[c & 15];
        }
    }
    // Windows reserves these names even when followed by an extension.
    const std::string stem = result.substr(0, result.find('.'));
    const bool device = stem == "con" || stem == "prn" || stem == "aux" || stem == "nul" ||
        (stem.size() == 4 && (stem.compare(0, 3, "com") == 0 || stem.compare(0, 3, "lpt") == 0) &&
         stem[3] >= '1' && stem[3] <= '9');
    if (device) result = "~" + std::string(1, hex[static_cast<unsigned char>(key[0]) >> 4]) +
        hex[static_cast<unsigned char>(key[0]) & 15] + result.substr(1);
    if (result.size() > 96)
    {
        // Bounded deterministic cache key, not a security/authentication digest.
        LLMD5 hash;
        hash.update(key);
        hash.finalize();
        char digest[MD5HEX_STR_SIZE];
        hash.hex_digest(digest);
        result = result.substr(0, 62) + "~h" + digest;
    }
    return result;
}
}
#endif

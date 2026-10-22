/**
 * @file llcurrency_symbol.h
 * @brief Small, testable helpers for viewer currency-symbol substitution.
 */
#ifndef LL_LLCURRENCY_SYMBOL_H
#define LL_LLCURRENCY_SYMBOL_H

#include <string>

namespace LLCurrencySymbol
{
inline std::string validated(const std::string& value)
{
    if (value.empty() || value.size() > 16) return {};
    for (unsigned char character : value)
    {
        if (character < 0x20 || character == 0x7f) return {};
    }
    return value;
}

inline std::string grid(const std::string& value)
{
    const std::string symbol = validated(value);
    return symbol.empty() ? "L$" : symbol;
}

inline std::string region(const std::string& value)
{
    return validated(value);
}

inline void replaceLindenSymbol(std::string& text, const std::string& symbol)
{
    if (symbol == "L$") return;
    if (symbol.empty()) return;
    std::string::size_type position = 0;
    while ((position = text.find("L$", position)) != std::string::npos)
    {
        text.replace(position, 2, symbol);
        position += symbol.size();
    }
}
}

#endif // LL_LLCURRENCY_SYMBOL_H

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
        if (character <= 0x20 || character == 0x7f || character == '[' || character == ']') return {};
    }
    return value;
}

inline std::string grid(const std::string& value, bool opensim)
{
    const std::string symbol = validated(value);
    return symbol.empty() ? (opensim ? "OS$" : "L$") : symbol;
}

inline std::string region(const std::string& value, bool opensim)
{
    return opensim ? validated(value) : std::string();
}

inline std::string active(const std::string& grid_symbol, const std::string& region_symbol)
{
    return region_symbol.empty() ? grid_symbol : region_symbol;
}

inline void replaceCurrencyToken(std::string& text, const std::string& symbol)
{
    static const std::string token = "[CURRENCY]";
    std::string::size_type position = 0;
    while ((position = text.find(token, position)) != std::string::npos)
    {
        text.replace(position, token.size(), symbol);
        position += symbol.size();
    }
}

// This helper is reserved for formatted numeric money values, never arbitrary
// UI, profile, chat, or other user-provided text.
inline void replaceFormattedAmount(std::string& text, const std::string& symbol)
{
    if (symbol.empty() || symbol == "L$") return;
    std::string::size_type position = 0;
    while ((position = text.find("L$", position)) != std::string::npos)
    {
        text.replace(position, 2, symbol);
        position += symbol.size();
    }
}
}

#endif // LL_LLCURRENCY_SYMBOL_H

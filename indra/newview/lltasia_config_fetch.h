#pragma once
#include <cstddef>
#include <functional>
#include <string>

// No credentials, redirects or user identifiers; limits enforced during receive.
using TasiaConfigCallback = std::function<void(bool, const std::string&)>;
void tasiaFetchConfig(const std::string& url, size_t limit, TasiaConfigCallback callback,
    bool allow_http = false);

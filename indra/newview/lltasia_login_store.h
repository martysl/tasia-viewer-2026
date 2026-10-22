/** Windows SQLite account history. Password blobs are protected with user DPAPI. */
#pragma once

#include <string>
#include <vector>

namespace TasiaLoginStore
{
// This account-history database is implemented only on Windows with DPAPI.
bool isAvailable();

struct Account
{
    std::string grid;
    std::string user;
    std::string label;
    std::string identifier;
};

bool list(const std::string& path, const std::string& grid, std::vector<Account>& accounts);
bool password(const std::string& path, const std::string& grid,
              const std::string& user, std::string& result);
// A null password preserves an existing encrypted blob; an empty password removes it.
bool save(const std::string& path, const Account& account, const std::string* password);
bool erase(const std::string& path, const std::string& grid, const std::string& user);
bool clearPassword(const std::string& path, const std::string& grid, const std::string& user);
// Import only this application's SQLite format. Existing accounts are not overwritten.
// DPAPI-protected passwords can only be imported by the same Windows user.
bool importDatabase(const std::string& path, const std::string& source, int& imported);
}

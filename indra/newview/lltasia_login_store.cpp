#include "lltasia_login_store.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#include <winsqlite/winsqlite3.h>
#include <memory>
#include <utility>

namespace
{
using Database = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
constexpr int APPLICATION_ID = 0x54415349;

Statement prepare(sqlite3* db, const char* sql)
{
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        if (stmt) sqlite3_finalize(stmt);
        stmt = nullptr;
    }
    return Statement(stmt, sqlite3_finalize);
}

bool execute(sqlite3* db, const char* sql)
{
    return sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool integerPragma(sqlite3* db, const char* sql, int expected)
{
    auto stmt = prepare(db, sql);
    return stmt && sqlite3_step(stmt.get()) == SQLITE_ROW &&
           sqlite3_column_int(stmt.get(), 0) == expected;
}

Database open(const std::string& path, bool read_only = false)
{
    sqlite3* raw = nullptr;
    int flags = read_only ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    int rc = sqlite3_open_v2(path.c_str(), &raw, flags, nullptr);
    Database db(raw, sqlite3_close);
    if (rc != SQLITE_OK) return Database(nullptr, sqlite3_close);
    sqlite3_busy_timeout(raw, 3000);
    sqlite3_limit(raw, SQLITE_LIMIT_LENGTH, 1024 * 1024);
    if (!read_only && integerPragma(raw, "PRAGMA application_id", 0) &&
        integerPragma(raw, "PRAGMA user_version", 0))
    {
        // Do not adopt an unrelated database just because it has no application id.
        auto tables = prepare(raw, "SELECT count(*) FROM sqlite_master WHERE type='table'");
        if (!tables || sqlite3_step(tables.get()) != SQLITE_ROW ||
            sqlite3_column_int(tables.get(), 0) != 0) return Database(nullptr, sqlite3_close);
        tables.reset();
        if (!execute(raw, "BEGIN IMMEDIATE;"
            "CREATE TABLE accounts(grid TEXT NOT NULL,user TEXT NOT NULL,label TEXT NOT NULL,"
            "identifier TEXT NOT NULL,password BLOB,PRIMARY KEY(grid,user));"
            "PRAGMA application_id=1413567305;PRAGMA user_version=1;COMMIT;"))
            return Database(nullptr, sqlite3_close);
    }
    if (!integerPragma(raw, "PRAGMA application_id", APPLICATION_ID) ||
        !integerPragma(raw, "PRAGMA user_version", 1)) return Database(nullptr, sqlite3_close);
    if (!read_only && !execute(raw, "PRAGMA secure_delete=ON")) return Database(nullptr, sqlite3_close);
    return db;
}

bool bind(sqlite3_stmt* stmt, int index, const std::string& value)
{
    return sqlite3_bind_text(stmt, index, value.data(), static_cast<int>(value.size()),
                             SQLITE_TRANSIENT) == SQLITE_OK;
}

std::string text(sqlite3_stmt* stmt, int index)
{
    const auto* value = sqlite3_column_text(stmt, index);
    return value ? std::string(reinterpret_cast<const char*>(value),
                              sqlite3_column_bytes(stmt, index)) : std::string();
}

bool valid(const TasiaLoginStore::Account& a)
{
    return !a.grid.empty() && !a.user.empty() && !a.label.empty() && !a.identifier.empty() &&
           a.grid.size() <= 2048 && a.user.size() <= 512 && a.label.size() <= 512 &&
           a.identifier.size() <= 16384;
}

bool protect(const std::string& value, std::vector<unsigned char>& encrypted)
{
    if (value.empty()) return true;
    if (value.size() > 4096) return false;
    DATA_BLOB input{static_cast<DWORD>(value.size()),
                    reinterpret_cast<BYTE*>(const_cast<char*>(value.data()))};
    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"Tasia login password", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output)) return false;
    encrypted.assign(output.pbData, output.pbData + output.cbData);
    LocalFree(output.pbData);
    return true;
}

bool unprotect(const void* bytes, int size, std::string& value)
{
    value.clear();
    if (size == 0) return true;
    if (!bytes || size < 0 || size > 16384) return false;
    DATA_BLOB input{static_cast<DWORD>(size), static_cast<BYTE*>(const_cast<void*>(bytes))};
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output)) return false;
    bool ok = output.cbData <= 4096;
    if (ok) value.assign(reinterpret_cast<const char*>(output.pbData), output.cbData);
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return ok;
}
}

namespace TasiaLoginStore
{
bool list(const std::string& path, const std::string& grid, std::vector<Account>& accounts)
{
    accounts.clear();
    auto db = open(path);
    if (!db) return false;
    auto stmt = prepare(db.get(), "SELECT grid,user,label,identifier FROM accounts WHERE grid=? ORDER BY label COLLATE NOCASE");
    if (!stmt || !bind(stmt.get(), 1, grid)) return false;
    int rc;
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW)
    {
        Account a{text(stmt.get(), 0), text(stmt.get(), 1), text(stmt.get(), 2), text(stmt.get(), 3)};
        if (!valid(a) || accounts.size() >= 10000) { accounts.clear(); return false; }
        accounts.push_back(std::move(a));
    }
    if (rc != SQLITE_DONE) { accounts.clear(); return false; }
    return true;
}

bool password(const std::string& path, const std::string& grid,
              const std::string& user, std::string& result)
{
    result.clear();
    auto db = open(path);
    if (!db) return false;
    auto stmt = prepare(db.get(), "SELECT password FROM accounts WHERE grid=? AND user=?");
    return stmt && bind(stmt.get(), 1, grid) && bind(stmt.get(), 2, user) &&
           sqlite3_step(stmt.get()) == SQLITE_ROW &&
           unprotect(sqlite3_column_blob(stmt.get(), 0), sqlite3_column_bytes(stmt.get(), 0), result);
}

bool save(const std::string& path, const Account& a, const std::string* password)
{
    if (!valid(a)) return false;
    std::vector<unsigned char> encrypted;
    if (password && !protect(*password, encrypted)) return false;
    auto db = open(path);
    if (!db || !execute(db.get(), "BEGIN IMMEDIATE")) return false;
    // Supported by the SQLite bundled with Windows 10, including versions predating UPSERT.
    auto stmt = prepare(db.get(), "INSERT OR IGNORE INTO accounts(grid,user,label,identifier) VALUES(?,?,?,?)");
    bool ok = stmt && bind(stmt.get(), 1, a.grid) && bind(stmt.get(), 2, a.user) &&
              bind(stmt.get(), 3, a.label) && bind(stmt.get(), 4, a.identifier) &&
              sqlite3_step(stmt.get()) == SQLITE_DONE;
    stmt.reset();
    if (ok)
    {
        stmt = prepare(db.get(), password ?
            "UPDATE accounts SET label=?,identifier=?,password=? WHERE grid=? AND user=?" :
            "UPDATE accounts SET label=?,identifier=? WHERE grid=? AND user=?");
        ok = stmt && bind(stmt.get(), 1, a.label) && bind(stmt.get(), 2, a.identifier);
        int index = 3;
        if (ok && password)
        {
            ok = (encrypted.empty() ? sqlite3_bind_null(stmt.get(), index) :
                  sqlite3_bind_blob(stmt.get(), index, encrypted.data(),
                                    static_cast<int>(encrypted.size()), SQLITE_TRANSIENT)) == SQLITE_OK;
            ++index;
        }
        ok = ok && bind(stmt.get(), index, a.grid) && bind(stmt.get(), index + 1, a.user) &&
             sqlite3_step(stmt.get()) == SQLITE_DONE;
    }
    stmt.reset();
    if (ok && execute(db.get(), "COMMIT")) return true;
    execute(db.get(), "ROLLBACK");
    return false;
}

bool erase(const std::string& path, const std::string& grid, const std::string& user)
{
    auto db = open(path);
    if (!db) return false;
    auto stmt = prepare(db.get(), "DELETE FROM accounts WHERE grid=? AND user=?");
    return stmt && bind(stmt.get(), 1, grid) && bind(stmt.get(), 2, user) &&
           sqlite3_step(stmt.get()) == SQLITE_DONE;
}

bool clearPassword(const std::string& path, const std::string& grid, const std::string& user)
{
    auto db = open(path);
    if (!db) return false;
    auto stmt = prepare(db.get(), "UPDATE accounts SET password=NULL WHERE grid=? AND user=?");
    return stmt && bind(stmt.get(), 1, grid) && bind(stmt.get(), 2, user) &&
           sqlite3_step(stmt.get()) == SQLITE_DONE;
}

bool importDatabase(const std::string& path, const std::string& source, int& imported)
{
    imported = 0;
    auto input = open(source, true);
    if (!input) return false;
    auto rows = prepare(input.get(), "SELECT grid,user,label,identifier,password FROM accounts");
    auto db = open(path);
    if (!rows || !db || !execute(db.get(), "BEGIN IMMEDIATE")) return false;
    int rc = SQLITE_ERROR, count = 0, visited = 0;
    bool ok = true;
    while ((rc = sqlite3_step(rows.get())) == SQLITE_ROW)
    {
        Account a{text(rows.get(), 0), text(rows.get(), 1), text(rows.get(), 2), text(rows.get(), 3)};
        std::string decrypted;
        if (!valid(a) || ++visited > 10000 ||
            !unprotect(sqlite3_column_blob(rows.get(), 4), sqlite3_column_bytes(rows.get(), 4), decrypted))
        { ok = false; break; }
        if (!decrypted.empty()) SecureZeroMemory(&decrypted[0], decrypted.size());
        auto stmt = prepare(db.get(), "INSERT OR IGNORE INTO accounts(grid,user,label,identifier,password) VALUES(?,?,?,?,?)");
        ok = stmt && bind(stmt.get(), 1, a.grid) && bind(stmt.get(), 2, a.user) &&
             bind(stmt.get(), 3, a.label) && bind(stmt.get(), 4, a.identifier) &&
             sqlite3_bind_blob(stmt.get(), 5, sqlite3_column_blob(rows.get(), 4),
                              sqlite3_column_bytes(rows.get(), 4), SQLITE_TRANSIENT) == SQLITE_OK &&
             sqlite3_step(stmt.get()) == SQLITE_DONE;
        if (!ok) break;
        count += sqlite3_changes(db.get());
    }
    rows.reset();
    if (ok && rc == SQLITE_DONE && execute(db.get(), "COMMIT")) { imported = count; return true; }
    execute(db.get(), "ROLLBACK");
    return false;
}
}
#else
namespace TasiaLoginStore
{
bool list(const std::string&, const std::string&, std::vector<Account>& a) { a.clear(); return false; }
bool password(const std::string&, const std::string&, const std::string&, std::string& p) { p.clear(); return false; }
bool save(const std::string&, const Account&, const std::string*) { return false; }
bool erase(const std::string&, const std::string&, const std::string&) { return false; }
bool clearPassword(const std::string&, const std::string&, const std::string&) { return false; }
bool importDatabase(const std::string&, const std::string&, int& n) { n = 0; return false; }
}
#endif

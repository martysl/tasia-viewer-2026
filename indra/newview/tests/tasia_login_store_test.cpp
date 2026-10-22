// Standalone Windows regression tests. Uses only synthetic passwords in a new test directory.
#include "../lltasia_login_store.h"
#include <winsqlite/winsqlite3.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

#define CHECK(x) do { if (!(x)) throw std::runtime_error("Failed: " #x); } while (0)

int main(int argc, char** argv)
{
    try
    {
        CHECK(argc == 2);
        std::string prefix = argv[1];
        std::string db = prefix + "accounts.sqlite3";
        std::string source = prefix + "import.sqlite3";
        std::string bad = prefix + "bad.sqlite3";
        std::vector<TasiaLoginStore::Account> rows;
        std::string value;
        const std::string secret = "SYNTHETIC-test-password-42";
        const std::string other = "SYNTHETIC-second-password";
        TasiaLoginStore::Account a{"grid-a", "first_last", "First Last", "<llsd><map /></llsd>"};
        CHECK(TasiaLoginStore::save(db, a, &secret));
        CHECK(TasiaLoginStore::password(db, "grid-a", "first_last", value) && value == secret);
        CHECK(TasiaLoginStore::save(db, a, nullptr));
        CHECK(TasiaLoginStore::password(db, "grid-a", "first_last", value) && value == secret);
        a.user = "another_user";
        CHECK(TasiaLoginStore::save(db, a, &other));
        CHECK(TasiaLoginStore::list(db, "grid-a", rows) && rows.size() == 2);
        a.grid = "grid-b";
        CHECK(TasiaLoginStore::save(db, a, &secret));
        CHECK(TasiaLoginStore::password(db, "grid-a", a.user, value) && value == other);
        CHECK(TasiaLoginStore::password(db, "grid-b", a.user, value) && value == secret);
        value.clear();
        CHECK(TasiaLoginStore::save(db, a, &value));
        CHECK(TasiaLoginStore::password(db, "grid-b", a.user, value) && value.empty());
        CHECK(!TasiaLoginStore::password(db, "grid-missing", a.user, value));
        CHECK(TasiaLoginStore::clearPassword(db, "grid-a", "another_user"));
        CHECK(TasiaLoginStore::password(db, "grid-a", "another_user", value) && value.empty());
        std::ifstream file(db, std::ios::binary);
        std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        CHECK(bytes.find(secret) == std::string::npos && bytes.find(other) == std::string::npos);
        a.grid = "grid-import";
        CHECK(TasiaLoginStore::save(source, a, &secret));
        int imported = -1;
        CHECK(TasiaLoginStore::importDatabase(db, source, imported) && imported == 1);
        CHECK(TasiaLoginStore::password(db, a.grid, a.user, value) && value == secret);
        CHECK(TasiaLoginStore::save(source, a, &other));
        CHECK(TasiaLoginStore::importDatabase(db, source, imported) && imported == 0);
        CHECK(TasiaLoginStore::password(db, a.grid, a.user, value) && value == secret);
        CHECK(TasiaLoginStore::importDatabase(db, db, imported) && imported == 0);
        a.grid = "bad-import";
        CHECK(TasiaLoginStore::save(bad, a, &secret));
        a.user = "z_corrupt";
        CHECK(TasiaLoginStore::save(bad, a, &secret));
        sqlite3* raw = nullptr;
        CHECK(sqlite3_open(bad.c_str(), &raw) == SQLITE_OK);
        CHECK(sqlite3_exec(raw, "UPDATE accounts SET password=x'010203' WHERE user='z_corrupt'", nullptr, nullptr, nullptr) == SQLITE_OK);
        sqlite3_close(raw);
        CHECK(!TasiaLoginStore::importDatabase(db, bad, imported) && imported == 0);
        CHECK(TasiaLoginStore::list(db, a.grid, rows) && rows.empty());
        CHECK(!TasiaLoginStore::importDatabase(db, prefix + "missing.sqlite3", imported));
        CHECK(TasiaLoginStore::erase(db, "grid-a", "first_last"));
        CHECK(TasiaLoginStore::list(db, "grid-a", rows) && rows.size() == 1);
        a.grid.clear();
        CHECK(!TasiaLoginStore::save(db, a, &secret));
        std::cout << "PASS: save/reopen, multiple accounts, grid isolation, preserve/clear password, "
                     "ciphertext at rest, import, duplicate preservation, self-import, invalid DPAPI "
                     "rollback, missing source, delete and invalid account\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

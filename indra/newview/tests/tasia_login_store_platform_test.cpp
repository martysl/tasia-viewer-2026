#include "lltasia_login_store.h"

#include <iostream>

int main()
{
#ifdef _WIN32
    if (!TasiaLoginStore::isAvailable()) return 1;
    std::cout << "PASS: Windows DPAPI account store reports available\n";
#else
    if (TasiaLoginStore::isAvailable()) return 1;
    std::cout << "PASS: non-Windows account extension reports unavailable\n";
#endif
    return 0;
}

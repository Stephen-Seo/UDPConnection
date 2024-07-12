#include "test_helpers.h"
#include "test_headers.h"

int checks_checked = 0;
int checks_passed = 0;

#include <iostream>

int main() {
    TEST_CXX11_shared_spin_lock();
    TEST_TSLQueue();
    TEST_UDPC();

    std::cout << "checks_checked: " << checks_checked
              << "\nchecks_passed:  " << checks_passed << std::endl;

    return checks_checked == checks_passed ? 0 : 1;
}

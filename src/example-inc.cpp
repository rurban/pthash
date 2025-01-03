#include <iostream>
#include <vector>
#include <stdint.h>

#include "pthash-static.hpp"
#include "pthash-example.hpp"  // by example-c dumped hash function

int main() {
    const uint64_t testkeys[] = {1UL, 531620205077631019UL, 15065834337849656826UL};
    for (int i = 0; i < 3; i++) {
        uint64_t k = testkeys[i];
        std::cout << "pthash_lookup(" << k << ") = " << pthash_lookup(k) << "\n";
    }
    return 0;
}

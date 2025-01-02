#include <iostream>

#include "pthash.hpp"
#include "pthash-example.hpp"  // statically dumped (not yet)

int main() {
    const std::vector<uint64_t> testkeys = {1ULL, 531620205077631019UL, 15065834337849656826UL};
    for (uint64_t k : testkeys) {
        std::cout << "pthash_lookup(" << k << ") = " << pthash_lookup(k) << "\n";
    }
    return 0;
}

#include <iostream>
#include <vector>
#include <stdint.h>

#include "pthash-static.hpp"
#include "pthash-example.hpp"  // by example-c dumped hash function

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

int main() {
    int errs = 0;
    /* generated with example-c and seed 1234567890 */
    const uint64_t num_keys = 10000;
    const struct tests {
        uint64_t key;
        pthash::bucket_id_type result;
    } testkeys[] = {{14850040288104027728UL, 1786}, // table[0]
                    {17066051041297866773UL, 3420}, // table[1]
                    {8211335932744827829UL, 2568},  // table[2]
                    {4393478487291976123UL, 2619},  // table[3]
                    {4407862767680125922UL, 4990},  // table[4]
                    {7838933779379963275UL, 8656},  // table[5]
                    {11606948423242176078UL, 5989}, // table[6]
                    {16650011335128101246UL, 7853}, // table[7]
                    {17373920153359927576UL, 3064}, // table[8]
                    {11951887939135090952UL, 5436}, // table[9]
                    /*invalid:*/
                    {3UL, 0}};
    std::ifstream sorted_keys_file("pthash-example-sorted-keys.dat");
    std::vector<uint64_t> table(num_keys);
    // std::fill(table.begin(), table.end(), 0UL);
    // this only works for minimal phf's, else fill it with a unique NOTFOUND sentinel
    for (uint64_t i = 0; i < num_keys; ++i) {
        sorted_keys_file >> table[i];
    }
    sorted_keys_file.close();

    for (size_t i = 0; i < ARRAY_SIZE(testkeys); i++) {
        uint64_t key = testkeys[i].key;
        pthash::bucket_id_type result = pthash_lookup(key);
        std::cout << "pthash_lookup(" << key << ") = " << result << "\n";
        if (result < num_keys) {
            if (table[result] != key) {
                if (i == ARRAY_SIZE(testkeys) - 1) {
                    std::cout << "table[" << result << "]: = " << table[result] << " (false-positive)\n";
                    continue; // should not be found
                }
                std::cerr << "ERROR: table[" << result << "]: " << table[result]
                          << " != " << key << "\n";
                errs++;
            } else {
                std::cout << "table[" << result << "]: " << table[result] << " = " << key << "\n";
            }
        } else {
            std::cerr << "ERROR: " << result << " > ARRAY_SIZE(testkeys)\n";
            errs++;
        }
    }
    return errs ? 1 : 0;
}

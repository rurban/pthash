#include <iostream>

#include "external/cmd_line_parser/include/parser.hpp"
#include "pthash.hpp"
#include "util.hpp"  // for functions distinct_keys and check

int main(int argc, char** argv) {
    cmd_line_parser::parser parser(argc, argv);
    using namespace pthash;

    /* Generate random 64-bit keys as input data. */
    static const uint64_t num_keys = 10000;
    static const uint64_t seed = 1234567890;
    std::cout << "generating input data..." << std::endl;
    std::vector<uint64_t> keys = distinct_keys<uint64_t>(num_keys, default_hash64(seed, seed));
    assert(keys.size() == num_keys);

    parser.add("dense_partitioning", "Activate dense partitioning.", "--dense", false, true);
    parser.add("verbose_output", "Verbose output during construction.", "--verbose", false, true);    if (!parser.parse()) return 1;

    /* Set up the build configuration. */
    build_configuration config;
    config.seed = seed;
    config.lambda = 6;
    config.alpha = 0.97;
    config.search = pthash_search_type::add_displacement;
    config.avg_partition_size = 3000;
    config.minimal_output = true;  // mphf
    config.verbose_output = parser.get<bool>("verbose_output");
    config.dense_partitioning = parser.get<bool>("dense_partitioning");

    /* Declare the PTHash function. */
    /*
        Caveat:
        when using single_phf, config.dense_partitioning must be set to false;
        when using dense_partitioned_phf, config.dense_partitioning must be set to true.
    */
    typedef single_phf<
        murmurhash2_64,                       // base hasher
        skew_bucketer,                        // bucketer type
        dictionary_dictionary,                // encoder type
        true,                                 // minimal
        pthash_search_type::add_displacement  // additive displacement
        >
        pthash_type_s;
    typedef dense_partitioned_phf<
        murmurhash2_64,                       // base hasher
        opt_bucketer,                         // bucketer type
        mono_EF,                              // encoder type
        true,                                 // minimal
        pthash_search_type::add_displacement  // additive displacement
        >
        pthash_type_d;
    // TODO switch to templated function
    if (config.dense_partitioning) {
        pthash_type_d f;
        /* Build the function in internal memory. */
        std::cout << "building the function..." << std::endl;
        auto start = clock_type::now();
        auto timings = f.build_in_internal_memory(keys.begin(), keys.size(), config);
        double total_microseconds = timings.partitioning_microseconds +
            timings.mapping_ordering_microseconds +
            timings.searching_microseconds + timings.encoding_microseconds;
        std::cout << "function built in " << to_microseconds(clock_type::now() - start) / 1000000
                  << " seconds" << std::endl;
        std::cout << "computed: " << total_microseconds / 1000000 << " seconds" << std::endl;
        /* Compute and print the number of bits spent per key. */
        double bits_per_key = static_cast<double>(f.num_bits()) / f.num_keys();
        std::cout << "function uses " << bits_per_key << " [bits/key]" << std::endl;

        /* Sanity check! */
        if (check(keys.begin(), f)) std::cout << "EVERYTHING OK!" << std::endl;

        /* Now evaluate f on some keys. */
        for (uint64_t i = 0; i != 10; ++i) {
            std::cout << "f(" << keys[i] << ") = " << f(keys[i]) << '\n';
        }

        /* Serialize the data structure to ascii and binary files */
        std::string output_filename_c("pthash-example.h");
        std::cout << "serializing the function to " << output_filename_c << std::endl;
        essentials::save("f", f, output_filename_c.c_str());

        std::string output_filename("example-c.bin");
        std::cout << "serializing the function to " << output_filename << std::endl;
        essentials::save(f, output_filename.c_str());

        {
            std::cout << "load the serialized function from " << output_filename << std::endl;
            /* Now reload from disk and query. */
            pthash_type_d loaded;
            essentials::load(loaded, output_filename.c_str());
            for (uint64_t i = 0; i != 10; ++i) {
                std::cout << "f(" << keys[i] << ") = " << loaded(keys[i]) << '\n';
                assert(f(keys[i]) == loaded(keys[i]));
            }
        }
        std::remove(output_filename.c_str());
    } else {
        pthash_type_s f;
        /* Build the function in internal memory. */
        std::cout << "building the function..." << std::endl;
        auto start = clock_type::now();
        auto timings = f.build_in_internal_memory(keys.begin(), keys.size(), config);
        double total_microseconds = timings.partitioning_microseconds +
            timings.mapping_ordering_microseconds +
            timings.searching_microseconds + timings.encoding_microseconds;
        std::cout << "function built in " << to_microseconds(clock_type::now() - start) / 1000000
                  << " seconds" << std::endl;
        std::cout << "computed: " << total_microseconds / 1000000 << " seconds" << std::endl;
        /* Compute and print the number of bits spent per key. */
        double bits_per_key = static_cast<double>(f.num_bits()) / f.num_keys();
        std::cout << "function uses " << bits_per_key << " [bits/key]" << std::endl;

        /* Sanity check! */
        if (check(keys.begin(), f)) std::cout << "EVERYTHING OK!" << std::endl;

        /* Now evaluate f on some keys. */
        for (uint64_t i = 0; i != 10; ++i) {
            std::cout << "f(" << keys[i] << ") = " << f(keys[i]) << '\n';
        }

        /* Serialize the data structure to ascii and binary files */
        std::string output_filename_c("pthash-example.h");
        std::cout << "serializing the function to " << output_filename_c << std::endl;
        essentials::save("f", f, output_filename_c.c_str());

        std::string output_filename("example-c.bin");
        std::cout << "serializing the function to " << output_filename << std::endl;
        essentials::save(f, output_filename.c_str());

        {
            std::cout << "load the serialized function from " << output_filename << std::endl;
            /* Now reload from disk and query. */
            pthash_type_s loaded;
            essentials::load(loaded, output_filename.c_str());
            for (uint64_t i = 0; i != 10; ++i) {
                std::cout << "f(" << keys[i] << ") = " << loaded(keys[i]) << '\n';
                assert(f(keys[i]) == loaded(keys[i]));
            }
        }
        std::remove(output_filename.c_str());
    }
    return 0;
}

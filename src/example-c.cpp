#include <iostream>
#include <vector>

#include "external/cmd_line_parser/include/parser.hpp"
#include "include/pthash.hpp"
#include "src/util.hpp"  // for functions distinct_keys and check

template <typename pthash_type>
void test(std::vector<uint64_t> &keys, pthash::build_configuration &config) {
    using namespace pthash;
    pthash_type f;
    const std::string key_type = "uint64_t";
    auto start = clock_type::now();
    auto timings = f.build_in_internal_memory(keys.begin(), keys.size(), config);
    double total_seconds = timings.partitioning_seconds +
        timings.mapping_ordering_seconds +
        timings.searching_seconds + timings.encoding_seconds;
    std::cout << "function built in " << seconds(clock_type::now() - start)
              << " seconds" << std::endl;
    std::cout << "computed: " << total_seconds << " seconds" << std::endl;
    /* Compute and print the number of bits spent per key. */
    double bits_per_key = static_cast<double>(f.num_bits()) / f.num_keys();
    std::cout << "function uses " << bits_per_key << " [bits/key]" << std::endl;

    /* Sanity check! */
    if (check(keys.begin(), f)) std::cout << "EVERYTHING OK!" << std::endl;

    /* Now evaluate f on some keys. */
    for (uint64_t i = 0; i < 10; ++i) {
        std::cout << i << ": f(" << keys[i] << ") = " << f(keys[i]) << '\n';
    }

    /* Serialize the data structure to ascii and binary files */
    std::string output_filename_c("pthash-example.hpp");
    std::cout << "serializing the function to " << output_filename_c << std::endl;
    essentials::save("f", f, output_filename_c.c_str());

    const std::string keys_filename("pthash-example-keys.dat");
    std::ofstream keys_file(keys_filename);
    std::cout << "writing the original random keys to " << keys_filename << std::endl;
    for (bucket_id_type i = 0; i < f.num_keys(); ++i) {
        keys_file << keys[i] << '\n';
    }
    keys_file.close();

    const std::string sorted_keys_filename("pthash-example-sorted-keys.dat");
    std::ofstream sorted_keys_file(sorted_keys_filename);
    std::cout << "writing the keys in lookup order to " << sorted_keys_filename << std::endl;
    std::vector<uint64_t> key_table(f.num_keys());
    std::vector<bucket_id_type> index_table(f.num_keys());
    std::fill(key_table.begin(), key_table.end(), 0UL);
    std::fill(index_table.begin(), index_table.end(), 0UL);
    for (bucket_id_type i = 0; i < f.num_keys(); ++i) {
        auto const &key = f(keys[i]);
        key_table[key] = keys[i];
        index_table[key] = static_cast<bucket_id_type>(i);
    }
    for (bucket_id_type i = 0; i < f.num_keys(); ++i) {
        sorted_keys_file << key_table[i] << '\n';
    }
    sorted_keys_file.close();
    // optimize space
    bits::compact_vector compact_table;
    compact_table.build(index_table.begin(), f.num_keys());

    std::ofstream out;
    out.open(output_filename_c, std::ios::app);
    out << "\n"
        "/* the order-preserving variant. */\n";
    // number of keys: bucket_id_type
#ifdef PTHASH_ENABLE_LARGE_BUCKET_ID_TYPE
    out << "uint64_t";
#else
    out << "uint32_t";
#endif
    out << " pthash_lookup(const " << key_type.c_str() << " key) {\n"
        "  using namespace pthash;\n"
        "  /* sorted table key indices, needed to lookup the keys. */\n"
        "  static const bits::compact_vector index_table = ";
    out.close();
    essentials::save("compact_table", compact_table, output_filename_c.c_str(), std::ios::app);
    out.open(output_filename_c, std::ios::app);
    out << ";\n  return index_table[pthash_unordered_lookup(key)];\n}\n";
    out.close();

    std::string output_filename("pthash-example.bin");
    std::cout << "serializing the function to " << output_filename << std::endl;
    essentials::save(f, output_filename.c_str());

    {
        std::cout << "load the serialized function from " << output_filename << std::endl;
        /* Now reload from disk and query. */
        pthash_type loaded;
        essentials::load(loaded, output_filename.c_str());
        for (uint64_t i = 0; i != 10; ++i) {
            auto const &p = loaded(keys[i]);
            std::cout << i << ": index_table[f(" << keys[i] << ")] = " << index_table[p] << '\n';
            assert(f(keys[i]) == p);
            assert(index_table[p] == i);
        }
    }
    std::remove(output_filename.c_str());
}

int main(int argc, char** argv) {
    cmd_line_parser::parser parser(argc, argv);
    using namespace pthash;

    /* Generate random 64-bit keys as input data. */
    static const uint64_t num_keys = 10000;
    static const uint64_t seed = 1234567890;
    std::cout << "generating input data..." << std::endl;
    std::vector<uint64_t> keys = distinct_keys<uint64_t>(num_keys, default_hash64(seed, seed));
    assert(keys.size() == num_keys);

    parser.add("num_partitions", "Number of partitions (0: single, default: 50)", "-p", false);
    parser.add("num_threads", "Number of threads to use for construction.", "-t", false);
    parser.add("verbose_output", "Verbose output during construction.", "--verbose", false, true);
    if (!parser.parse()) return 1;

    /* Set up the build configuration. */
    build_configuration config;
    config.seed = seed;
    config.c = 6.0;
    config.alpha = 0.97;
    config.num_threads = 4;
    config.num_partitions = 50;
    config.minimal_output = true;
    config.verbose_output = parser.get<bool>("verbose_output");
    config.num_partitions = parser.get<uint64_t>("num_partitions");
    config.num_threads = parser.get<uint64_t>("num_threads");

    typedef single_phf<
        murmurhash2_64,                       // base hasher
        dictionary_dictionary,                // encoder type
        true                                  // minimal
        >
        pthash_type_s;
    typedef partitioned_phf<
        murmurhash2_64,                       // base hasher
        compact_compact,                      // encoder type
        true                                  // minimal
        >
        pthash_type_d;
    if (config.num_partitions > 0) {
        /* Build and test the partitioned_phf function in internal memory. */
        std::cout << "building the partitioned function..." << std::endl;
        test<pthash_type_d>(keys, config);
    } else {
        /* Build and test the single_phf function in internal memory. */
        std::cout << "building the single function..." << std::endl;
        test<pthash_type_s>(keys, config);
    }
    return 0;
}

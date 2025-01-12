#include <iostream>
#include <thread>
#include <unordered_set>

#include "../external/cmd_line_parser/include/parser.hpp"
#include "pthash.hpp"
#include "util.hpp"

using namespace pthash;

template <typename Iterator>
struct build_parameters {
    build_parameters(Iterator keys, uint64_t num_keys) : keys(keys), num_keys(num_keys) {}

    Iterator keys;
    uint64_t num_keys;
    bool external_memory, check, compile, lookup, ordered;
    std::string encoder_type;
    std::string output_filename;
    std::string keys_index_filename;
    std::string keys_filename;
};

/* variants:
 * - emit the order-preserving lookup function with the index_table not exported: --ordered
 * - or emit the index_table (appended or extra): --keys-index-file
 * and seperately:
 * - emit the sorted keys table (appended or extra): --keys
 */
void save_key(std::ostream &out, const bool binary_out, const std::string &key)
{
    if (binary_out) {
        essentials::save_pod(out, key.size());
        essentials::save_pod(out, key.c_str());
    }
    else
        essentials::save_pod(out, "", key.c_str());
}
void save_key(std::ostream &out, const bool binary_out, const uint64_t key)
{
    if (binary_out)
        essentials::save_pod(out, key);
    else
        essentials::save_pod(out, "", key);
}

// FIXME binary loader.visit if same file
template <typename Function, typename Iterator>
void save_keys(Function f, build_parameters<Iterator> const& params,
               const std::string &key_type)
{
    std::ofstream out;
    std::vector<bucket_id_type> index_table(f.num_keys());
    Iterator keys = params.keys;
    const bool binary_out =
        !(params.compile || (params.keys_filename.find(".hpp") != std::string::npos));
    if (binary_out) {
        out.open(params.keys_filename, std::ios::app | std::ios::binary);
    } else {
        out.open(params.keys_filename, std::ios::app);
        out << "\nconst " << key_type.c_str() << " pthash_keys = {\n";
    }
    for (bucket_id_type i = 0; i < f.num_keys(); i++) {
        auto const &key = *keys;
        bucket_id_type p = f(key);
        index_table[p] = i;
        keys++;
    }
    keys = params.keys; // reset
    // TODO very slow lines_iterator, linear search forward only.
    // but number of keys can be too big for a RandomAcessIterator
    for (bucket_id_type i = 0; i < f.num_keys(); i++) {
        auto const &key = *keys;
        bucket_id_type p = f(key);
        if (p == index_table[i]) {
            save_key(out, binary_out, key);
            if (i + 1 < f.num_keys() && index_table[i + 1] < p) {
                keys = params.keys; // reset, else we can forward
                break;
            }
        }
        keys++;
    }
    if (!binary_out)
        out << "};\n\n";
    out.close();
}

template <typename Function, typename Iterator, typename Keytype>
void save_keys_index(Function f, build_parameters<Iterator> const& params,
                     const std::string &key_type) {
    Iterator keys = params.keys;
    std::ofstream out;
    std::vector<bucket_id_type> index_table(f.num_keys());
    //bool is_string = key_type.find("string") != std::string::npos;
    for (bucket_id_type i = 0; i < f.num_keys(); i++) {
        auto const &key = *keys;
        bucket_id_type p = f(key);
        index_table[p] = p;
        keys++;
    }
    
    // optimize space
    compact compact_table;
    compact_table.encode(index_table.begin(), f.num_keys());

    if (params.compile || (params.keys_index_filename.find(".hpp") != std::string::npos)) {
        if (params.keys_index_filename == params.output_filename) { // append keys
            out.open(params.keys_index_filename, std::ios::app);
        } else {
            out.open(params.keys_index_filename);
            out << pthash_static_header_coda();
            out << ";\n";
        }
        out << "\n/* the order-preserving variant. */\n";
        // number of keys: bucket_id_type
#ifdef PTHASH_ENABLE_LARGE_BUCKET_ID_TYPE
        out << "uint64_t";
#else
        out << "uint32_t";
#endif
        out << " pthash_lookup(const " << key_type.c_str() << " key) {\n"
            "  using namespace pthash;\n"
            "  /* sorted table key indices, needed to lookup the keys.\n"
            "  static const struct compact index_table = ";
        out.close();
        essentials::save("compact_table", compact_table, params.keys_index_filename.c_str(),
                         std::ios::app);
        out.open(params.keys_index_filename, std::ios::app);
        out << "  return index_table[pthash_unordered_lookup(key)];\n";
        out << "}\n";
        out.close();
    } else {
        essentials::save(compact_table, params.keys_index_filename.c_str());
    }
}

template <typename Function, typename Builder, typename Iterator>
void build_benchmark(Builder& builder, build_timings const& timings,
                     build_parameters<Iterator> const& params, build_configuration const& config) {
    Function f;
    if (config.verbose_output) {
        essentials::logger("building pthash...");
    }
    double encoding_seconds = f.build(builder, config);

    // timings breakdown
    double total_seconds = timings.partitioning_seconds + timings.mapping_ordering_seconds +
                           timings.searching_seconds + encoding_seconds;
    if (config.verbose_output) {
        std::cout << "partitioning: " << timings.partitioning_seconds << " [sec]" << std::endl;
        std::cout << "mapping+ordering: " << timings.mapping_ordering_seconds << " [sec]"
                  << std::endl;
        std::cout << "searching: " << timings.searching_seconds << " [sec]" << std::endl;
        std::cout << "encoding: " << encoding_seconds << " [sec]" << std::endl;
        std::cout << "total: " << total_seconds << " [sec]" << std::endl;
    }

    // space breakdown
    uint64_t table_size = f.table_size();
    double pt_bits_per_key = static_cast<double>(f.num_bits_for_pilots()) / table_size;
    double mapper_bits_per_key = static_cast<double>(f.num_bits_for_mapper()) / table_size;
    double bits_per_key = static_cast<double>(f.num_bits()) / table_size;
    if (config.verbose_output) {
        std::cout << "pilots: " << pt_bits_per_key << " [bits/key]" << std::endl;
        std::cout << "mapper: " << mapper_bits_per_key << " [bits/key]" << std::endl;
        std::cout << "total: " << bits_per_key << " [bits/key]" << std::endl;
    }

    // correctness check
    if (params.check) {
        if (config.verbose_output) {
            essentials::logger("checking data structure for correctness...");
        }
        if (check(params.keys, f) and config.verbose_output) {
            std::cout << "EVERYTHING OK!" << std::endl;
        }
    }

    double nanosec_per_key = 0;
    if (params.lookup) {
        if (config.verbose_output) essentials::logger("measuring lookup time...");
        if (params.external_memory) {
            std::vector<typename Iterator::value_type> queries;
            uint64_t remaining = params.num_keys, batch_size = 100 * 1000000;
            Iterator query = params.keys;
            while (remaining > 0) {
                auto cur_batch_size = std::min(remaining, batch_size);
                queries.reserve(cur_batch_size);
                for (uint64_t i = 0; i != cur_batch_size; ++i, ++query) queries.push_back(*query);
                nanosec_per_key += perf(queries.begin(), cur_batch_size, f) * cur_batch_size;
                remaining -= cur_batch_size;
                queries.clear();
            }
            nanosec_per_key /= params.num_keys;
        } else {
            nanosec_per_key = perf(params.keys, params.num_keys, f);
        }
        if (config.verbose_output) std::cout << nanosec_per_key << " [nanosec/key]" << std::endl;
    }

    essentials::json_lines result;

    result.add("n", params.num_keys);
    if (params.num_keys != table_size)
        result.add("table_size", table_size);
    result.add("c", config.c);
    result.add("alpha", config.alpha);
    result.add("minimal", params.num_keys == table_size ? "true" : "false");
    result.add("encoder_type", Function::encoder_type::name().c_str());
    result.add("num_partitions", config.num_partitions);
    if (config.seed != constants::invalid_seed) result.add("seed", config.seed);
    result.add("num_threads", config.num_threads);
    result.add("external_memory", params.external_memory ? "true" : "false");

    result.add("partitioning_seconds", timings.partitioning_seconds);
    result.add("mapping_ordering_seconds", timings.mapping_ordering_seconds);
    result.add("searching_seconds", timings.searching_seconds);
    result.add("encoding_seconds", encoding_seconds);
    result.add("total_seconds", total_seconds);
    result.add("pt_bits_per_key", pt_bits_per_key);
    result.add("mapper_bits_per_key", mapper_bits_per_key);
    result.add("bits_per_key", bits_per_key);
    result.add("nanosec_per_key", nanosec_per_key);
    result.add("ordered", params.ordered ? "true" : "false");
    result.add("compile", params.compile ? "true" : "false");
    if (params.output_filename.size())
        result.add("output_filename", params.output_filename.c_str());
    if (params.keys_index_filename.size())
        result.add("keys_index_filename", params.keys_index_filename.c_str());
    if (params.keys_filename.size())
        result.add("keys_filename", params.keys_filename.c_str());
    result.print_line();

    if (params.output_filename != "") {
        if (params.compile) {
            if (config.verbose_output)
                essentials::logger("compile to " + params.output_filename);
            essentials::save("f", f, params.output_filename.c_str());
        } else {
            if (config.verbose_output)
                essentials::logger("saving data structure to disk...");
            essentials::save(f, params.output_filename.c_str());
        }
    }
    if (params.keys_filename.size()) {
        if (config.verbose_output)
            essentials::logger("save sorted keys to " + params.keys_filename);
        Iterator keys = params.keys;
        std::string key_type = essentials::demangle(typeid(*keys).name());
        if (key_type.find("string") != std::string::npos)
            save_keys<Function, Iterator>(f, params, "std::string");
        else
            save_keys<Function, Iterator>(f, params, "uint64_t");
    }
    // when sorted keys are stored, we don't need the key_index anymore
    else if (params.ordered || params.keys_index_filename.size()) {
        if (config.verbose_output)
            essentials::logger("compile keys indices to " + params.keys_index_filename);
        Iterator keys = params.keys;
        // for now only string or u64 key types
        std::string key_type = essentials::demangle(typeid(*keys).name());
        if (key_type.find("string") != std::string::npos)
            save_keys_index<Function, Iterator, std::string>(f, params, "std::string");
        else
            save_keys_index<Function, Iterator, uint64_t>(f, params, "uint64_t");
    }
    if (config.verbose_output)
        essentials::logger("DONE");
}

template <bool partitioned, typename Encoder, typename Builder, typename Iterator>
void choose_phf(Builder& builder, build_timings const& timings,
                build_parameters<Iterator> const& params, build_configuration const& config) {
    if constexpr (partitioned) {
        if (config.minimal_output) {
            build_benchmark<partitioned_phf<typename Builder::hasher_type, Encoder, true>>(
                builder, timings, params, config);
        } else {
            build_benchmark<partitioned_phf<typename Builder::hasher_type, Encoder, false>>(
                builder, timings, params, config);
        }
    } else {
        if (config.minimal_output) {
            build_benchmark<single_phf<typename Builder::hasher_type, Encoder, true>>(
                builder, timings, params, config);
        } else {
            build_benchmark<single_phf<typename Builder::hasher_type, Encoder, false>>(
                builder, timings, params, config);
        }
    }
}

template <bool partitioned, typename Builder, typename Iterator>
void choose_encoder(build_parameters<Iterator> const& params, build_configuration const& config) {
    if (config.verbose_output) essentials::logger("construction starts");

    Builder builder;
    build_timings timings = builder.build_from_keys(params.keys, params.num_keys, config);

    bool encode_all = (params.encoder_type == "all");

#ifdef PTHASH_ENABLE_ALL_ENCODERS
    if (encode_all or params.encoder_type == "compact") {
        choose_phf<partitioned, compact>(builder, timings, params, config);
    }
    if (encode_all or params.encoder_type == "partitioned_compact") {
        choose_phf<partitioned, partitioned_compact>(builder, timings, params, config);
    }
    if (encode_all or params.encoder_type == "compact_compact") {
        choose_phf<partitioned, compact_compact>(builder, timings, params, config);
    }
    if (encode_all or params.encoder_type == "dictionary") {
        choose_phf<partitioned, dictionary>(builder, timings, params, config);
    }
    if (encode_all or params.encoder_type == "dictionary_dictionary") {
        choose_phf<partitioned, dictionary_dictionary>(builder, timings, params, config);
    }
    if (encode_all or params.encoder_type == "elias_fano") {
        choose_phf<partitioned, elias_fano>(builder, timings, params, config);
    }
    if (encode_all or params.encoder_type == "dictionary_elias_fano") {
        choose_phf<partitioned, dictionary_elias_fano>(builder, timings, params, config);
    }
    if (encode_all or params.encoder_type == "sdc") {
        choose_phf<partitioned, sdc>(builder, timings, params, config);
    }
#else
    if (encode_all or params.encoder_type == "partitioned_compact") {
        choose_phf<partitioned, partitioned_compact>(builder, timings, params, config);
    }
    if (encode_all or params.encoder_type == "dictionary_dictionary") {
        choose_phf<partitioned, dictionary_dictionary>(builder, timings, params, config);
    }
    if (encode_all or params.encoder_type == "elias_fano") {
        choose_phf<partitioned, elias_fano>(builder, timings, params, config);
    }
#endif
}

template <typename Hasher, typename Iterator>
void choose_builder(build_parameters<Iterator> const& params, build_configuration const& config) {
    if (config.num_partitions > 1) {
        if (params.external_memory) {
            choose_encoder<true, external_memory_builder_partitioned_phf<Hasher>>(params, config);
        } else {
            choose_encoder<true, internal_memory_builder_partitioned_phf<Hasher>>(params, config);
        }
    } else {
        if (params.external_memory) {
            choose_encoder<false, external_memory_builder_single_phf<Hasher>>(params, config);
        } else {
            choose_encoder<false, internal_memory_builder_single_phf<Hasher>>(params, config);
        }
    }
}

template <typename Iterator>
void choose_hasher(build_parameters<Iterator> const& params, build_configuration const& config) {
    if (params.num_keys <= (uint64_t(1) << 30)) {
        choose_builder<murmurhash2_64>(params, config);
    } else {
        choose_builder<murmurhash2_128>(params, config);
    }
}

template <typename Iterator>
void build(cmd_line_parser::parser const& parser, Iterator keys, uint64_t num_keys) {
    build_parameters<Iterator> params(keys, num_keys);
    params.external_memory = parser.get<bool>("external_memory");
    params.check = parser.get<bool>("check");
    params.compile = parser.get<bool>("compile"); // to header
    params.ordered = parser.get<bool>("ordered"); // order-preserving
    params.lookup = parser.get<bool>("lookup");

    params.encoder_type = parser.get<std::string>("encoder_type");
    {
        std::unordered_set<std::string> encoders({
#ifdef PTHASH_ENABLE_ALL_ENCODERS
            "compact", "partitioned_compact", "compact_compact", "dictionary",
            "dictionary_dictionary", "elias_fano", "dictionary_elias_fano", "sdc", "all"
#else
            "partitioned_compact", "dictionary_dictionary", "elias_fano", "all"
#endif
        });
        if (encoders.find(params.encoder_type) == encoders.end()) {
            std::cerr << "unknown encoder type" << std::endl;
            return;
        }
    }

    params.output_filename =
        (!parser.parsed("output_filename")) ? "" : parser.get<std::string>("output_filename");
    params.keys_filename =
        (!parser.parsed("keys_filename")) ? "" : parser.get<std::string>("keys_filename");
    if (params.compile && params.output_filename == "") {
        params.output_filename = "pthash.hpp";
    }
    else if (params.output_filename.find(".hpp") != std::string::npos)
        params.compile = true;
    if (params.ordered && params.keys_filename == "") {
        params.keys_filename = params.output_filename;
    }

    build_configuration config;
    config.c = parser.get<double>("c");
    config.alpha = parser.get<double>("alpha");
    config.minimal_output = parser.get<bool>("minimal_output");
    config.verbose_output = parser.get<bool>("verbose_output");

    config.num_partitions = 1;
    if (parser.parsed("num_partitions")) {
        config.num_partitions = parser.get<uint64_t>("num_partitions");
    }

    if (parser.parsed("num_threads")) {
        config.num_threads = parser.get<uint64_t>("num_threads");
        if (config.num_threads == 0) {
            std::cout << "Warning: specified 0 threads, defaulting to 1" << std::endl;
            config.num_threads = 1;
        }
        uint64_t num_threads = std::thread::hardware_concurrency();
        if (config.num_threads > num_threads) {
            config.num_threads = num_threads;
            std::cout << "Warning: too many threads specified, defaulting to " << config.num_threads
                      << std::endl;
        }
    }

    if (parser.parsed("seed")) config.seed = parser.get<uint64_t>("seed");
    if (parser.parsed("tmp_dir")) config.tmp_dir = parser.get<std::string>("tmp_dir");

    if (parser.parsed("ram")) {
        constexpr uint64_t GB = 1000000000;
        uint64_t ram = parser.get<double>("ram") * GB;
        if (ram > constants::available_ram) {
            double available_ram_in_GB = static_cast<double>(constants::available_ram) / GB;
            std::cout << "Warning: too much RAM specified, this machine has " << available_ram_in_GB
                      << " GB of RAM; defaulting to " << available_ram_in_GB * 0.75 << " GB"
                      << std::endl;
            ram = static_cast<double>(constants::available_ram) * 0.75;
        }
        config.ram = ram;
    }

    choose_hasher(params, config);
}

int version(void) {
    std::cout << "pthash 0.2.1 static" << std::endl;
    return 0;
}

int main(int argc, char** argv) {
    cmd_line_parser::parser parser(argc, argv);

    /* Required arguments. */
    parser.add("num_keys", "The size of the input.", "-n", true);
    parser.add("c",
               "A constant that trades construction speed for space effectiveness. "
               "A reasonable value lies between 3.0 and 10.0.",
               "-c", true);
    parser.add("alpha", "The table load factor. It must be a quantity > 0 and <= 1.", "-a", true);

    parser.add("encoder_type",
               "The encoder type. Possibile values are: "
#ifdef PTHASH_ENABLE_ALL_ENCODERS
               "'compact', 'partitioned_compact', 'compact_compact', 'dictionary', "
               "'dictionary_dictionary', 'elias_fano', 'dictionary_elias_fano', 'sdc', "
               "'all'.\n\t"
#else
               "'partitioned_compact', 'dictionary_dictionary', 'elias_fano', "
               "'all'.\n\t"
               "(For more encoders, compile again with 'cmake .. -D "
               "PTHASH_ENABLE_ALL_ENCODERS=On').\n\t"
#endif
               "The 'all' type will just benchmark all encoders. (Useful for benchmarking "
               "purposes.)",
               "-e", true);

    /* Optional arguments. */
    parser.add("num_partitions", "Number of partitions.", "-p", false);
    parser.add("seed", "Seed to use for construction.", "-s", false);
    parser.add("num_threads", "Number of threads to use for construction.", "-t", false);
    parser.add("input_filename",
               "A string input file name. If this is not provided, then num_keys 64-bit random "
               "keys will be used as input instead."
               "If, instead, the filename is '-', then input is read from standard input.",
               "-i", false);
    parser.add("output_filename", "Output file name where the function will be serialized. (may be a .hpp)", "-o",
               false);
    parser.add("keys_filename", "Output file name to write the sorted keys to. (may be a .hpp)", "--keys",
               false);
    parser.add("keys_index_filename", "Output file name to write the sorted key indices to. (may be a .hpp)", "--keys-index",
               false);
    parser.add("ordered", "Order-preserving lookup, store index mapping for ordered keys also.", "--ordered", false,
               true);
    parser.add("tmp_dir",
               "Temporary directory used for building in external memory. Default is directory '" +
                   constants::default_tmp_dirname + "'.",
               "-d", false);
    parser.add("ram", "Number of Giga bytes of RAM to use for construction in external memory.",
               "-m", false);
    parser.add("minimal_output", "Build a minimal PHF.", "--minimal", false, true);
    parser.add("external_memory", "Build the function in external memory.", "--external", false,
               true);
    parser.add("compile", "Compile to .hpp source file(s).", "--compile", false, true);
    parser.add("verbose_output", "Verbose output during construction.", "--verbose", false, true);
    parser.add("check", "Check correctness after construction.", "--check", false, true);
    parser.add("lookup", "Measure average lookup time after construction.", "--lookup", false,
               true);
    parser.add("version", "Prints the program version and exits", "--version", false, true);

    //if (argc > 1 && strcmp(argv[1], "--version") == 0) // without rurban's help2man cmd_parser
    //    return version();
    if (!parser.parse()) return 1;
    if (parser.parsed("version"))
        return version();
    if (parser.parsed("input_filename") && parser.get<std::string>("input_filename") == "-" &&
        parser.get<bool>("external_memory")) {
        if (parser.get<bool>("check") || parser.get<bool>("lookup")) {
            std::cerr << "--input_filename - (stdin input) in combination with --external can be "
                         "used only without --check and --lookup"
                      << std::endl;
            return 1;
        }
    }

    auto num_keys = parser.get<uint64_t>("num_keys");
    auto seed = (parser.parsed("seed")) ? parser.get<uint64_t>("seed") : constants::invalid_seed;
    bool external_memory = parser.get<bool>("external_memory");

    if (parser.parsed("input_filename")) {
        auto input_filename = parser.get<std::string>("input_filename");
        if (external_memory) {
            if (input_filename == "-") {
                sequential_lines_iterator keys(std::cin);
                build(parser, keys, num_keys);
            } else {
                mm::file_source<uint8_t> input(input_filename, mm::advice::sequential);
                lines_iterator keys(input.data(), input.data() + input.size());
                build(parser, keys, num_keys);
                input.close();
            }
        } else {
            std::vector<std::string> keys;
            if (input_filename == "-") {
                keys =
                    read_string_collection(num_keys, std::cin, parser.get<bool>("verbose_output"));
            } else {
                std::ifstream input(input_filename.c_str());
                if (!input.good()) throw std::runtime_error("error in opening file.");
                keys = read_string_collection(num_keys, input, parser.get<bool>("verbose_output"));
                input.close();
            }
            build(parser, keys.begin(), keys.size());
        }
    } else {  // use num_keys random 64-bit keys
        if (external_memory) {
            std::cout << "Warning: external memory construction with in-memory input" << std::endl;
        }
        VECTOR(uint64_t) keys = distinct_keys<uint64_t>(num_keys, seed);
        build(parser, keys.begin(), keys.size());
    }

    return 0;
}

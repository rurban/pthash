#pragma once

#include "utils/bucketers.hpp"
#include "builders/util.hpp"
#include "builders/internal_memory_builder_single_phf.hpp"

namespace pthash {

template <typename Hasher, typename Bucketer, typename Encoder, bool Minimal,
          pthash_search_type Search>
struct single_phf {
    static_assert(!std::is_base_of<dense_encoder, Encoder>::value,
                  "Dense encoders are only for dense PTHash. Select another encoder.");
    typedef Encoder encoder_type;
    static constexpr bool minimal = Minimal;

    template <typename Iterator>
    build_timings build_in_internal_memory(Iterator keys, const uint64_t num_keys,
                                           build_configuration const& config) {
        assert(Minimal == config.minimal_output);
        assert(Search == config.search);
        internal_memory_builder_single_phf<Hasher, Bucketer> builder;
        auto timings = builder.build_from_keys(keys, num_keys, config);
        timings.encoding_microseconds = build(builder, config);
        return timings;
    }

    template <typename Builder>
    double build(Builder const& builder, build_configuration const&) {
        auto start = clock_type::now();
        m_seed = builder.seed();
        m_num_keys = builder.num_keys();
        m_table_size = builder.table_size();
        m_M_128 = fastmod::computeM_u64(m_table_size);
        m_M_64 = fastmod::computeM_u32(m_table_size);
        m_bucketer = builder.bucketer();
        m_pilots.encode(builder.pilots().data(), m_bucketer.num_buckets());
        if (Minimal and m_num_keys < m_table_size) {
            assert(builder.free_slots().size() == m_table_size - m_num_keys);
            m_free_slots.encode(builder.free_slots().data(), m_table_size - m_num_keys);
        }
        auto stop = clock_type::now();
        return to_microseconds(stop - start);
    }

#ifdef PTHASH_STATIC
    // static init's
    single_phf();

    single_phf(uint64_t seed, uint64_t num_keys, uint64_t table_size,
               __uint128_t M_128, uint64_t M_64,
               Bucketer bucketer, Encoder pilots, ef_sequence<false> free_slots)
        : m_seed(seed)
        , m_num_keys(num_keys)
        , m_table_size(table_size)
        , m_M_128(M_128)
        , m_M_64(M_64)
        , m_bucketer(bucketer)
        , m_pilots(pilots)
        , m_free_slots(free_slots) {}

    single_phf(uint64_t seed, uint64_t num_keys, uint64_t table_size,
               Bucketer bucketer, Encoder pilots, ef_sequence<false> free_slots)
        : m_seed(seed)
        , m_num_keys(num_keys)
        , m_table_size(table_size)
        , m_M_128(fastmod::computeM_u64(table_size))
        , m_M_64(fastmod::computeM_u32(table_size))
        , m_bucketer(bucketer)
        , m_pilots(pilots)
        , m_free_slots(free_slots) {}
#endif

    template <typename T>
    uint64_t operator()(T const& key) const {
        auto hash = Hasher::hash(key, m_seed);
        return position(hash);
    }

    uint64_t position(typename Hasher::hash_type hash) const {
        const uint64_t bucket = m_bucketer.bucket(hash.first());
        const uint64_t pilot = m_pilots.access(bucket);

        uint64_t p = 0;
        if constexpr (Search == pthash_search_type::xor_displacement) {
            const uint64_t hashed_pilot = default_hash64(pilot, m_seed);
            p = fastmod::fastmod_u64(hash.second() ^ hashed_pilot, m_M_128, m_table_size);
        } else if constexpr (Search == pthash_search_type::add_displacement) {
            const uint64_t s = fastmod::fastdiv_u32(pilot, m_M_64);
            p = fastmod::fastmod_u32(((hash64(hash.second() + s).mix()) >> 33) + pilot, m_M_64,
                                     m_table_size);
        } else {
            assert(false);
        }

        if constexpr (Minimal) {
            if (PTHASH_LIKELY(p < num_keys())) return p;
            return m_free_slots.access(p - num_keys());
        }

        return p;
    }

    size_t num_bits_for_pilots() const {
        return 8 * (sizeof(m_seed) + sizeof(m_num_keys) + sizeof(m_table_size) + sizeof(m_M_64) +
                    sizeof(m_M_128)) +
               m_pilots.num_bits();
    }

    size_t num_bits_for_mapper() const {
        return m_bucketer.num_bits() + m_free_slots.num_bits();
    }

    size_t num_bits() const {
        return num_bits_for_pilots() + num_bits_for_mapper();
    }

    inline uint64_t num_keys() const {
        return m_num_keys;
    }

    inline uint64_t table_size() const {
        return m_table_size;
    }

    template <typename Visitor>
    void visit(Visitor& visitor) {
        visitor.visit(m_seed);
        visitor.visit(m_num_keys);
        visitor.visit(m_table_size);
        visitor.visit(m_M_128);
        visitor.visit(m_M_64);
        visitor.visit(m_bucketer);
        visitor.visit(m_pilots);
        visitor.visit(m_free_slots);
    }

    template <typename Visitor>
    void visit(const std::string name, Visitor& visitor) {
        (void)name;
        visitor.dump(R"(#pragma once
#include <stdlib.h>
#include <stdint.h>
#include "pthash-static.hpp"

uint64_t pthash_lookup(uint64_t key) {
  using namespace pthash;
  )");
        std::string type = essentials::demangle(typeid(*this).name());
        visitor.dump(type);
        visitor.dump(" f(\n    ");
        visitor.visit("m_seed", m_seed);
        visitor.dump(",\n    ");
        visitor.visit("m_num_keys", m_num_keys);
        visitor.dump(",\n    ");
        visitor.visit("m_table_size", m_table_size);
        visitor.dump(",\n    ");
        visitor.visit("m_M_128", m_M_128);
        visitor.dump(",\n    ");
        visitor.visit("m_M_64", m_M_64);
        visitor.dump(",\n    ");
        visitor.visit("m_bucketer", m_bucketer);
        visitor.dump(",\n    ");
        visitor.visit("m_pilots", m_pilots);
        visitor.dump(",\n    ");
        visitor.visit("m_free_slots", m_free_slots);
        visitor.dump(");\n  return f(key);\n}");
    }

private:
    uint64_t m_seed;
    uint64_t m_num_keys;
    uint64_t m_table_size;
    __uint128_t m_M_128;
    uint64_t m_M_64;
    Bucketer m_bucketer;
    Encoder m_pilots;
    ef_sequence<false> m_free_slots;
};

}  // namespace pthash

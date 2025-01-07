#pragma once

#include "util.hpp"

namespace pthash {

struct skew_bucketer {
    skew_bucketer()
        : m_num_dense_buckets(0)
        , m_num_sparse_buckets(0)
        , m_M_num_dense_buckets(0)
        , m_M_num_sparse_buckets(0) {}

    skew_bucketer(uint64_t num_dense_buckets, uint64_t num_sparse_buckets,
                  __uint128_t M_num_dense_buckets, __uint128_t M_num_sparse_buckets)
        : m_num_dense_buckets(num_dense_buckets)
        , m_num_sparse_buckets(num_sparse_buckets)
        , m_M_num_dense_buckets(M_num_dense_buckets)
        , m_M_num_sparse_buckets(M_num_sparse_buckets) {}

    skew_bucketer(uint64_t num_dense_buckets, uint64_t num_sparse_buckets)
        : m_num_dense_buckets(num_dense_buckets)
        , m_num_sparse_buckets(num_sparse_buckets)
        , m_M_num_dense_buckets(fastmod::computeM_u64(num_dense_buckets))
        , m_M_num_sparse_buckets(fastmod::computeM_u64(num_sparse_buckets)) {}

    void init(uint64_t num_buckets) {
        m_num_dense_buckets = constants::b * num_buckets;
        m_num_sparse_buckets = num_buckets - m_num_dense_buckets;
        m_M_num_dense_buckets = fastmod::computeM_u64(m_num_dense_buckets);
        m_M_num_sparse_buckets = fastmod::computeM_u64(m_num_sparse_buckets);
    }

    inline uint64_t bucket(uint64_t hash) const {
        static const uint64_t T = constants::a * static_cast<float>(UINT64_MAX);
        return (hash < T) ? fastmod::fastmod_u64(hash, m_M_num_dense_buckets, m_num_dense_buckets)
                          : m_num_dense_buckets + fastmod::fastmod_u64(hash, m_M_num_sparse_buckets,
                                                                       m_num_sparse_buckets);
    }

    uint64_t num_buckets() const {
        return m_num_dense_buckets + m_num_sparse_buckets;
    }

    size_t num_bits() const {
        return 8 * (sizeof(m_num_dense_buckets) + sizeof(m_num_sparse_buckets) +
                    sizeof(m_M_num_dense_buckets) + sizeof(m_M_num_sparse_buckets));
    }

    void swap(skew_bucketer& other) {
        std::swap(m_num_dense_buckets, other.m_num_dense_buckets);
        std::swap(m_num_sparse_buckets, other.m_num_sparse_buckets);
        std::swap(m_M_num_dense_buckets, other.m_M_num_dense_buckets);
        std::swap(m_M_num_sparse_buckets, other.m_M_num_sparse_buckets);
    }

    template <typename Visitor>
    void visit(Visitor& visitor) const {
        visit_impl(visitor, *this);
    }

    template <typename Visitor>
    void visit(Visitor& visitor) {
        visit_impl(visitor, *this);
    }

    template <typename Visitor>
    void visit(const std::string name, Visitor& visitor) {
        visit_impl(name, visitor, *this);
    }

private:
    template <typename Visitor, typename T>
    static void visit_impl(Visitor& visitor, T&& t) {
        visitor.visit(t.m_num_dense_buckets);
        visitor.visit(t.m_num_sparse_buckets);
        visitor.visit(t.m_M_num_dense_buckets);
        visitor.visit(t.m_M_num_sparse_buckets);
    }
    template <typename Visitor, typename T>
    static void visit_impl(const std::string, Visitor& visitor, T&& t) {
        visitor.dump("skew_bucketer(");
        visitor.visit("m_num_dense_buckets", t.m_num_dense_buckets);
        visitor.dump(", ");
        visitor.visit("m_num_sparse_buckets", t.m_num_sparse_buckets);
        visitor.dump(", ");
        visitor.visit("m_M_num_dense_buckets", t.m_M_num_dense_buckets);
        visitor.dump(", ");
        visitor.visit("m_M_num_sparse_buckets", t.m_M_num_sparse_buckets);
        visitor.dump(")");
    }

    uint64_t m_num_dense_buckets, m_num_sparse_buckets;
    __uint128_t m_M_num_dense_buckets, m_M_num_sparse_buckets;
};

struct uniform_bucketer {
    uniform_bucketer() : m_num_buckets(0), m_M_num_buckets(0) {}
    uniform_bucketer(uint64_t num_buckets, __uint128_t M_num_buckets)
        : m_num_buckets(num_buckets), m_M_num_buckets(M_num_buckets) {}

    void init(uint64_t num_buckets) {
        m_num_buckets = num_buckets;
        m_M_num_buckets = fastmod::computeM_u64(m_num_buckets);
    }

    inline uint64_t bucket(uint64_t hash) const {
        return fastmod::fastmod_u64(hash, m_M_num_buckets, m_num_buckets);
    }

    uint64_t num_buckets() const {
        return m_num_buckets;
    }

    size_t num_bits() const {
        return 8 * (sizeof(m_num_buckets) + sizeof(m_M_num_buckets));
    }

    template <typename Visitor>
    void visit(Visitor& visitor) const {
        visit_impl(visitor, *this);
    }

    template <typename Visitor>
    void visit(Visitor& visitor) {
        visit_impl(visitor, *this);
    }

    template <typename Visitor>
    void visit(const std::string name, Visitor& visitor) {
        visit_impl(name, visitor, *this);
    }

private:
    template <typename Visitor, typename T>
    static void visit_impl(Visitor& visitor, T&& t) {
        visitor.visit(t.m_num_buckets);
        visitor.visit(t.m_M_num_buckets);
    }
    template <typename Visitor, typename T>
    void visit_impl(const std::string, Visitor& visitor, T&& t) {
        visitor.dump("uniform_bucketer(");
        visitor.visit("m_num_buckets", t.m_num_buckets);
        visitor.dump(", ");
        visitor.visit("m_M_num_buckets", t.m_M_num_buckets);
        visitor.dump(")");
    }
    uint64_t m_num_buckets;
    __uint128_t m_M_num_buckets;
};

}  // namespace pthash

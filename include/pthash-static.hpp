#pragma once

#define PTHASH_STATIC

// Static vectors
template <typename T>
class noAlloc {
public:
    typedef T value_type;

    noAlloc() noexcept {}

    template<typename U>
    constexpr noAlloc(const noAlloc<U>&) noexcept {}

    T* allocate(std::size_t n) {
        (void)n;
        return reinterpret_cast<T*>(this);
    }

    void deallocate(T* p, std::size_t n) noexcept {
        (void)p; (void)n;
    }
#if 1
    template<typename U, typename... Args>
    void construct(U*, Args&&...) {
    }
    template<typename U>
    void destroy(U*) noexcept {
    }
#endif

    friend bool operator==(const noAlloc&, const noAlloc&) { return true; }
    friend bool operator!=(const noAlloc&, const noAlloc&) { return false; }
};
#define ALLOCATOR noAlloc
#define VECTOR(T) std::vector<T,noAlloc<T>>

#include "utils/encoders.hpp"
#include "single_phf.hpp"
#include "partitioned_phf.hpp"

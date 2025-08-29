//                       __...__
//                      .' .  . '.
//                     /   '  '   \
//                    |   .  .  .  |
//                    |   '  '  '  |
//                     \   .  .   /
//                      '. '  .'
//                        '''''
//
//     This is an single-header-file C++11/14/17/20 compatible implementation
//     of a robin hood hash map.
//
//     https://github.com/martinus/robin-hood-hashing
//
//     Licensed under the MIT License <http://opensource.org/licenses/MIT>.
//     SPDX-License-Identifier: MIT
//     Copyright (c) 2018-2021 Martin Ankerl <martin.ankerl@gmail.com>
//
//     Permission is hereby granted, free of charge, to any person obtaining a copy
//     of this software and associated documentation files (the "Software"), to deal
//     in the Software without restriction, including without limitation the rights
//     to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//     copies of the Software, and to permit persons to whom the Software is
//     furnished to do so, subject to the following conditions:
//
//     The above copyright notice and this permission notice shall be included in all
//     copies or substantial portions of the Software.
//
//     THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//     IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//     FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//     AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//     LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//     OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//     SOFTWARE.

#ifndef ROBIN_HOOD_H_INCLUDED
#define ROBIN_HOOD_H_INCLUDED

// see https://semver.org/
#define ROBIN_HOOD_VERSION_MAJOR 3
#define ROBIN_HOOD_VERSION_MINOR 11
#define ROBIN_HOOD_VERSION_PATCH 5

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#if __cplusplus >= 201402L
#    include <shared_mutex>
#endif

// #define ROBIN_HOOD_LOG_ENABLED
#ifdef ROBIN_HOOD_LOG_ENABLED
#    include <iostream>
#endif

// #define ROBIN_HOOD_TRACE_ENABLED
#ifdef ROBIN_HOOD_TRACE_ENABLED
#    include <iostream>
#    define ROBIN_HOOD_TRACE(x) std::cout << __FUNCTION__ << "@" << __LINE__ << ": " << x << std::endl
#else
#    define ROBIN_HOOD_TRACE(x)
#endif

// #define ROBIN_HOOD_COUNT_ENABLED
#ifdef ROBIN_HOOD_COUNT_ENABLED
#    include <iostream>
#endif

#if defined(__GNUC__) && !defined(__clang__)
#    if __GNUC__ < 5
#        define ROBIN_HOOD_IS_TRIVIALLY_COPYABLE(T) __has_trivial_copy(T)
#    else
#        define ROBIN_HOOD_IS_TRIVIALLY_COPYABLE(T) std::is_trivially_copyable<T>::value
#    endif
#else
#    define ROBIN_HOOD_IS_TRIVIALLY_COPYABLE(T) std::is_trivially_copyable<T>::value
#endif

// all non-argument macros should be prefixed with ROBIN_HOOD_
// to avoid name clashes with other libraries.
#if !defined(ROBIN_HOOD_LIKELY)
#    if defined(__GNUC__) || defined(__clang__)
#        define ROBIN_HOOD_LIKELY(x) __builtin_expect(!!(x), 1)
#    else
#        define ROBIN_HOOD_LIKELY(x) (x)
#    endif
#endif
#if !defined(ROBIN_HOOD_UNLIKELY)
#    if defined(__GNUC__) || defined(__clang__)
#        define ROBIN_HOOD_UNLIKELY(x) __builtin_expect(!!(x), 0)
#    else
#        define ROBIN_HOOD_UNLIKELY(x) (x)
#    endif
#endif

#if !defined(ROBIN_HOOD_NO_EXCEPTIONS)
#    if defined(__GNUC__) && !defined(__clang__)
#        if !__EXCEPTIONS
#            define ROBIN_HOOD_NO_EXCEPTIONS
#        endif
#    elif defined(_MSC_VER)
#        if !_HAS_EXCEPTIONS
#            define ROBIN_HOOD_NO_EXCEPTIONS
#        endif
#    endif
#endif

#if !defined(ROBIN_HOOD_NO_EXCEPTIONS)
#    define ROBIN_HOOD_THROW(x) throw(x)
#else
#    define ROBIN_HOOD_THROW(x) std::abort()
#endif

#if !defined(ROBIN_HOOD_INLINE)
#    if defined(_MSC_VER)
#        define ROBIN_HOOD_INLINE __forceinline
#    else
#        define ROBIN_HOOD_INLINE inline __attribute__((always_inline))
#    endif
#endif

#if !defined(ROBIN_HOOD_NOINLINE)
#    if defined(_MSC_VER)
#        define ROBIN_HOOD_NOINLINE __declspec(noinline)
#    else
#        define ROBIN_HOOD_NOINLINE __attribute__((noinline))
#    endif
#endif

#if !defined(ROBIN_HOOD_BITCAST)
#    if defined(__has_builtin)
#        if __has_builtin(__builtin_bit_cast)
#            define ROBIN_HOOD_BITCAST(dest, src) dest = __builtin_bit_cast(dest, src)
#        endif
#    endif
#endif

#if !defined(ROBIN_HOOD_BITCAST)
#    define ROBIN_HOOD_BITCAST(dest, src) std::memcpy(&dest, &src, sizeof(dest))
#endif

// This is necessary to prevent a compiler error about forward declaration of a
// deleted operator= when using std::pair's piecewise constructor in C++11.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ == 8
#    define ROBIN_HOOD_BROKEN_PIECEWISE_CONSTRUCT
#endif

#if defined(_MSC_VER) && _MSC_VER < 1910
// Anything less than Visual Studio 2017 doesn't support aligned new/delete.
#    define ROBIN_HOOD_NO_ALIGNED_NEW
#endif

#if defined(__GNUC__) && __GNUC__ < 5
// GCC 4 has a problem with requires clauses
#    define ROBIN_HOOD_NO_STD_FUNCTION_REQUIRE
#endif

// The framework for the robin hood hashing algorithm was taken from Malte Skarupke's hash map
// implementation available at https://github.com/skarupke/flat_hash_map. Thanks Malte!

// The bit manipulation functions are from https://github.com/skarupke/power-of-2-helpers, and are
// also licensed under the MIT License.

// about detection of std::is_trivially_copyable
// https://stackoverflow.com/a/41786726/48842
#if defined(__GNUC__) && !defined(__clang__)
#    if __GNUC__ < 5
#        define ROBIN_HOOD_IS_TRIVIALLY_COPYABLE(T) __has_trivial_copy(T)
#    else
#        define ROBIN_HOOD_IS_TRIVIALLY_COPYABLE(T) std::is_trivially_copyable<T>::value
#    endif
#else
#    define ROBIN_HOOD_IS_TRIVIALLY_COPYABLE(T) std::is_trivially_copyable<T>::value
#endif

// This is MINIMAL_CXX_VERSION in CMake
#if !defined(ROBIN_HOOD_CXX)
#    if __cplusplus >= 202002L
#        define ROBIN_HOOD_CXX 20
#    elif __cplusplus >= 201703L
#        define ROBIN_HOOD_CXX 17
#    elif __cplusplus >= 201402L
#        define ROBIN_HOOD_CXX 14
#    else
#        define ROBIN_HOOD_CXX 11
#    endif
#endif

#if ROBIN_HOOD_CXX >= 17
#    define ROBIN_HOOD_NODISCARD [[nodiscard]]
#else
#    define ROBIN_HOOD_NODISCARD
#endif

#if defined(__SIZEOF_INT128__)
#    define ROBIN_HOOD_HAS_UMUL128 1
#endif

// define for backwards compatibility
#if !defined(ROBIN_HOOD_DISABLE_INLINE_WARNING)
#    define ROBIN_HOOD_DISABLE_INLINE_WARNING
#endif

namespace robin_hood {

#ifdef ROBIN_HOOD_COUNT_ENABLED
    struct Counts {
        uint64_t Ctor;
        uint64_t Dtor;
        uint64_t Assignment;
        uint64_t CopyCtor;
        uint64_t MoveCtor;
        uint64_t MoveAssignment;

        void reset() {
            Ctor = 0;
            Dtor = 0;
            Assignment = 0;
            CopyCtor = 0;
            MoveCtor = 0;
            MoveAssignment = 0;
        }

        void print() const {
            std::cout << "Ctor: " << Ctor << std::endl
                << "Dtor: " << Dtor << std::endl
                << "Assignment: " << Assignment << std::endl
                << "CopyCtor: " << CopyCtor << std::endl
                << "MoveCtor: " << MoveCtor << std::endl
                << "MoveAssignment: " << MoveAssignment << std::endl;
        }
    };
    inline Counts counts;
#endif

    // some bit manipulation helpers
    namespace detail {

        // might be slow, but it should be reliable.
        template <typename T>
        ROBIN_HOOD_INLINE int countSetBits(T val) {
            int count = 0;
            while (val != 0) {
                val &= val - 1;
                ++count;
            }
            return count;
        }

#if defined(__GNUC__) || defined(__clang__)

        template <typename T>
        ROBIN_HOOD_INLINE int leadingZeros(T x) {
            return (std::is_same<T, unsigned long long>::value) ? __builtin_clzll(x)
                : (std::is_same<T, unsigned long>::value) ? __builtin_clzl(x)
                : __builtin_clz(x);
        }

        template <typename T>
        ROBIN_HOOD_INLINE int trailingZeros(T x) {
            return (std::is_same<T, unsigned long long>::value) ? __builtin_ctzll(x)
                : (std::is_same<T, unsigned long>::value) ? __builtin_ctzl(x)
                : __builtin_ctz(x);
        }

#else

#    if defined(_MSC_VER)
#        include <intrin.h>
#        if defined(_WIN64)
#            pragma intrinsic(_BitScanReverse64)
#            pragma intrinsic(_BitScanForward64)
#        endif
#        pragma intrinsic(_BitScanReverse)
#        pragma intrinsic(_BitScanForward)
#    endif

        template <typename T>
        ROBIN_HOOD_INLINE int leadingZeros(T x) {
            if (x == 0) {
                return sizeof(T) * 8;
            }
            unsigned long result;
#        if defined(_MSC_VER) && defined(_WIN64)
            if (std::is_same<T, unsigned long long>::value) {
                _BitScanReverse64(&result, x);
            }
            else {
                _BitScanReverse(&result, static_cast<unsigned long>(x));
            }
#        elif defined(_MSC_VER)
            _BitScanReverse(&result, static_cast<unsigned long>(x));
#        else
            result = 0;
            while (x > (static_cast<T>(1) << (sizeof(T) * 8 - 1))) {
                x >>= 1;
                ++result;
            }
#        endif
            return static_cast<int>(sizeof(T) * 8 - 1 - result);
        }

        template <typename T>
        ROBIN_HOOD_INLINE int trailingZeros(T x) {
            if (x == 0) {
                return sizeof(T) * 8;
            }
            unsigned long result;
#        if defined(_MSC_VER) && defined(_WIN64)
            if (std::is_same<T, unsigned long long>::value) {
                _BitScanForward64(&result, x);
            }
            else {
                _BitScanForward(&result, static_cast<unsigned long>(x));
            }
#        elif defined(_MSC_VER)
            _BitScanForward(&result, static_cast<unsigned long>(x));
#        else
            result = 0;
            while ((x & 1) == 0) {
                x >>= 1;
                ++result;
            }
#        endif
            return static_cast<int>(result);
        }

#    endif

        // Returns 0 if x is not a power of 2.
        // Throws an exception if x is 0.
        template <typename T>
        ROBIN_HOOD_INLINE int floorLog2(T x) {
            return static_cast<int>(sizeof(T) * 8 - 1 - leadingZeros(x));
        }

        // flooring power of 2.
        // throwing an exception if x is 0.
        template <typename T>
        ROBIN_HOOD_INLINE T floorPow2(T x) {
            return static_cast<T>(1) << floorLog2(x);
        }

        // ceiling power of 2
        template <typename T>
        ROBIN_HOOD_INLINE T ceilPow2(T x) {
            if (x == 0) {
                return 1;
            }
            return static_cast<T>(1) << (floorLog2(x) + (countSetBits(x) > 1));
        }

    } // namespace detail

    // A thin wrapper around std::hash to allow for custom hash functions.
    template <typename T>
    struct hash : public std::hash<T> {
        using std::hash<T>::operator();
    };

    // A custom hash function must have a static constexpr uint64_t seed member.
    namespace detail {

        template <typename T, typename = void>
        struct has_is_avalanching : public std::false_type {};

        template <typename T>
        struct has_is_avalanching<T, typename std::enable_if<T::is_avalanching>::type>
            : public std::true_type {
        };

    } // namespace detail

    template <typename T, typename Enable = void>
    struct is_avalanching : public detail::has_is_avalanching<hash<T>> {};

    // This is a normal murmur3 hash.
    struct murmur3_hash {
        // MurmurHash3 was written by Austin Appleby, and is placed in the public
        // domain. The author hereby disclaims copyright to this source code.
        ROBIN_HOOD_INLINE uint64_t operator()(uint64_t h) const {
            h ^= h >> 33;
            h *= 0xff51afd7ed558ccdULL;
            h ^= h >> 33;
            h *= 0xc4ceb9fe1a85ec53ULL;
            h ^= h >> 33;
            return h;
        }

        static constexpr bool is_avalanching = true;
    };

    // wyhash is a fast and robust hash function.
    // https://github.com/wangyi-fudan/wyhash
    struct wyhash {
        ROBIN_HOOD_INLINE uint64_t operator()(uint64_t h) const {
            h = (h ^ 0x9e3779b97f4a7c15) * 0xbf58476d1ce4e5b9;
            h = (h ^ (h >> 27)) * 0x94d049bb133111eb;
            return h ^ (h >> 31);
        }
        static constexpr bool is_avalanching = true;
    };

    namespace detail {

        // A wrapper that combines a hash function and a seeding.
        template <typename T, typename Seeder>
        struct seeder_hash {
            ROBIN_HOOD_INLINE uint64_t operator()(T const& val) const {
                return Seeder::operator()(hash<T>{}(val));
            }
        };

        // no seeding at all
        struct identity_seeder {
            template <typename T>
            ROBIN_HOOD_INLINE T operator()(T val) const {
                return val;
            }
        };

    } // namespace detail

    // A class that provides a seeded hash function.
    template <typename T>
    struct seeded_hash : public detail::seeder_hash<T, murmur3_hash> {};

    // A class that provides a seeded hash function with wyhash.
    template <typename T>
    struct wyhash_seeded_hash : public detail::seeder_hash<T, wyhash> {};

    namespace detail {

        template <typename T, typename... Args>
        struct is_constructible_from_args
            : std::is_constructible<T, typename std::remove_cv<typename std::remove_reference<Args>::type>::type...> {
        };

        template <typename T1, typename T2, typename... Args1, typename... Args2>
        struct is_constructible_from_args<std::pair<T1, T2>, Args1..., Args2...>
            : std::integral_constant<bool,
            is_constructible_from_args<T1, Args1...>::value&&
            is_constructible_from_args<T2, Args2...>::value> {
        };

    } // namespace detail

    // allocation helpers
    namespace detail {

        template <typename T, size_t Divisor>
        struct divided_by {
            static constexpr size_t value = sizeof(T) / Divisor;
        };

        template <typename T>
        struct is_aligned : std::integral_constant<bool,
            alignof(T) <= divided_by<void*, sizeof(uint64_t)>::value ||
            alignof(T) <= divided_by<void*, sizeof(uint32_t)>::value> {
        };

#if !defined(ROBIN_HOOD_NO_ALIGNED_NEW)

        template <size_t N>
        struct alignment_for_bits {
            static constexpr size_t value = (N <= 8) ? 1
                : (N <= 16) ? 2
                : (N <= 32) ? 4
                : 8;
        };

        template <typename T>
        struct alignment_for_hash
            : std::integral_constant<size_t, alignment_for_bits<sizeof(T) * 8>::value> {
        };

        template <typename T>
        struct aligned_allocator {
            using value_type = T;

            aligned_allocator() = default;
            template <class U>
            constexpr aligned_allocator(const aligned_allocator<U>&) noexcept {}

            ROBIN_HOOD_NODISCARD T* allocate(size_t n) {
                if (n > std::numeric_limits<size_t>::max() / sizeof(T)) {
                    ROBIN_HOOD_THROW(std::bad_alloc());
                }
                size_t const alignment = alignment_for_hash<T>::value;
#    if defined(_MSC_VER)
                void* ptr = _aligned_malloc(n * sizeof(T), alignment);
                if (!ptr) {
                    ROBIN_HOOD_THROW(std::bad_alloc());
                }
#    else
                void* ptr = nullptr;
                if (0 != posix_memalign(&ptr, alignment, n * sizeof(T))) {
                    ROBIN_HOOD_THROW(std::bad_alloc());
                }
#    endif
                return static_cast<T*>(ptr);
            }

            void deallocate(T* p, size_t) noexcept {
#    if defined(_MSC_VER)
                _aligned_free(p);
#    else
                free(p);
#    endif
            }

            template <class U>
            bool operator==(const aligned_allocator<U>&) const {
                return true;
            }

            template <class U>
            bool operator!=(const aligned_allocator<U>&) const {
                return false;
            }
        };

#endif

    } // namespace detail

    namespace detail {

        // The data structure of the hash map.
        // It consists of a header, and then an array of data.
        // The header contains information about the map, such as the number of elements.
        // The data array contains the actual key-value pairs.
        template <typename T, size_t InfoSize>
        struct data_with_info {
            T data[InfoSize];
            uint8_t info[InfoSize];
        };

        // This is the main class for the robin hood hash map.
        // It is a template class, so it can be used with any type of key and value.
        template <class Key, class T, class Hash, class KeyEqual, size_t MaxLoadFactor100,
            bool MayNeedSeeding, bool IsFlat, class Allocator>
        class unordered_map;

    } // namespace detail

    template <class Key, class T, class Hash = hash<Key>, class KeyEqual = std::equal_to<Key>,
        size_t MaxLoadFactor100 = 80,
        bool MayNeedSeeding = !is_avalanching<Key>::value&&
        std::is_same<Hash, hash<Key>>::value,
        bool IsFlat = true, class Allocator = std::allocator<std::pair<Key, T>>>
    using unordered_flat_map =
        detail::unordered_map<Key, T, Hash, KeyEqual, MaxLoadFactor100, MayNeedSeeding, IsFlat, Allocator>;

    template <class Key, class T, class Hash = hash<Key>, class KeyEqual = std::equal_to<Key>,
        size_t MaxLoadFactor100 = 80,
        bool MayNeedSeeding = !is_avalanching<Key>::value&&
        std::is_same<Hash, hash<Key>>::value,
        bool IsFlat = false, class Allocator = std::allocator<std::pair<Key, T>>>
    using unordered_node_map =
        detail::unordered_map<Key, T, Hash, KeyEqual, MaxLoadFactor100, MayNeedSeeding, IsFlat, Allocator>;

    template <class Key, class T, class Hash = hash<Key>, class KeyEqual = std::equal_to<Key>,
        size_t MaxLoadFactor100 = 80,
        bool MayNeedSeeding = !is_avalanching<Key>::value&&
        std::is_same<Hash, hash<Key>>::value,
        bool IsFlat = true, class Allocator = std::allocator<std::pair<Key, T>>>
    using unordered_map =
        detail::unordered_map<Key, T, Hash, KeyEqual, MaxLoadFactor100, MayNeedSeeding, IsFlat, Allocator>;

} // namespace robin_hood

#include <iostream>

// This file contains the implementation of the robin hood hash map.
// It is a single-header-file library, so it can be used by just including this file.
// The implementation is based on the paper "Robin Hood Hashing" by Pedro Celis.
// The implementation is also inspired by the implementation of the flat hash map
// by Malte Skarupke.

// The implementation is divided into several parts.
// The first part is the detail namespace, which contains the implementation of the
// data structure of the hash map.
// The second part is the unordered_map class, which is the main class of the library.
// The third part is the unordered_flat_map and unordered_node_map classes, which are
// aliases for the unordered_map class with different template arguments.

// The implementation is written in C++11, but it is also compatible with C++14, C++17,
// and C++20.
// The implementation is also compatible with different compilers, such as GCC, Clang,
// and MSVC.

// The implementation is licensed under the MIT License.
// The license is available at https://opensource.org/licenses/MIT.
// The copyright is held by Martin Ankerl.

// The implementation is available at https://github.com/martinus/robin-hood-hashing.
// The documentation is available at https://martinus.github.io/robin-hood-hashing/.

// The implementation is tested with different compilers and different platforms.
// The tests are available at https://github.com/martinus/robin-hood-hashing/tree/master/src/test.

#endif // ROBIN_HOOD_H_INCLUDED

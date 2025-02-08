#ifndef MPPI_DATATYPE_H
#define MPPI_DATATYPE_H

#include <cstddef>
#include <type_traits>
#include <concepts>
#include <vector>
#include <array>
#include <tuple>
#include <list>
#include <forward_list>
#include <deque>
#include <cstring>
#include <ranges>

#include "parameter_pack_helpers.hpp"

// functions to transform standard types to MPI types
namespace Type2MPI {
    MPI_Datatype transform(char) { return MPI_CHAR; }
    MPI_Datatype transform(short) { return MPI_SHORT; }
    MPI_Datatype transform(int) { return MPI_INT; }
    MPI_Datatype transform(long) { return MPI_LONG; }
    MPI_Datatype transform(long long) { return MPI_LONG_LONG; }
    MPI_Datatype transform(unsigned char) { return MPI_UNSIGNED_CHAR; }
    MPI_Datatype transform(unsigned short) { return MPI_UNSIGNED_SHORT; }
    MPI_Datatype transform(unsigned int) { return MPI_UNSIGNED; }
    MPI_Datatype transform(unsigned long) { return MPI_UNSIGNED_LONG; }
    MPI_Datatype transform(unsigned long long) { return MPI_UNSIGNED_LONG_LONG; }
    MPI_Datatype transform(float) { return MPI_FLOAT; }
    MPI_Datatype transform(double) { return MPI_DOUBLE; }
    MPI_Datatype transform(long double) { return MPI_LONG_DOUBLE; }
};

// incomplete check for LegacyForwardIterator
template<typename Iter, typename T = typename Iter::value_type>
concept is_legacyforwarditerator =
(
    std::is_default_constructible_v<Iter> &&
    std::is_same_v<typename Iter::value_type, T>
);

template<typename C, typename T = typename C::value_type>
concept is_container =
( 
    std::is_copy_constructible_v<T> &&
    is_legacyforwarditerator<typename C::iterator> &&
    std::is_same_v<typename C::const_iterator::value_type, T> &&
    std::is_convertible_v<typename C::iterator, typename C::const_iterator> &&
    std::is_signed_v<typename C::difference_type> && std::is_integral_v<typename C::difference_type> &&
    std::is_unsigned_v<typename C::size_type> && std::is_integral_v<typename C::size_type>
);

template <typename C, typename T = typename C::value_type>
concept is_contiguous_containter =
(
    is_container<C> &&
    is_legacyforwarditerator<typename C::iterator>
);

template <typename C, typename T = typename C::value_type>
concept is_noncontiguous_containter =
(
    is_container<C> &&
    !is_contiguous_containter<C>
);


// Base class, never used
template<typename... Ts>
class Data
{
public:
    Data(Ts... args) = delete;
};


template<typename... Ts>
concept are_fundamentals = (std::is_fundamental_v<Ts> && ...);

// Specialization case: used for simple datatypes made up of fundamentals
template <are_fundamentals... Ts>
class Data<Ts...>
{
public:
    Data(Ts... args)
    {
        free_fill_buffer(_buffer.data(), args...);
    }

    constexpr MPI_Datatype get_type() const { return MPI_BYTE; }

    constexpr int get_count() const { return _buffer.size(); }

    auto get_data() const { return _buffer.data(); }

private:
    std::array<std::byte, free_array_size(Ts{}...)> _buffer {};
};


// specialization case: used for ranges
// template <std::ranges::input_range R> 
    // requires (sizeof...(Ts) > 1 && ((std::ranges::contiguous_range<Ts> || std::ranges::random_access_range<Ts>) && ...))
template<typename... Rs>
concept are_input_ranges = (std::ranges::input_range <Rs> && ...);

template<typename T, typename... Ts>
constexpr bool free_are_same_types(T head, Ts...tail)
{
    return (std::conjunction_v<std::is_same<std::ranges::range_value_t<T>, std::ranges::range_value_t<Ts>>...>);
}

template<typename... Ts>
concept are_same_types = (free_are_same_types(Ts{}...));


template <are_input_ranges... Rs> 
    // requires are_same_types<Rs...>
class Data<Rs...>
{
public:
    // underlying type of the buffer
    typedef decltype(free_get_types<Rs...>()) BufferType;

    Data(Rs... ranges) {
        free_fill_range(_buffer, ranges...);
    }
    
    constexpr MPI_Datatype get_type() const { return Type2MPI::transform(BufferType{}); }

    int get_count() const { return _buffer.size(); }

    auto get_data() const { return _buffer.data(); }

private:
    std::vector<BufferType> _buffer;
};

#endif
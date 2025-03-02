#ifndef MPPI_DATATYPE_H
#define MPPI_DATATYPE_H

#include <cstddef>
#include <vector>
#include <array>
#include <tuple>
#include <list>
#include <forward_list>
#include <deque>
#include <cstring>
#include <memory_resource>

#include "parameter_pack_helpers.hpp"
#include "custom_concepts.hpp"
#include "datapattern.hpp"

#include "mpi.h"

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

struct Send{};
struct Recv{};

template<typename T>
concept is_send_or_recv = ( std::is_same_v<T, Send> || std::is_same_v<T, Recv> );


/* ------------------------------ Base: never used ------------------------------ */
template<typename... Ts>
class Data
{
public:
    Data(Ts... args) = delete;
};


/* ------------------------------ Specialization: multiple fundamentals ------------------------------ */
template <is_send_or_recv SR, is_polymorphic_memory_resource MemRes, are_fundamentals... Fs>
class Data<SR, MemRes, Fs...>
{
public:
    Data(SR, MemRes& memres, Fs... fundamentals)
        : _buffer { &memres } 
    {
        std::size_t offset = 0;
        _buffer.resize(size_of_fundamentals(offset, fundamentals...));
        // if we are sending data, we need to copy the args
        if constexpr ( std::is_same_v<SR, Send> )
            fill_buffer(_buffer.data(), offset, fundamentals...);
    }

    void retrieve_data(Fs&... fundamentals)
    {
        size_t offset = 0;
        fill_fundamentals(offset, fundamentals...);
    }

    constexpr MPI_Datatype get_type() const { return MPI_BYTE; }
    int get_count() const { return _buffer.size(); }
    auto get_data() { return _buffer.data(); }

private:
    // calculate size of types including algnment
    // overload
    template<typename T>
    constexpr std::size_t size_of_fundamentals(size_t offset, T const& /* head */)
    {
        // check if offset matches alignment of head
        auto mod = offset % alignof(T);
        if (mod != 0)
            offset += alignof(T) - mod;

        offset += sizeof(T);
        return offset;
    }
    // base case
    template<typename T, typename... Ts>
    constexpr std::size_t size_of_fundamentals(size_t offset, T const& /* head */, Ts const&... tail)
    {
        // check if offset matches alignment of head
        auto mod = offset % alignof(T);
        if (mod != 0)
            offset += alignof(T) - mod;
        
        offset += sizeof(T);
        return size_of_fundamentals(offset, tail...);
    }

    // put values of variadic template into buffer of bytes
    // overload
    template<typename T>
    constexpr void fill_buffer(std::byte* buffer, size_t offset, T const& head) 
    {
        // check if offset matches alignment of head
        auto mod = offset % alignof(T);
        if (mod != 0)
            offset += alignof(T) - mod;
        
        std::memcpy(buffer + offset, &head, sizeof(head));
    }
    // base case
    template<typename T, typename... Ts>
    constexpr void fill_buffer(std::byte* buffer, size_t offset, T const& head, Ts const&... tail)
    {
        // check if offset matches alignment of head
        auto mod = offset % alignof(T);
        if (mod != 0)
            offset += alignof(T) - mod;
        
        std::memcpy(buffer + offset, &head, sizeof(head));
        offset += sizeof(head);
        fill_buffer(buffer, offset, tail...);
    }


    template<typename T>
    void fill_fundamentals(size_t offset, T& head)
    {
        // check if offset matches alignment of head
        auto mod = offset % alignof(T);
        if (mod != 0)
            offset += alignof(T) - mod;
        
        std::memcpy(&head, _buffer.data() + offset, sizeof(head)); 
    }
    template<typename T, typename... Ts>
    void fill_fundamentals(size_t offset, T& head, Ts&... tail)
    {
        // check if offset matches alignment of head
        auto mod = offset % alignof(T);
        if (mod != 0)
            offset += alignof(T) - mod;
        
        std::memcpy(&head, _buffer.data() + offset, sizeof(head));
        offset += sizeof(T);

        fill_fundamentals(offset, tail...);
    }

    std::pmr::vector<std::byte> _buffer;
};


/* ------------------------------ Specialization: multiple ranges ------------------------------ */
template <is_send_or_recv SR, is_polymorphic_memory_resource MemRes, are_input_ranges... Rs> 
    // requires are_same_types<Rs...> 
    requires must_be_copied<Rs...>
class Data<SR, MemRes, Rs...>
{
public:
    // constructor that fills the internal _buffer
    Data(SR, MemRes& mem_res, Rs&... ranges)
        : _buffer{ &mem_res }
    {
        if constexpr ( std::is_same_v<SR, Send>)
            fill_buffer(_buffer, ranges...);
        else
            _buffer.resize(range_size(ranges...));
    }

    void retrieve_data(Rs&... ranges)
    {
        fill_range(_buffer.data(), ranges...);
    }
    
    constexpr MPI_Datatype get_type() const { return Type2MPI::transform(BufferType{}); }
    int get_count() const { return _buffer.size(); }
    auto get_data() { return _buffer.data(); }

private:
    // underlying type of the buffer
    typedef decltype(get_first_underlying_type<Rs...>()) BufferType;

    // base case
    template<typename C, typename T>
    constexpr void fill_buffer(C& container, T const& head)
    {
        std::ranges::copy(head, std::back_inserter(container));
    }
    // specialization case
    template<typename C, typename T, typename... Ts>
    constexpr void fill_buffer(C& container, T const& head, Ts const&... tail)
    {
        std::ranges::copy(head, std::back_inserter(container));

        fill_buffer(_buffer, tail...);
    }

    // base case
    template<typename Iter, typename T>
    constexpr void fill_range(Iter iter, T const& head)
    {
        auto range_size = std::ranges::distance(head);
        auto iter_end = iter + range_size;

        std::ranges::copy(iter, iter_end, head.begin());
    }
    // specialization case
    template<typename Iter, typename T, typename... Ts>
    constexpr void fill_range(Iter iter, T const& head, Ts const&... tail)
    {
        auto range_size = std::ranges::distance(head);
        auto iter_end = iter + range_size;

        std::ranges::copy(iter, iter_end, head.begin());

        iter = std::move(iter_end);
        fill_range(iter, tail...);
    }

    std::pmr::vector<BufferType> _buffer;
};


/* ------------------------------ Specialization: single ranges ------------------------------ */
template <is_send_or_recv SR, is_polymorphic_memory_resource MemRes, std::ranges::input_range R>
class Data<SR, MemRes, R>
{
public:
    // TODO: works with views, but ranges need to be given by reference for this to work
    // data is only referenced by _buffer_ptr, so no copying done
    Data(SR, MemRes&, R& range)
    {
        _buffer_ptr = get_buffer_ptr(range);
        _buffer_size = static_cast<int>(get_range_size(range));
    }

    void retrieve_data(R& range)
    {
        if (_buffer_ptr != get_buffer_ptr(range))
            copy_to_range(range);
    }

    constexpr MPI_Datatype get_type() const { return Type2MPI::transform(BufferType{}); }
    int get_count() const { return _buffer_size; }
    auto get_data() { return _buffer_ptr; }

private:
    // underlying type of the buffer
    typedef decltype(get_first_underlying_type<R>()) BufferType;

    template<typename T>
    BufferType* get_buffer_ptr(T& head) { /* return std::addressof(*head.begin()); */ return head.data(); }

    template<typename T>
    auto get_range_size(T& head) { return std::ranges::distance(head); }

    template<typename T>
    void copy_to_range(T range) { std::memcpy(range.data(), _buffer_ptr, _buffer_size * sizeof(BufferType)); }

    BufferType* _buffer_ptr {};
    int _buffer_size {};
};


/* ------------------------------ Specialization: ranges with DataPattern ------------------------------ */
template <is_send_or_recv SR, is_polymorphic_memory_resource MemRes, typename... Ts, are_input_ranges... Rs> 
class Data<SR, MemRes, DataPattern<Ts...>, Rs...>
{
public:
    // constructor that fills the internal _buffer
    Data(SR, MemRes& mem_res, DataPattern<Ts...> const& data_pattern, Rs&... ranges)
        : _buffer{ &mem_res }
    {
        // resize _buffer
        _buffer.resize(range_size(ranges...) * data_pattern.get_size());
        // check if we want to send and have to copy the data
        if constexpr ( std::is_same_v<SR, Send>)
        {
            // copy data
            constexpr size_t offset = 0;
            fill_pattern(data_pattern, offset, ranges...);
        }
    }

    void retrieve_data(DataPattern<Ts...> const& data_pattern, Rs&... ranges)
    {
        size_t offset = 0;
        fill_buffer(data_pattern, offset, ranges...);
    }
    
    constexpr MPI_Datatype get_type() const { return MPI_BYTE; }
    int get_count() const { return _buffer.size(); }
    auto get_data() { return _buffer.data(); }

private:
    // base case
    template<typename Pattern, typename U>
    constexpr void fill_pattern(Pattern const& pattern, size_t offset, U& range)
    {
        // loop through range
        for (auto& element : range)
            offset = pattern.store_from_type(_buffer.data(), &element, offset);
    }
    // specialization case
    template<typename Pattern, typename U, typename... Us>
    constexpr void fill_pattern(Pattern const& pattern, size_t offset, U& range, Us&... tail)
    {
        // loop through range
        for (auto& element : range)
            offset = pattern.store_from_type(_buffer.data(), &element, offset);

        fill_pattern(pattern, offset, tail...);
    }

    // base case
    template<typename Pattern, typename U>
    constexpr void fill_buffer(Pattern const& pattern, size_t offset, U& range)
    {
        // loop through range
        for (auto& element : range)
            offset = pattern.load_to_type(&element, _buffer.data(), offset);
    }
    // specialization case
    template<typename Pattern, typename U, typename... Us>
    constexpr void fill_buffer(Pattern const& pattern, size_t offset, U& range, Us&... tail)
    {
        // loop through range
        for (auto& element : range)
            offset = pattern.load_to_type(&element, _buffer.data(), offset);

        fill_buffer(pattern, offset, tail...);
    }


    std::pmr::vector<std::byte> _buffer;
};

#endif
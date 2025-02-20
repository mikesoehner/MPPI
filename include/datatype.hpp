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

// Base class, never used
template<typename... Ts>
class Data
{
public:
    Data(Ts... args) = delete;
};


// Specialization case: used for simple datatypes made up of fundamentals
template <is_send_or_recv SR, is_polymorphic_memory_resource MemRes, are_fundamentals... Ts>
class Data<SR, MemRes, Ts...>
{
public:
    Data(SR, MemRes& memres, Ts... args)
        : _buffer { &memres } 
    {
        _buffer.resize(free_array_size(Ts{}...));
        // if we are sending data, we need to copy the args
        if constexpr ( std::is_same_v<SR, Send> )
            free_fill_buffer(_buffer.data(), args...);
    }

    void retrieve_data(Ts&... args)
    {

    }

    constexpr MPI_Datatype get_type() const { return MPI_BYTE; }
    constexpr int get_count() const { return _buffer.size(); }
    auto get_data() const { return _buffer.data(); }

private:
    std::pmr::vector<std::byte> _buffer;
};

// Specialization case: used for ranges
template <is_send_or_recv SR, is_polymorphic_memory_resource MemRes, are_input_ranges... Rs> 
    // requires are_same_types<Rs...> 
    requires must_be_copied<Rs...>
class Data<SR, MemRes, Rs...>
{
public:
    // constructor that fills the internal _buffer
    Data(SR, MemRes& mem_res, Rs... ranges)
        : _buffer{ &mem_res }
    {
        if constexpr ( std::is_same_v<SR, Send>)
            free_fill_range(_buffer, ranges...);
        else
            free_resize_range(_buffer, ranges...);
    }

    void retrieve_data(Rs&... ranges)
    {
        free_copy_range(_buffer, ranges...);
    }
    
    constexpr MPI_Datatype get_type() const { return Type2MPI::transform(BufferType{}); }
    int get_count() const { return _buffer.size(); }
    auto get_data() { return _buffer.data(); }

private:
    // underlying type of the buffer
    typedef decltype(free_get_types<Rs...>()) BufferType;

    std::pmr::vector<BufferType> _buffer;
};

// Specialization case: used for ranges
template <is_send_or_recv SR, is_polymorphic_memory_resource MemRes, are_input_ranges... Rs> 
    // requires are_same_types<Rs...> 
    requires can_be_refed<Rs...>
class Data<SR, MemRes, Rs...>
{
public:
    // TODO: works with views, but ranges need to be given by reference for this to work
    // data is only referenced by _buffer_ptr, so no copying done
    Data(SR, MemRes&, Rs&... ranges)
        requires are_only_ranges<Rs...>
    {
        _buffer_ptr = free_get_buffer_ptr(ranges...);
        _buffer_size = static_cast<int>(free_get_range_size(ranges...));
    }

    Data(SR, MemRes&, Rs... ranges)
        requires are_only_views<Rs...>
    {
        _buffer_ptr = free_get_buffer_ptr(ranges...);
        _buffer_size = static_cast<int>(free_get_range_size(ranges...));
    }

    void retrieve_data(Rs&... ranges)
    {
        free_copy_to_range(ranges...);
    }

    constexpr MPI_Datatype get_type() const { return Type2MPI::transform(BufferType{}); }
    int get_count() const { return _buffer_size; }
    auto get_data() { return _buffer_ptr; }

private:
    // underlying type of the buffer
    typedef decltype(free_get_types<Rs...>()) BufferType;

    template<typename T>
    BufferType* get_buffer_ptr(T& head) 
    { 
        // return std::addressof(*head.begin());
        return head.data(); 
    }
    template<typename... Ts>
    BufferType* free_get_buffer_ptr(Ts&... ranges) { return get_buffer_ptr(ranges...); }

    template<typename T>
    auto free_get_range_size(T& head) { return std::ranges::distance(head); }
    template<typename... Ts>
    auto free_get_range_size(Ts&... ranges) { return get_range_size(ranges...); }

    template<typename T>
    void copy_to_range(T range)
    {
        std::memcpy(range.data(), _buffer_ptr, _buffer_size * sizeof(BufferType));
    }
    template<typename... Ts>
    void free_copy_to_range(Ts... ranges)
    {
        copy_to_range(ranges...);
    }

    BufferType* _buffer_ptr {};
    int _buffer_size {};
};

#endif
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


namespace mppi
{
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
            constexpr std::size_t offset = 0;
            _buffer.resize(size_of_fundamentals<offset>(fundamentals...));
            // if we are sending data, we need to copy the args
            if constexpr ( std::is_same_v<SR, Send> )
                pack<offset>(_buffer.data(), fundamentals...);
        }

        void retrieve_data(Fs&... fundamentals)
        {
            constexpr size_t offset = 0;
            unpack<offset>(fundamentals...);
        }

        constexpr MPI_Datatype get_type() const { return MPI_BYTE; }
        int get_count() const { return _buffer.size(); }
        auto get_data() { return _buffer.data(); }

    private:
        // calculate size of types including algnment
        // overload
        template<size_t offset, typename T>
        constexpr size_t size_of_fundamentals(T const& /* head */)
        {
            // check if offset matches alignment of head
            constexpr auto mod = offset % alignof(T);
            constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;

            return offset + sizeof(T) + adjust_for_alignment;
        }
        // base case
        template<size_t offset, typename T, typename... Ts>
        constexpr size_t size_of_fundamentals(T const& /* head */, Ts const&... tail)
        {
            // check if offset matches alignment of head
            constexpr auto mod = offset % alignof(T);
            constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;

            return size_of_fundamentals<offset + sizeof(T) + adjust_for_alignment>(tail...);
        }

        // put values of variadic template into buffer of bytes
        // overload
        template<size_t offset, typename T>
        constexpr void pack(std::byte* buffer, T const& head) 
        {
            // check if offset matches alignment of head
            constexpr auto mod = offset % alignof(T);
            constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;
            
            std::memcpy(buffer + offset + adjust_for_alignment, &head, sizeof(T));
        }
        // base case
        template<size_t offset, typename T, typename... Ts>
        constexpr void pack(std::byte* buffer, T const& head, Ts const&... tail)
        {
            // check if offset matches alignment of head
            constexpr auto mod = offset % alignof(T);
            constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;
            
            std::memcpy(buffer + offset + adjust_for_alignment, &head, sizeof(T));

            pack<offset + adjust_for_alignment + sizeof(T)>(buffer, tail...);
        }


        template<size_t offset, typename T>
        constexpr void unpack(T& head)
        {
            // check if offset matches alignment of head
            constexpr auto mod = offset % alignof(T);
            constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;
            
            std::memcpy(&head, _buffer.data() + offset + adjust_for_alignment, sizeof(T));
        }
        template<size_t offset, typename T, typename... Ts>
        constexpr void unpack(T& head, Ts&... tail)
        {
            // check if offset matches alignment of head
            constexpr auto mod = offset % alignof(T);
            constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;
            
            std::memcpy(&head, _buffer.data() + offset + adjust_for_alignment, sizeof(T));

            unpack<offset + adjust_for_alignment + sizeof(T)>(tail...);
        }

        std::pmr::vector<std::byte> _buffer;
    };


    /* ------------------------------ Specialization: multiple ranges ------------------------------ */
    template <is_send_or_recv SR, is_polymorphic_memory_resource MemRes, are_input_ranges... Rs> 
        requires are_same_types<Rs...> // underlying type of the ranges needs to be the same
    class Data<SR, MemRes, Rs...>
    {
    public:
        // constructor that fills the internal _buffer
        Data(SR, MemRes& mem_res, Rs&... ranges)
            : _buffer{ &mem_res }
        {
            if constexpr ( std::is_same_v<SR, Send>)
                (pack(_buffer, ranges), ...);
            else
                _buffer.resize(ranges_size(ranges...));
        }

        void retrieve_data(Rs&... ranges)
        {
            unpack(_buffer.begin(), ranges...);
        }
        
        constexpr MPI_Datatype get_type() const { return MPI_BYTE; }
        int get_count() const { return _buffer.size() * sizeof(BufferType); }
        auto get_data() { return _buffer.data(); }

    private:
        // underlying type of the buffer
        typedef decltype(get_first_underlying_type<Rs...>()) BufferType;

        // base case
        template<typename C, typename T>
        constexpr void pack(C& container, T& head)
        {
            std::ranges::copy(head, std::back_inserter(container));
        }

        // base case
        template<typename Iter, typename T>
        constexpr void unpack(Iter iter, T& head)
        {
            auto range_size = std::ranges::distance(head);
            auto iter_end = iter + range_size;

            std::ranges::copy(iter, iter_end, head.begin());
        }
        // specialization case
        template<typename Iter, typename T, typename... Ts>
        constexpr void unpack(Iter iter, T& head, Ts&... tail)
        {
            auto range_size = std::ranges::distance(head);
            auto iter_end = iter + range_size;

            std::ranges::copy(iter, iter_end, head.begin());

            iter = std::move(iter_end);
            unpack(iter, tail...);
        }

        std::pmr::vector<BufferType> _buffer;
    };


    /* ------------------------------ Specialization: single range ------------------------------ */
    template <is_send_or_recv SR, is_polymorphic_memory_resource MemRes, std::ranges::input_range R>
        requires std::ranges::contiguous_range<R> // range has to be contiguous (i.e. vector, array, etc.)
    class Data<SR, MemRes, R>
    {
    public:
        // TODO: works with views, but ranges need to be given by reference for this to work
        // data is only referenced by _buffer_ptr, so no copying done
        Data(SR, MemRes&, R& range)
        {
            _buffer_ptr = get_buffer_ptr(range);
            _buffer_size = static_cast<int>(ranges_size(range));
        }

        void retrieve_data(R& range)
        {
            if (_buffer_ptr != get_buffer_ptr(range))
                copy_to_range(range);
        }

        constexpr MPI_Datatype get_type() const { return MPI_BYTE; }
        int get_count() const { return _buffer_size * sizeof(BufferType); }
        auto get_data() { return _buffer_ptr; }

    private:
        // underlying type of the buffer
        typedef decltype(get_first_underlying_type<R>()) BufferType;

        BufferType* get_buffer_ptr(R& range) { /*return std::addressof(*range.begin());*/ return range.data(); }

        auto get_range_size(R& range) const { return std::ranges::distance(range); }

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
            _buffer.resize(ranges_size(ranges...) * data_pattern.get_size());
            // check if we want to send and have to copy the data
            if constexpr ( std::is_same_v<SR, Send>)
            {
                // copy data
                constexpr size_t offset = 0;
                pack(data_pattern, offset, ranges...);
            }
        }

        void retrieve_data(DataPattern<Ts...> const& data_pattern, Rs&... ranges)
        {
            size_t offset = 0;
            unpack(data_pattern, offset, ranges...);
        }
        
        constexpr MPI_Datatype get_type() const { return MPI_BYTE; }
        int get_count() const { return _buffer.size(); }
        auto get_data() { return _buffer.data(); }

    private:
        // base case
        template<typename Pattern, typename U>
        constexpr void pack(Pattern const& pattern, size_t offset, U& range)
        {
            // loop through range
            for (auto& element : range)
                offset = pattern.pack(_buffer.data(), &element, offset);
        }
        // specialization case
        template<typename Pattern, typename U, typename... Us>
        constexpr void pack(Pattern const& pattern, size_t offset, U& range, Us&... tail)
        {
            // loop through range
            for (auto& element : range)
                offset = pattern.pack(_buffer.data(), &element, offset);

            pack(pattern, offset, tail...);
        }

        // base case
        template<typename Pattern, typename U>
        constexpr void unpack(Pattern const& pattern, size_t offset, U& range)
        {
            // loop through range
            for (auto& element : range)
                offset = pattern.unpack(&element, _buffer.data(), offset);
        }
        // specialization case
        template<typename Pattern, typename U, typename... Us>
        constexpr void unpack(Pattern const& pattern, size_t offset, U& range, Us&... tail)
        {
            // loop through range
            for (auto& element : range)
                offset = pattern.unpack(&element, _buffer.data(), offset);

            unpack(pattern, offset, tail...);
        }


        std::pmr::vector<std::byte> _buffer;
    };
};

#endif
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
#include "pattern.hpp"
#include "reflection_helpers.hpp"

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
                fill_buffer<offset>(_buffer.data(), fundamentals...);
        }

        void retrieve_data(Fs&... fundamentals) const
        {
            constexpr size_t offset = 0;
            fill_fundamentals<offset>(fundamentals...);
        }

        constexpr MPI_Datatype get_type() const { return MPI_BYTE; }
        int get_count() const { return _buffer.size(); }
        auto get_data() { return _buffer.data(); }
        const auto get_data() const { return _buffer.data(); }

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
        constexpr void fill_buffer(std::byte* buffer, T const& head)
        {
            // check if offset matches alignment of head
            constexpr auto mod = offset % alignof(T);
            constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;
            
            std::memcpy(buffer + offset + adjust_for_alignment, &head, sizeof(T));
        }
        // base case
        template<size_t offset, typename T, typename... Ts>
        constexpr void fill_buffer(std::byte* buffer, T const& head, Ts const&... tail)
        {
            // check if offset matches alignment of head
            constexpr auto mod = offset % alignof(T);
            constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;
            
            std::memcpy(buffer + offset + adjust_for_alignment, &head, sizeof(T));

            fill_buffer<offset + adjust_for_alignment + sizeof(T)>(buffer, tail...);
        }


        template<size_t offset, typename T>
        constexpr void fill_fundamentals(T& head) const
        {
            // check if offset matches alignment of head
            constexpr auto mod = offset % alignof(T);
            constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;
            
            std::memcpy(&head, _buffer.data() + offset + adjust_for_alignment, sizeof(T));
        }
        template<size_t offset, typename T, typename... Ts>
        constexpr void fill_fundamentals(T& head, Ts&... tail) const
        {
            // check if offset matches alignment of head
            constexpr auto mod = offset % alignof(T);
            constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;
            
            std::memcpy(&head, _buffer.data() + offset + adjust_for_alignment, sizeof(T));

            fill_fundamentals<offset + adjust_for_alignment + sizeof(T)>(tail...);
        }

        std::pmr::vector<std::byte> _buffer;
    };


    /* ------------------------------ Specialization: multiple ranges ------------------------------ */
    template <is_send_or_recv SR, is_polymorphic_memory_resource MemRes, are_input_ranges... Rs> 
        requires are_same_types<Rs...> &&                       // value type of the ranges needs to be the same
                are_value_types_trivially_copyable<Rs...> &&    // value type needs to be trivially copyable
                (!contain_patterns<Rs...>)                      // Must not contain a pattern
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
                _buffer.resize(ranges_size(ranges...));
        }

        void retrieve_data(Rs&... ranges) const
        {
            fill_range(_buffer.begin(), ranges...);
        }
        
        constexpr MPI_Datatype get_type() const { return MPI_BYTE; }
        int get_count() const { return _buffer.size() * sizeof(BufferType); }
        auto get_data() { return _buffer.data(); }
        const auto get_data() const { return _buffer.data(); }

    private:
        // underlying type of the buffer
        typedef decltype(get_first_underlying_type<Rs...>()) BufferType;

        // base case
        template<typename C, typename T>
        constexpr void fill_buffer(C& container, T& head)
        {
            std::ranges::copy(head, std::back_inserter(container));
        }
        // specialization case
        template<typename C, typename T, typename... Ts>
        constexpr void fill_buffer(C& container, T& head, Ts&... tail)
        {
            std::ranges::copy(head, std::back_inserter(container));

            fill_buffer(_buffer, tail...);
        }

        // base case
        template<typename Iter, typename T>
        constexpr void fill_range(Iter iter, T& head) const
        {
            auto range_size = std::ranges::distance(head);
            auto iter_end = iter + range_size;

            std::ranges::copy(iter, iter_end, head.begin());
        }
        // specialization case
        template<typename Iter, typename T, typename... Ts>
        constexpr void fill_range(Iter iter, T& head, Ts&... tail) const
        {
            auto range_size = std::ranges::distance(head);
            auto iter_end = iter + range_size;

            std::ranges::copy(iter, iter_end, head.begin());

            iter = std::move(iter_end);
            fill_range(iter, tail...);
        }

        std::pmr::vector<BufferType> _buffer;
    };


    /* ------------------------------ Specialization: multiple ranges with underlying non-standard-layout type ---------------------------- */
    template <is_send_or_recv SR, is_polymorphic_memory_resource MemRes, are_input_ranges... Rs> 
        requires are_same_types<Rs...> &&                       // value type of the ranges needs to be the same
                (!are_value_types_trivially_copyable<Rs...>) && // specialization for non-trivial copy types
                (!contain_patterns<Rs...>)                      // check that no range has a pattern
    class Data<SR, MemRes, Rs...>
    {
    public:
        // constructor that fills the internal _buffer
        Data(SR, MemRes& mem_res, Rs&... ranges)
            : _buffer{ &mem_res }
        {
            // we want to create a datatype that gets all the members of a type and it's bases
            constexpr auto N = get_total_nb_members<BufferType>();
            constexpr auto indices = std::make_index_sequence<N>{};

            auto lambda = [&]<size_t... Indices>(auto type, std::index_sequence<Indices...>)
            {
                using T = decltype(type);

                return Pattern<T, expand_indices<Indices>() ...>();
            };

            auto pattern = lambda(BufferType{}, indices);

            // resize _buffer
            _buffer.resize(pattern.get_packed_ranges_size(ranges...));

            // check if we want to send and have to copy the data
            if constexpr ( std::is_same_v<SR, Send>)
            {
                // copy data
                constexpr size_t offset = 0;
                fill_pattern(pattern, offset, ranges...);
            }
        }

        void retrieve_data(Rs&... ranges) const
        {
            // we want to create a datatype that gets all the members of a type and it's bases
            constexpr auto N = get_total_nb_members<BufferType>();
            constexpr auto indices = std::make_index_sequence<N>{};

            auto lambda = [&]<size_t... Indices>(auto type, std::index_sequence<Indices...>)
            {
                using T = decltype(type);

                return Pattern<T, expand_indices<Indices>() ...>();
            };

            auto pattern = lambda(BufferType{}, indices);

            size_t offset = 0;
            fill_buffer(pattern, offset, ranges...);
        }
        
        constexpr MPI_Datatype get_type() const { return MPI_BYTE; }
        int get_count() const { return _buffer.size(); }
        auto get_data() { return _buffer.data(); }
        const auto get_data() const { return _buffer.data(); }


    private:
        // underlying type of the buffer
        typedef decltype(get_first_underlying_type<Rs...>()) BufferType;

        // base case
        template<typename Pattern, typename U>
        constexpr void fill_pattern(Pattern const& pattern, size_t offset, U& range)
        {
            // loop through range
            for (auto& element : range)
                offset = pattern.pack(_buffer.data(), &element, offset);
        }
        // specialization case
        template<typename Pattern, typename U, typename... Us>
        constexpr void fill_pattern(Pattern const& pattern, size_t offset, U& range, Us&... tail)
        {
            // loop through range
            for (auto& element : range)
                offset = pattern.pack(_buffer.data(), &element, offset);

            fill_pattern(pattern, offset, tail...);
        }

        // base case
        template<typename Pattern, typename U>
        constexpr void fill_buffer(Pattern const& pattern, size_t offset, U& range) const
        {
            // loop through range
            for (auto& element : range)
                offset = pattern.unpack(&element, _buffer.data(), offset);
        }
        // specialization case
        template<typename Pattern, typename U, typename... Us>
        constexpr void fill_buffer(Pattern const& pattern, size_t offset, U& range, Us&... tail) const
        {
            // loop through range
            for (auto& element : range)
                offset = pattern.unpack(&element, _buffer.data(), offset);

            fill_buffer(pattern, offset, tail...);
        }

        std::pmr::vector<std::byte> _buffer;
    };


    /* ------------------------------ Specialization: single range ------------------------------ */
    template <is_send_or_recv SR, is_polymorphic_memory_resource MemRes, std::ranges::input_range R>
        requires std::ranges::contiguous_range<R> &&    // Range has to be contigous
                 is_value_type_trivially_copyable<R> && // Value type of range has to be trivially copyable
                 (!contains_pattern<R>)                 // Should not contain a pattern
    class Data<SR, MemRes, R>
    {
    public:
        // data is only referenced by _buffer_ptr, so no copying done
        Data(SR, MemRes&, R& range)
        {
            _buffer_ptr = get_buffer_ptr(range);
            _buffer_size = static_cast<int>(ranges_size(range));
        }

        void retrieve_data(R& range) const
        {
            if (_buffer_ptr != get_buffer_ptr(range))
                copy_to_range(range);
        }

        constexpr MPI_Datatype get_type() const { return MPI_BYTE; }
        int get_count() const { return _buffer_size * sizeof(BufferType); }
        auto get_data() { return _buffer_ptr; }
        const auto get_data() const { return _buffer_ptr; }


    private:
        // underlying type of the buffer
        typedef decltype(get_first_underlying_type<R>()) BufferType;

        BufferType* get_buffer_ptr(R& range) const { /*return std::addressof(*range.begin());*/ return range.data(); }

        auto get_range_size(R& range) const { return std::ranges::distance(range); }

        template<typename T>
        void copy_to_range(T& range) const { std::memcpy(range.data(), _buffer_ptr, _buffer_size * sizeof(BufferType)); }

        BufferType* _buffer_ptr {};
        int _buffer_size {};
    };


    /* ------------------------------ Specialization: multiple ranges with same Pattern ------------------------------ */
    template <is_send_or_recv SR, is_polymorphic_memory_resource MemRes, are_only_views... Vs>
        requires are_same_types<Vs...>  &&      // value type of the ranges needs to be the same
                contain_patterns<Vs...> &&      // check if all ranges contain the same pattern
                contain_same_patterns<Vs...>    // check if all ranges contain the same pattern
    class Data<SR, MemRes, Vs...>
    {
    public:
        // constructor that fills the internal _buffer
        Data(SR, MemRes& mem_res, Vs... views)
            : _buffer{ &mem_res }
        {
            // resize _buffer
            resize(views...);

            // check if we want to send and have to copy the data
            if constexpr ( std::is_same_v<SR, Send>)
            {
                // copy data
                size_t offset = 0;
                (fill_pattern(offset, views), ...);
            }
        }

        void retrieve_data(Vs... views) const
        {
            size_t offset = 0;
            (fill_buffer(offset, views), ...);
        }
        
        constexpr MPI_Datatype get_type() const { return MPI_BYTE; }
        int get_count() const { return _buffer.size(); }
        auto get_data() { return _buffer.data(); }
        const auto get_data() const { return _buffer.data(); }

    private:
        // 
        template<typename V>
        constexpr void fill_pattern(size_t& offset, V view)
        {
            auto pattern = view.get_pattern();
            // loop through range
            for (auto const& element : view)
                offset = pattern.pack(_buffer.data(), &element, offset);
        }

        //
        template<typename V>
        constexpr void fill_buffer(size_t& offset, V view) const
        {
            auto pattern = view.get_pattern();
            // loop through range
            for (auto& element : view)
                offset = pattern.unpack(&element, _buffer.data(), offset);
        }

        template<typename T, typename... Ts>
        inline auto get_pattern(T head, Ts... tail) const
        {
            return head.get_pattern();
        }

        inline auto resize(Vs... views)
        {
            auto pattern = get_pattern(views...);
            _buffer.resize(pattern.get_packed_ranges_size(views...));
        }


        std::pmr::vector<std::byte> _buffer;
    };



    /* ------------------------------ Specialization: multiple ranges with different Patterns ------------------------------ */
    template <is_send_or_recv SR, is_polymorphic_memory_resource MemRes, are_only_views... Vs>
        requires are_same_types<Vs...>  &&      // Value type of the ranges needs to be the same
                contain_patterns<Vs...> &&      // Views must contain patterns
                (!contain_same_patterns<Vs...>) // Views must not contain the same pattern
    class Data<SR, MemRes, Vs...>
    {
    public:
        // constructor that fills the internal_buffer
        Data(SR, MemRes& mem_res, Vs... views)
            : _buffer{ &mem_res }
        {
            // resize _buffer
            size_t size = sizeof...(Vs) * sizeof(size_t);
            (calc_buffer_size(size, views), ...);
            _buffer.resize(size);

            // check if we want to send and have to copy the data
            if constexpr ( std::is_same_v<SR, Send>)
            {
                size_t offset = 0;
                // include metadata of how long the ranges are

                // fill buffer with metadata
                (append_view_size(offset, views), ...);

                // copy data
                (fill_pattern(views, offset), ...);
            }
        }

        void resize(size_t nb_bytes) { _buffer.resize(nb_bytes); }

        void get_metadata(Vs... views) const
        {
            size_t offset = 0;
            // include metadata of how long the ranges are
            auto retrieve_view_size = [this, &offset]<std::ranges::input_range V>(V view)
            {
                size_t size;
                std::memcpy(&size, _buffer.data() + offset, sizeof(size));
                offset += sizeof(size);

                view.resize(size);
            };

            (retrieve_view_size(views), ...);
        }

        void retrieve_data(Vs... views) const
        {
            size_t offset = sizeof...(Vs) * sizeof(size_t);
            (fill_buffer(views, offset), ...);
        }

        template<std::ranges::input_range V>
        void calc_buffer_size(size_t& size, V view)
        {
            auto pattern = view.get_pattern();
            auto mod = size % pattern.get_packed_size();
            if (mod != 0)
            {
                auto adjust_for_alignment = pattern.get_packed_size() - mod;
                size += adjust_for_alignment;
            }
            size += pattern.get_packed_ranges_size(view);
        }

        constexpr MPI_Datatype get_type() const { return MPI_BYTE; }
        int get_count() const { return _buffer.size(); }
        auto get_data() { return _buffer.data(); }
        const auto get_data() const { return _buffer.data(); }

    private:
        template<std::ranges::input_range V>
            requires is_not_nested_container<V>
        auto append_view_size(size_t& offset, V& view)
        {
            size_t size = view.size();
            std::memcpy(_buffer.data() + offset, &size, sizeof(size));
            offset += sizeof(size);
        };
        
        template<std::ranges::input_range V>
            requires (!is_not_nested_container<V>)
        auto append_view_size(size_t& offset, V& view)
        {
            size_t size = 0;
            for (auto const& subrange : view)
                size++;// size += subrange.size();
            
            std::memcpy(_buffer.data() + offset, &size, sizeof(size));
            offset += sizeof(size);
        };

        template<std::ranges::input_range V>
        inline void fill_pattern(V& view, size_t& offset)
        {
            auto pattern = view.get_pattern();
            // check for alignment of offset
            auto mod = offset % pattern.get_packed_size();
            if (mod != 0)
            {
                auto adjust_for_alignment = pattern.get_packed_size() - mod;
                offset += adjust_for_alignment;
            }

            // loop through range
            for (auto const& element : view)
                offset = pattern.pack(_buffer.data(), &element, offset);
        }

        template<std::ranges::input_range V>
        inline void fill_buffer(V& view, size_t& offset) const
        {
            auto pattern = view.get_pattern();
            // check for alignment of offset
            auto mod = offset % pattern.get_packed_size();
            if (mod != 0)
            {
                auto adjust_for_alignment = pattern.get_packed_size() - mod;
                offset += adjust_for_alignment;
            }

            // loop through range
            for (auto& element : view)
                offset = pattern.unpack(&element, _buffer.data(), offset);
        }


        std::pmr::vector<std::byte> _buffer;
    };


    /* ------------------------------ Specialization: single range with trivial Pattern ------------------------------ */
    template <is_send_or_recv SR, is_polymorphic_memory_resource MemRes, std::ranges::input_range V>
        requires std::ranges::contiguous_range<V> &&        // Range has to be contigous
                is_value_type_trivially_copyable<V> &&      // Value type of range has to be trivially copyable
                contains_pattern<V> &&                      // Must contain pattern
                std::is_trivially_copyable_v<typename V::Pattern_Type::base_type> &&          
                                                            // Original type must be trivially-copyable
                (V::Pattern_Type::is_trivial_pattern())     // Given Pattern must be trivial, i.e. contain all members of OriginalType
    class Data<SR, MemRes, V>
    {
    public:
        Data(SR, MemRes&, V view)
        {
            _buffer_ptr = get_buffer_ptr(view);
            _buffer_size = static_cast<int>(ranges_size(view));
        }

        void retrieve_data(V view) const
        {
            if (_buffer_ptr != get_buffer_ptr(view))
                copy_to_range(view);
        }

        constexpr MPI_Datatype get_type() const { return MPI_BYTE; }
        int get_count() const { return _buffer_size * sizeof(BufferType); }
        auto get_data() { return _buffer_ptr; }
        const auto get_data() const { return _buffer_ptr; }


    private:
        // underlying type of the buffer
        typedef decltype(get_first_underlying_type<V>()) BufferType;

        BufferType* get_buffer_ptr(V view) const { /*return std::addressof(*range.begin());*/ return view.data(); }

        auto get_range_size(V view) const { return std::ranges::distance(view); }

        void copy_to_range(V view) const { std::memcpy(view.data(), _buffer_ptr, _buffer_size * sizeof(BufferType)); }

        BufferType* _buffer_ptr {};
        int _buffer_size {};
    };
};

#endif
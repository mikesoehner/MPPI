#ifndef MPPI_DATAPATTERN_HPP
#define MPPI_DATAPATTERN_HPP

#include <experimental/meta>
#include <ranges>
#include <concepts>
#include <type_traits>
#include <cstddef>
#include <cstring>

#include "stringliteral.hpp"
#include "reflection_helpers.hpp" 
#include "custom_concepts.hpp"


template<typename OriginalType, StringLiteral... Identifiers>
class DataPattern
{
public:
    // Compile-time evaluated constructors
    // This constructor takes the StringLiterals as arguments
    consteval DataPattern()
        requires has_only_trivially_copyable_types<OriginalType, Identifiers...> && is_without_pointers<OriginalType, Identifiers...>
        : _size(calc_packed_size<OriginalType, Identifiers...>()),
        _sizes(get_sizes<OriginalType, Identifiers...>()),
        _offset_type(get_offsets<OriginalType, Identifiers...>()),
        _offset_buffer(calc_offsets_in_buffer<OriginalType, Identifiers...>())
    {}

    // This constructor takes predetermined arrays and works on them
    template<size_t N>
    consteval DataPattern(size_t size, std::array<size_t, N> sizes, std::array<size_t, N> offset_type, std::array<size_t, N> offset_buffer)
        requires has_only_trivially_copyable_types<OriginalType> && is_without_pointers<OriginalType, Identifiers...>
        : _size(size),
        _sizes(sizes),
        _offset_type(offset_type),
        _offset_buffer(offset_buffer)
    {}
    
    // Run-time evaluated constructors
    // This constructor takes the StringLiterals as arguments (the ranges are required to calculate the dynamic ranges)
    template<are_input_ranges... Rs>
    DataPattern(Rs... ranges)
        requires (!has_only_trivially_copyable_types<OriginalType, Identifiers...>) && is_without_pointers<OriginalType, Identifiers...>
        : _size(calc_packed_size_with_container_identifier<OriginalType, Identifiers...>(ranges...)),
        _sizes(get_sizes<OriginalType, Identifiers...>()),
        _offset_type(get_offsets<OriginalType, Identifiers...>()),
        _offset_buffer(calc_offsets_in_buffer<OriginalType, Identifiers...>())
    {}

    // This constructor takes predetermined arrays and works on them
    template<size_t N>
    DataPattern(size_t size, std::array<size_t, N> sizes, std::array<size_t, N> offset_type, std::array<size_t, N> offset_buffer)
        requires (!has_only_trivially_copyable_types<OriginalType>) && is_without_pointers<OriginalType, Identifiers...>
        : _size(size),
        _sizes(sizes),
        _offset_type(offset_type),
        _offset_buffer(offset_buffer)
    {}

    size_t store_from_type(std::byte* dest, OriginalType* base, size_t offset) const
    {
        constexpr auto N = sizeof...(Identifiers);
        constexpr auto Indices = std::make_index_sequence<N>{};

        // Target Members are compile-time resolvable
        if constexpr (has_only_trivially_copyable_types<OriginalType, Identifiers...>)
            return store_to_dest(dest, reinterpret_cast<std::byte*>(base), offset, Indices);
        // Target Members are not all compile-time resolvable
        else
            return store_to_dest_dynamic(dest, reinterpret_cast<std::byte*>(base), offset, Indices);
    }

    size_t load_to_type(OriginalType* base, std::byte* src, size_t offset) const
    {
        constexpr auto N = sizeof...(Identifiers);
        constexpr auto Indices = std::make_index_sequence<N>{};

        // Target Members are compile-time resolvable
        if constexpr (has_only_trivially_copyable_types<OriginalType, Identifiers...>)
            return load_from_src(reinterpret_cast<std::byte*>(base), src, offset, Indices);
        // Target Members are not all compile-time resolvable
        else
            return load_from_src_dynamic(reinterpret_cast<std::byte*>(base), src, offset, Indices);
    }

    constexpr auto get_size() const { return _size; }

private:

    template<size_t... Indices>
    constexpr size_t store_to_dest(std::byte* dest, std::byte* origin, size_t offset, std::index_sequence<Indices...> indices) const
    {
        ((std::memcpy(dest + offset + _offset_buffer[Indices], origin + _offset_type[Indices], _sizes[Indices])), ...);

        return offset + _size;
    }

    template<size_t Index>
    auto copy(std::byte* dest, std::byte* origin, size_t& offset) const
    {
        constexpr auto types = get_types<OriginalType, Identifiers...>();

        using T = typename [: types[Index] :];

        // this requires runtime information
        if constexpr (is_allocator_aware_container<T>)
        {
            auto container_ptr = reinterpret_cast<T*>(origin + _offset_type[Index]);
            
            auto data = container_ptr->data();
            auto size = container_ptr->size();

            using Value_Type = typename T::value_type;
            
            auto mod = offset % alignof(Value_Type);
            auto adjust_for_alignment = mod != 0 ? alignof(Value_Type) - mod : 0ul;
            offset += adjust_for_alignment;

            std::memcpy(dest + offset, data, size * sizeof(Value_Type));

            offset += size * sizeof(Value_Type);
        }
        // this not
        else
        {
            auto mod = offset % alignof(T);
            auto adjust_for_alignment = mod != 0 ? alignof(T) - mod : 0ul;
            offset += adjust_for_alignment;

            std::memcpy(dest + offset, origin + _offset_type[Index], _sizes[Index]);

            offset += _sizes[Index];
        }

        return offset;
    }

    template<size_t... Indices>
    size_t store_to_dest_dynamic(std::byte* dest, std::byte* origin, size_t offset, std::index_sequence<Indices...>) const
    {
        // the comma operator return the last instance of copy, 
        // since in copy offset is passed by reference it is the same offset in functions called here
        return (copy<Indices>(dest, origin, offset), ...);
    }


    template<size_t... Indices>
    constexpr size_t load_from_src(std::byte* origin, std::byte* src, size_t offset, std::index_sequence<Indices...> indices) const
    {
        ((std::memcpy(origin + _offset_type[Indices], src + offset + _offset_buffer[Indices], _sizes[Indices])), ...);

        return offset + _size;
    }

    template<size_t Index>
    auto copy_rev(std::byte* origin, std::byte* src, size_t& offset) const
    {
        constexpr auto types = get_types<OriginalType, Identifiers...>();

        using T = typename [: types[Index] :];
        
        if constexpr (is_allocator_aware_container<T>)
        {
            // this is a container
            auto container_ptr = reinterpret_cast<T*>(origin + _offset_type[Index]);
            
            auto data = container_ptr->data();
            auto size = container_ptr->size();

            using Value_Type = typename T::value_type;

            auto mod = offset % alignof(Value_Type);
            auto adjust_for_alignment = mod != 0 ? alignof(Value_Type) - mod : 0ul;
            offset += adjust_for_alignment;

            std::memcpy(data, src + offset, size * sizeof(Value_Type));

            offset += size * sizeof(Value_Type);
        }
        else
        {            
            auto mod = offset % alignof(T);
            auto adjust_for_alignment = mod != 0 ? alignof(T) - mod : 0ul;
            offset += adjust_for_alignment;

            std::memcpy(origin + _offset_type[Index], src + offset, _sizes[Index]);

            offset += _sizes[Index];
        }

        return offset;
    }

    template<size_t... Indices>
    constexpr size_t load_from_src_dynamic(std::byte* origin, std::byte* src, size_t offset, std::index_sequence<Indices...> indices) const
    {
        // the comma operator return the last instance of copy, 
        // since in copy offset is passed by reference it is the same offset in functions called here
        return (copy_rev<Indices>(origin, src, offset), ...);
    }

    // The size has a different meaning depending on the types in a datapattern
    // If all types are trivialy copyable, _size is the size of one instance serialized
    // If there is at least one allocator aware container, _size is the size of the entire range (because it is possible that one instance has a different size)
    const size_t _size {0};
    const std::array<size_t, sizeof...(Identifiers)> _sizes;
    const std::array<size_t, sizeof...(Identifiers)> _offset_type;
    const std::array<size_t, sizeof...(Identifiers)> _offset_buffer;
};


#endif
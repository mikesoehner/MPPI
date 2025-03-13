#ifndef MPPI_DATAPATTERN_HPP
#define MPPI_DATAPATTERN_HPP

#include <ranges>
#include <concepts>
#include <type_traits>
#include <cstddef>
#include <cstring>

template<typename Origin, typename... Members>
class DataPattern
{
public:
    DataPattern(Origin* origin, Members&... members)
    {
        size_t offset = 0;
        fill_displs_buffer<0>(origin, offset, members...);
        // adjust for alignment of first element again
        adjust_for_first_element(members...);   
    }

    size_t store_from_type(std::byte* dest, Origin* base, size_t offset) const
    {
        return store_to_dest<0>(dest, reinterpret_cast<std::byte*>(base), offset, Members{}...);
    }

    size_t load_to_type(Origin* base, std::byte* src, size_t offset) const
    {
        return load_from_src<0>(reinterpret_cast<std::byte*>(base), src, offset, Members{}...);
    }

    auto get_size() const { return _size; }

private:

    template<int I, typename T>
    constexpr void fill_displs_buffer(Origin* origin, size_t offset, T& head)
    {
        // displacement between start of struct and start of the value we want to copy
        _displacements[I] = std::distance(reinterpret_cast<std::byte*>(origin), reinterpret_cast<std::byte*>(&head));
        // displacement between data in the buffer we copy into
        auto mod = offset % alignof(T);
        if (mod != 0)
            offset += alignof(T) - mod;
        
        offset += sizeof(T);

        // we need to check for alignment with first element again
        _size = offset;
    }
    template<int I, typename T, typename... Ts>
    constexpr void fill_displs_buffer(Origin* origin, size_t offset, T& head, Ts&... tail)
    {
        // displacement between start of struct and start of the value we want to copy
        _displacements[I] = std::distance(reinterpret_cast<std::byte*>(origin), reinterpret_cast<std::byte*>(&head));
        // displacement between data in the buffer we copy into
        auto mod = offset % alignof(T);
        if (mod != 0)
            offset += alignof(T) - mod;
        
        offset += sizeof(T);

        fill_displs_buffer<I+1>(origin, offset, tail...);
    }

    template<typename T, typename... Ts>
    constexpr void adjust_for_first_element(T const& head, Ts const&... tails)
    {
        auto mod = _size % alignof(T);
        if (mod != 0)
            _size += alignof(T) - mod;
    }

    template<int I, typename T>
    constexpr size_t store_to_dest(std::byte* dest, std::byte* origin, size_t offset, T const& head) const
    {
        // check if offset matches alignment of head
        auto mod = offset % alignof(T);
        if (mod != 0)
            offset += alignof(T) - mod;

        std::memcpy(dest + offset, origin + _displacements[I], sizeof(T));

        return offset + sizeof(T);
    }
    template<int I, typename T, typename... Ts>
    constexpr size_t store_to_dest(std::byte* dest, std::byte* origin, size_t offset, T const& head, Ts const&... tail) const
    {
        // check if offset matches alignment of head
        auto mod = offset % alignof(T);
        if (mod != 0)
            offset += alignof(T) - mod;
        
        std::memcpy(dest + offset, origin + _displacements[I], sizeof(T));

        offset += sizeof(T);

        return store_to_dest<I+1>(dest, origin, offset, tail...);
    }

    template<int I, typename T>
    constexpr size_t load_from_src(std::byte* origin, std::byte* src, size_t offset, T const& head) const
    {
        // check if offset matches alignment of head
        auto mod = offset % alignof(T);
        if (mod != 0)
            offset += alignof(T) - mod;

        std::memcpy(origin + _displacements[I], src + offset, sizeof(T));

        return offset + sizeof(T);
    }
    template<int I, typename T, typename... Ts>
    constexpr size_t load_from_src(std::byte* origin, std::byte* src, size_t offset, T const& head, Ts const&...  tail) const
    {
        // check if offset matches alignment of head
        auto mod = offset % alignof(T);
        if (mod != 0)
            offset += alignof(T) - mod;

        std::memcpy(origin + _displacements[I], src + offset, sizeof(T));

        offset += sizeof(T);

        return load_from_src<I+1>(origin, src, offset, tail...);
    }

    std::array<std::ptrdiff_t, sizeof...(Members)> _displacements;
    size_t _size {0};
};

#endif
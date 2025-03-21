#ifndef MPPI_DATAPATTERN_HPP
#define MPPI_DATAPATTERN_HPP

#include <ranges>
#include <concepts>
#include <type_traits>
#include <cstddef>
#include <cstring>


template<size_t offset, typename T>
consteval auto calc_size(T)
{
    constexpr auto mod = offset % alignof(T);
    constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;

    return offset + sizeof(T) + adjust_for_alignment;
}
template<size_t offset, typename T, typename... Ts>
consteval auto calc_size(T, Ts...)
{
    constexpr auto mod = offset % alignof(T);
    constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;

    return calc_size<offset + sizeof(T) + adjust_for_alignment>(Ts{}...);
}

template<size_t size, typename T, typename... Ts>
consteval auto adjust_for_first_element(T const&, Ts const&...)
{
    constexpr auto mod = size % alignof(T);
    constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;
    
    return size + adjust_for_alignment;
}

template<typename Origin, typename... Members>
class DataPattern
{
public:
    DataPattern(Origin* origin, Members&... members)
    {
        // determine size of one packed element
        constexpr size_t offset = 0;
        constexpr auto tmp_size = calc_size<offset>(Members{}...);
        // adjust for alignment of first element again
        _size = adjust_for_first_element<tmp_size>(Members{}...);
        // fill buffer of offsets
        fill_displs_buffer<0>(origin, members...);
    }

    size_t store_from_type(std::byte* dest, Origin* base, size_t offset) const
    {
        constexpr size_t alignment = 0;
        return store_to_dest<alignment, 0>(dest, reinterpret_cast<std::byte*>(base), offset, Members{}...);
    }

    size_t load_to_type(Origin* base, std::byte* src, size_t offset) const
    {
        constexpr size_t alignment = 0;
        return load_from_src<alignment, 0>(reinterpret_cast<std::byte*>(base), src, offset, Members{}...);
    }

    auto get_size() const { return _size; }

private:

    template<int I, typename T>
    constexpr void fill_displs_buffer(Origin* origin, T& head)
    {
        // displacement between start of struct and start of the value we want to copy
        _displacements[I] = std::distance(reinterpret_cast<std::byte*>(origin), reinterpret_cast<std::byte*>(&head));
    }
    template<int I, typename T, typename... Ts>
    constexpr void fill_displs_buffer(Origin* origin, T& head, Ts&... tail)
    {
        // displacement between start of struct and start of the value we want to copy
        _displacements[I] = std::distance(reinterpret_cast<std::byte*>(origin), reinterpret_cast<std::byte*>(&head));

        fill_displs_buffer<I+1>(origin, tail...);
    }


    template<size_t alignment, int I, typename T>
    constexpr size_t store_to_dest(std::byte* dest, std::byte* origin, size_t offset, T const& head) const
    {
        // check if offset matches alignment of head
        constexpr auto mod = alignment % alignof(T);
        constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;

        offset += adjust_for_alignment;

        std::memcpy(dest + offset, origin + _displacements[I], sizeof(T));

        return offset + sizeof(T);
    }
    // TODO: Fix this function (replacing alignment with offset works, why? Maybe replace mod calculations with an array with stored precomputed results).
    template<size_t alignment, int I, typename T, typename... Ts>
    constexpr size_t store_to_dest(std::byte* dest, std::byte* origin, size_t offset, T const& head, Ts const&... tail) const
    {
        // check if offset matches alignment of head
        constexpr auto mod = alignment % alignof(T);
        constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;
        
        offset += adjust_for_alignment;

        std::memcpy(dest + offset, origin + _displacements[I], sizeof(T));

        offset += sizeof(T);

        return store_to_dest<alignment + adjust_for_alignment + sizeof(T), I+1>(dest, origin, offset, tail...);
    }

    template<size_t alignment, int I, typename T>
    constexpr size_t load_from_src(std::byte* origin, std::byte* src, size_t offset, T const& head) const
    {
        // check if offset matches alignment of head
        constexpr auto mod = alignment % alignof(T);
        constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;

        offset += adjust_for_alignment;

        std::memcpy(origin + _displacements[I], src + offset, sizeof(T));

        return offset + sizeof(T);
    }
    template<size_t alignment, int I, typename T, typename... Ts>
    constexpr size_t load_from_src(std::byte* origin, std::byte* src, size_t offset, T const& head, Ts const&...  tail) const
    {
        // check if offset matches alignment of head
        constexpr auto mod = alignment % alignof(T);
        constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;

        offset += adjust_for_alignment;

        std::memcpy(origin + _displacements[I], src + offset, sizeof(T));

        offset += sizeof(T);

        return load_from_src<alignment + adjust_for_alignment + sizeof(T), I+1>(origin, src, offset, tail...);
    }

    std::array<std::ptrdiff_t, sizeof...(Members)> _displacements;
    size_t _size {0};
};

#endif
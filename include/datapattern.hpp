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

template<typename T>
consteval auto max_alignment(T)
{
    return alignof(T);
}
template<typename T, typename... Ts>
consteval auto max_alignment(T, Ts...)
{
    return std::max(alignof(T), max_alignment(Ts{}...));
}

template<size_t size, typename T, typename... Ts>
consteval auto adjust_for_first_element()
{
    constexpr auto max = max_alignment(T{}, Ts{}...);
    constexpr auto mod = size % max;
    constexpr auto adjust_for_alignment = mod != 0 ? max - mod :  0ul;
    
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
        _size = adjust_for_first_element<tmp_size, Members...>();
        // fill buffer of offsets
        fill_displs_buffer<0>(origin, members...);
    }

    size_t pack(std::byte* dest, Origin* base, size_t offset) const
    {
        constexpr size_t alignment = 0;
        return copy<alignment, 0, true>(dest, reinterpret_cast<std::byte*>(base), offset, Members{}...);
    }

    size_t unpack(Origin* base, std::byte* src, size_t offset) const
    {
        constexpr size_t alignment = 0;
        return copy<alignment, 0, false>(reinterpret_cast<std::byte*>(base), src, offset, Members{}...);
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

    template<size_t Alignment, int I, bool Pack, typename T>
    constexpr size_t copy(std::byte* dest, std::byte* src, size_t offset, T const& head) const
    {
        // check if offset matches alignment of head
        constexpr auto mod = Alignment % alignof(T);
        constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;
        
        offset += adjust_for_alignment;
        
        if constexpr (Pack)
            std::memcpy(dest + offset, src + _displacements[I], sizeof(T));
        else
            std::memcpy(dest + _displacements[I], src + offset, sizeof(T));

        return offset + sizeof(T);
    }
    
    template<size_t Alignment, int I, bool Pack, typename T, typename... Ts>
    constexpr size_t copy(std::byte* dest, std::byte* src, size_t offset, T const& head, Ts const&... tail) const
    {
        // check if offset matches alignment of head
        constexpr auto mod = Alignment % alignof(T);
        constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;
        
        offset += adjust_for_alignment;
        
        if constexpr (Pack)
            std::memcpy(dest + offset, src + _displacements[I], sizeof(T));
        else
            std::memcpy(dest + _displacements[I], src + offset, sizeof(T));

        offset += sizeof(T);

        return copy<Alignment + adjust_for_alignment + sizeof(T), I+1, Pack>(dest, src, offset, tail...);
    }

    std::array<std::ptrdiff_t, sizeof...(Members)> _displacements;
    size_t _size {0};
};

#endif
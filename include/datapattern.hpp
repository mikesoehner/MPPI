#ifndef MPPI_DATAPATTERN_HPP
#define MPPI_DATAPATTERN_HPP

#include <experimental/meta>
#include <ranges>
#include <concepts>
#include <type_traits>
#include <cstddef>
#include <cstring>


template<int Index, typename OriginalType, typename Layout>
consteval void get_sizes_impl(Layout& sizes, std::string_view identifier)
{
    for (std::meta::info member : nonstatic_data_members_of(^^OriginalType))
        if (identifier_of(member) == identifier)
            sizes[Index] = size_of(member);
}

template <typename OriginalType, typename Layout, int... IDs, typename... Identifiers>
consteval auto get_sizes(Layout& sizes, std::integer_sequence<int, IDs...> id_seq, Identifiers... identifiers) 
{
    (get_sizes_impl<IDs, OriginalType>(sizes, identifiers), ...);
}

template <typename OriginalType, typename... Identifiers>
consteval auto get_sizes_wrapper(Identifiers... identifiers) 
{
    constexpr auto N = sizeof...(Identifiers);
    constexpr auto ids = std::make_integer_sequence<int, N>{};
    std::array<size_t, N> arr;

    get_sizes<OriginalType>(arr, ids, identifiers...);

    return arr;
}

template<int Index, typename OriginalType, typename Layout>
consteval void get_alignment_impl(Layout& alignment, std::string_view identifier)
{
    for (std::meta::info member : nonstatic_data_members_of(^^OriginalType))
        if (identifier_of(member) == identifier)
            alignment[Index] = alignment_of(member);
}

template <typename OriginalType, typename Layout, int... IDs, typename... Identifiers>
consteval auto get_alignment(Layout& alignment, std::integer_sequence<int, IDs...> id_seq, Identifiers... identifiers) 
{
    (get_alignment_impl<IDs, OriginalType>(alignment, identifiers), ...);
}

template <typename OriginalType, typename... Identifiers>
consteval auto get_alignment_wrapper(Identifiers... identifiers) 
{
    constexpr auto N = sizeof...(Identifiers);
    constexpr auto ids = std::make_integer_sequence<int, N>{};
    std::array<size_t, N> arr;

    get_alignment<OriginalType>(arr, ids, identifiers...);

    return arr;
}

template<int Index, typename OriginalType, typename Layout>
consteval void calc_offset_type_impl(Layout& offset_type, std::string_view identifier)
{
    for (std::meta::info member : nonstatic_data_members_of(^^OriginalType))
        if (identifier_of(member) == identifier)
            offset_type[Index] = offset_of(member).bytes;
}

template <typename OriginalType, typename Layout, int... Indices, typename... Identifiers>
consteval auto calc_offset_type_wrapper(Layout& offset_type, std::integer_sequence<int, Indices...> id_seq, Identifiers... identifiers) 
{
    (calc_offset_type_impl<Indices, OriginalType>(offset_type, identifiers), ...);
}

template <typename OriginalType, typename... Identifiers>
consteval auto calc_offset_type(Identifiers... identifiers) 
{
    constexpr auto N = sizeof...(Identifiers);
    constexpr auto Indices = std::make_integer_sequence<int, N>{};
    std::array<size_t, N> offset_type;

    calc_offset_type_wrapper<OriginalType>(offset_type, Indices, identifiers...);

    return offset_type;
}

template<int Index, typename OriginalType, typename Layout>
consteval void calc_offset_buffer_impl(Layout& offset_buffer, Layout& sizes, std::string_view identifier)
{
    if constexpr (Index == 0)
    {
        offset_buffer[0] = 0;
    }
    else
    {
        size_t alignment = 0;

        for (std::meta::info member : nonstatic_data_members_of(^^OriginalType))
            if (identifier_of(member) == identifier)
                alignment = alignment_of(member);

        auto prev_type_size = sizes[Index-1];
        auto mod = (offset_buffer[Index-1] + prev_type_size) % alignment;
        auto adjust_for_alignment = mod != 0 ? alignment - mod : 0ul;

        offset_buffer[Index] = offset_buffer[Index-1] + prev_type_size + adjust_for_alignment;
    }
}

template <typename OriginalType, typename Layout, int... Indices, typename... Identifiers>
consteval auto calc_offset_buffer_wrapper(Layout& offset_buffer, Layout& sizes, std::integer_sequence<int, Indices...> id_seq, Identifiers... identifiers) 
{
    (get_sizes_impl<Indices, OriginalType>(sizes, identifiers), ...);
    (calc_offset_buffer_impl<Indices, OriginalType>(offset_buffer, sizes, identifiers), ...);
}

template <typename OriginalType, typename... Identifiers>
consteval auto calc_offset_buffer(Identifiers... identifiers) 
{
    constexpr auto N = sizeof...(Identifiers);
    constexpr auto Indices = std::make_integer_sequence<int, N>{};
    std::array<size_t, N> offset_buffer;
    std::array<size_t, N> sizes;

    calc_offset_buffer_wrapper<OriginalType>(offset_buffer, sizes, Indices, identifiers...);

    return offset_buffer;
}


template<typename OriginalType, typename... Identifiers>
consteval auto calc_size(Identifiers... identifiers)
{
    constexpr auto N = sizeof...(Identifiers);
    std::array<size_t, N> sizes = get_sizes_wrapper<OriginalType>(identifiers...);
    std::array<size_t, N> alignments = get_alignment_wrapper<OriginalType>(identifiers...);

    size_t offset = 0;

    for (size_t i = 0; i < N; i++)
    {
        auto mod = offset % alignments[i];
        auto adjust_for_alignment = mod != 0 ? alignments[i] - mod :  0ul;

        offset += sizes[i] + adjust_for_alignment;
    }

    // adjust for alignment of first element
    auto mod = offset % alignments[0];
    auto adjust_for_alignment = mod != 0 ? alignments[0] - mod :  0ul;

    return offset + adjust_for_alignment;
}

template<typename OriginalType, typename... Members>
class DataPattern
{
public:
    consteval DataPattern(OriginalType origin, Members... members)
        : _size(calc_size<OriginalType>(members...)), _sizes(get_sizes_wrapper<OriginalType>(members...)),
        _offset_type(calc_offset_type<OriginalType>(members...)),
        _offset_buffer(calc_offset_buffer<OriginalType>(members...))
    {}

    size_t store_from_type(std::byte* dest, OriginalType* base, size_t offset) const
    {
        constexpr auto N = sizeof...(Members);
        constexpr auto Indices = std::make_index_sequence<N>{};

        return store_to_dest(dest, reinterpret_cast<std::byte*>(base), offset, Indices);
    }

    size_t load_to_type(OriginalType* base, std::byte* src, size_t offset) const
    {
        constexpr auto N = sizeof...(Members);
        constexpr auto Indices = std::make_index_sequence<N>{};

        return load_from_src(reinterpret_cast<std::byte*>(base), src, offset, Indices);
    }

    constexpr auto get_size() const { return _size; }

private:

    template<size_t... Indices>
    constexpr size_t store_to_dest(std::byte* dest, std::byte* origin, size_t offset, std::index_sequence<Indices...> indices) const
    {
        ((std::memcpy(dest + offset + _offset_buffer[Indices], origin + _offset_type[Indices], _sizes[Indices])), ...);

        return offset + _size;
    }

    template<size_t... Indices>
    constexpr size_t load_from_src(std::byte* origin, std::byte* src, size_t offset, std::index_sequence<Indices...> indices) const
    {
        ((std::memcpy(origin + _offset_type[Indices], src + offset + _offset_buffer[Indices], _sizes[Indices])), ...);

        return offset + _size;
    }

    const size_t _size {0};
    const std::array<size_t, sizeof...(Members)> _sizes;
    const std::array<size_t, sizeof...(Members)> _offset_type;
    const std::array<size_t, sizeof...(Members)> _offset_buffer;
};

#endif
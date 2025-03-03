#ifndef MPPI_PARAMETER_PACK_HELPER_H
#define MPPI_PARAMETER_PACK_HELPER_H

#include <cstring>
#include <ranges>
#include <algorithm>


// base case
template<typename R>
constexpr size_t range_size(R& head)
{
    return std::ranges::distance(head);
}
// specialization case
template<typename R, typename... Rs>
constexpr size_t range_size(R& head, Rs&... tail)
{
    return std::ranges::distance(head) + range_size(tail...);
}


// base case
template<typename R>
constexpr size_t ranges_size(R head)
{
    return std::ranges::distance(head);
}
// specialization case
template<typename R, typename... Rs>
constexpr size_t ranges_size(R head, Rs... tail)
{
    return std::ranges::distance(head) + ranges_size(tail...);
}
// free helper function
template<typename... Rs>
size_t free_ranges_size(Rs... ranges)
{
    return ranges_size(ranges...);
}

// getting underlying type out of ranges, so we can create a vector
template<typename R, typename... Rs>
constexpr auto get_first_underlying_type()
{
    return (typename std::ranges::range_value_t<R>){};
}


// base case
template<typename Iter, typename V>
constexpr void copy_view(Iter iter, V head)
{
    auto range_size = std::ranges::distance(head);
    auto iter_end = iter + range_size;

    std::ranges::copy(iter, iter_end, head.begin());
}
// specialization case
template<typename Iter, typename V, typename... Vs>
constexpr void copy_view(Iter iter, V head, Vs... tail)
{
    auto range_size = std::ranges::distance(head);
    auto iter_end = iter + range_size;

    std::ranges::copy(iter, iter_end, head.begin());

    iter = std::move(iter_end);
    copy_view(iter, tail...);
}
// free helper function
template<typename C, typename... Vs>
void free_copy_view(C& result_data, Vs... ranges)
{
    copy_view(result_data.begin(), ranges...);
}

#endif
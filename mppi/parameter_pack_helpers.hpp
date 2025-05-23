#ifndef MPPI_PARAMETER_PACK_HELPER_HPP
#define MPPI_PARAMETER_PACK_HELPER_HPP

#include <cstring>
#include <ranges>
#include <algorithm>


namespace mppi
{
    // sum length of ranges
    template<typename... Rs>
    constexpr size_t ranges_size(Rs&... ranges)
    {
        return (std::ranges::distance(ranges) + ...);
    }
    
    // getting underlying type out of ranges, so we can create a vector
    template<typename R, typename... Rs>
    consteval auto get_first_underlying_type()
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
};

#endif
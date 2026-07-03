#ifndef MPPI_PARAMETER_PACK_HELPER_HPP
#define MPPI_PARAMETER_PACK_HELPER_HPP

#include <cstring>
#include <ranges>
#include <algorithm>


namespace mppi
{
    // getting underlying type out of ranges, so we can create a vector
    template<typename R, typename... Rs>
    constexpr auto get_first_underlying_type()
    {
        return (typename std::ranges::range_value_t<R>){};
    }
};

#endif
#ifndef MPPI_CUSTOM_CONCEPTS_H
#define MPPI_CUSTOM_CONCEPTS_H

#include <type_traits>
#include <ranges>
#include <concepts>

// concept checks for pmr memory resource (etc. monotonic_buffer_resource, unsynchronized_pool_resource, ...)
template<typename T>
concept is_polymorphic_memory_resource = (std::derived_from<T, std::pmr::memory_resource>);

// checks if all types in the pack are fundamental types
template<typename... Ts>
concept are_fundamentals = (std::is_fundamental_v<Ts> && ...);

// checks if all types in the pack are input ranges
template<typename... Rs>
concept are_input_ranges = (std::ranges::input_range <Rs> && ...);

// checks if the input data needs to be copied (necessary for multiple ranges and mutiple fundamentals)
template<typename... Ts>
concept must_be_copied = ( sizeof...(Ts) > 1 );

// if there is only one range or one fundamental we can just reference it
template<typename... Ts>
concept can_be_refed = ( sizeof...(Ts) == 1 );

// the following two concepts are used to differentiate between ranges and views
template<typename... Vs>
concept are_only_views = ((std::ranges::range<Vs> && std::movable<Vs> && std::ranges::enable_view<Vs>) && ...);

template<typename... Rs>
concept are_only_ranges = ((std::ranges::range<Rs> && std::movable<Rs> && !std::ranges::enable_view<Rs>) && ...);


// checks if all types in the pack are ranges with the same underlying type
template<typename T, typename... Ts>
constexpr bool free_are_same_types(T head, Ts...tail)
{
    return (std::conjunction_v<std::is_same<std::ranges::range_value_t<T>, std::ranges::range_value_t<Ts>>...>);
}

template<typename... Ts>
concept are_same_types = (free_are_same_types(Ts{}...));

#endif
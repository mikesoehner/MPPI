#ifndef MPPI_CUSTOM_CONCEPTS_H
#define MPPI_CUSTOM_CONCEPTS_H

#include <type_traits>
#include <ranges>
#include <concepts>
#include "parameter_pack_helpers.hpp"


namespace mppi
{
    // concept checks for pmr memory resource (etc. monotonic_buffer_resource, unsynchronized_pool_resource, ...)
    template<typename T>
    concept is_polymorphic_memory_resource = (std::derived_from<T, std::pmr::memory_resource>);
    
    // checks if all types in the pack are fundamental types
    template<typename... Ts>
    concept are_fundamentals = (std::is_fundamental_v<Ts> && ...);
    
    // checks if all types in the pack are input ranges
    template<typename... Rs>
    concept are_input_ranges = (std::ranges::input_range<Rs> && ...);
    
    // checks if the input data needs to be copied (necessary for multiple ranges and mutiple fundamentals)
    template<typename... Ts>
    concept must_be_copied = ( sizeof...(Ts) > 1 );
    
    // if there is only one range or one fundamental we can just reference it
    template<typename... Ts>
    concept can_be_refed = ( sizeof...(Ts) == 1 );
    
    // the following two concepts are used to differentiate between ranges and views
    // the following two concepts are used to differentiate between ranges and views
    template<typename V>
    concept is_only_view = (std::ranges::range<V> && std::movable<V> && std::ranges::enable_view<V>);

    template<typename... Vs>
    concept are_only_views = ((std::ranges::range<Vs> && std::movable<Vs> && std::ranges::enable_view<Vs>) && ...);
    
    template<typename R>
    concept is_only_range = (std::ranges::range<R> && std::movable<R> && !std::ranges::enable_view<R>);
    
    template<typename... Rs>
    concept are_only_ranges = ((std::ranges::range<Rs> && std::movable<Rs> && !std::ranges::enable_view<Rs>) && ...);
    
     // checks if the underlying type of all ranges is the same
    template<typename T, typename... Ts>
    constexpr bool all_types_are_same = std::conjunction_v<std::is_same<
                                                std::ranges::range_value_t<T>, 
                                                std::ranges::range_value_t<Ts>>...>;
    
    template<typename... Ts>
    concept are_same_types = (all_types_are_same<Ts...>);
    
    // check if underlying type has a standard layout
    template<typename... Ts>
    concept has_underlying_standard_layout = std::is_standard_layout_v<decltype(get_first_underlying_type<Ts...>())>;
    
    // check if underlying type is trivially copyable
    template<typename Range>
    concept is_value_type_trivially_copyable = 
        requires { typename std::ranges::range_value_t<Range>; } && 
        std::is_trivially_copyable_v< typename std::ranges::range_value_t<Range> >;
    
    template<typename... Ranges>
    concept are_value_types_trivially_copyable = (is_value_type_trivially_copyable<Ranges> && ...);

    template<typename Container>
    concept is_not_nested_container = requires(Container container)
    {
        { container.size() } -> std::same_as<size_t>;
    };

    // test
    template<typename T>
    concept contains_pattern = requires(T t)
    {
        typename T::Pattern_Type;
    };

    template<typename... Ts>
    concept contain_patterns = (contains_pattern<Ts> && ...);


    template<typename T, typename U>
    consteval bool same_pattern_wrapper()
    {
        return std::is_same_v<typename T::Pattern_Type, typename U::Pattern_Type>;
    };

    template<typename T, typename U, typename... Ts>
        requires (sizeof...(Ts) >= 1)
    consteval bool same_pattern_wrapper()
    {
        return std::is_same_v<typename T::Pattern_Type, typename U::Pattern_Type> 
                && same_pattern_wrapper<T, Ts...>();
    };

    template<typename T, typename... Ts>
    consteval bool same_pattern()
    {
        // if there is only one pattern, it is the same as all others
        if constexpr (sizeof...(Ts) == 0)
            return true;
        else
            return same_pattern_wrapper<T, Ts...>();
    };

    template<typename... Ts>
    concept contain_same_patterns = contain_patterns<Ts...> && same_pattern<Ts...>();
};

#endif
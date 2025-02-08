#ifndef MPPI_PARAMETER_PACK_HELPER_H
#define MPPI_PARAMETER_PACK_HELPER_H


#include <cstring>
#include <ranges>



// put values of variadic template into array of bytes
// overload
template<typename U>
constexpr void fill_buffer(std::byte* buffer, size_t offset, U head) 
{
    // check if offset matches alignment of head
    if (auto mod = offset % alignof(U) != 0)
        offset += alignof(U) - mod;
    
    std::memcpy(buffer + offset, &head, sizeof(head));
}
// base case
template<typename U, typename... Us>
constexpr void fill_buffer(std::byte* buffer, size_t offset, U head, Us... tail)
{
    // check if offset matches alignment of head
    if (auto mod = offset % alignof(U) != 0)
        offset += alignof(U) - mod;
    
    std::memcpy(buffer + offset, &head, sizeof(head));
    offset += sizeof(head);
    fill_buffer(buffer, offset, tail...);
}
// helper function
template<typename... Us>
constexpr void free_fill_buffer(std::byte* buffer, Us... args)
{
    size_t offset = 0;
    fill_buffer(buffer, offset, args...);
}

// calculate arraysize of alignment of types
// overload
template<typename U>
constexpr std::size_t array_size(size_t offset, U head)
{
    // check if offset matches alignment of head
    if (auto mod = offset % alignof(U) != 0)
        offset += alignof(U) - mod;

    offset += sizeof(head);
    return offset;
}
// base case
template<typename U, typename... Us>
constexpr std::size_t array_size(size_t offset, U head, Us... tail)
{
    // check if offset matches alignment of head
    if (auto mod = offset % alignof(U) != 0)
        offset += alignof(U) - mod;
    
    offset += sizeof(head);
    return array_size(offset, tail...);
}
// helper function
template<typename... Us>
constexpr std::size_t free_array_size(Us... args) 
{
    std::size_t offset = 0;
    return array_size(offset, args...);
}


// base case
template<typename C, typename R>
constexpr void fill_range(C& container, R head)
{
    std::ranges::copy(head, std::back_inserter(container));
}
// specialization case
template<typename C, typename R, typename... Rs>
constexpr void fill_range(C& container, R head, Rs... tail)
{
    std::ranges::copy(head, std::back_inserter(container));

    fill_range(container, tail...);
}
// free helper function
template<typename C, typename... Rs>
void free_fill_range(C& container, Rs... ranges)
{
    fill_range(container, ranges...);
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
constexpr auto get_first_type()
{
    // return (typename T::range_value_t){};
    return (typename std::ranges::range_value_t<R>){};
}

template<typename... Rs>
constexpr auto free_get_types()
{
    return get_first_type<Rs...>();
}

#endif
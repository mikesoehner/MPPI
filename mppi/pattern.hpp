#ifndef MPPI_DATAPATTERN_HPP
#define MPPI_DATAPATTERN_HPP

#include <experimental/meta>
#include <ranges>
#include <concepts>
#include <type_traits>
#include <cstddef>
#include <cstring>

#include "stringliteral.hpp"
#include "reflection_helpers.hpp" 
#include "custom_concepts.hpp"

namespace mppi 
{
    template<typename OriginalType, StringLiteral... Identifiers>
    class Pattern
    {
    public:
        // Make Pattern construction compile-time executable and check for pointers
        consteval Pattern()
            requires is_without_pointers<OriginalType, Identifiers...>
        = default;
    
        size_t pack(std::byte* dest, OriginalType const* base, size_t offset) const
        {
            constexpr auto N = sizeof...(Identifiers);
            constexpr auto Indices = std::make_index_sequence<N>{};

            // Target Members are compile-time resolvable
            if constexpr (has_only_trivially_copyable_types<OriginalType, Identifiers...>)
                return copy<true>(dest, reinterpret_cast<std::byte const*>(base), offset, Indices);
            // Target Members are not all compile-time resolvable
            else
                return copy_dynamic<true>(dest, reinterpret_cast<std::byte const*>(base), offset, Indices);
        }

        size_t unpack(OriginalType* base, std::byte const* src, size_t offset) const
        {
            constexpr auto N = sizeof...(Identifiers);
            constexpr auto Indices = std::make_index_sequence<N>{};
    
            // Target Members are compile-time resolvable
            if constexpr (has_only_trivially_copyable_types<OriginalType, Identifiers...>)
                return copy<false>(reinterpret_cast<std::byte*>(base), src, offset, Indices);
            // Target Members are not all compile-time resolvable
            else
                return copy_dynamic<false>(reinterpret_cast<std::byte*>(base), src, offset, Indices);
        }
    
        // Return the size of one packed element. This size does not exist for types with allocator-aware containers,
        // as the size of one packed element varies
        constexpr auto get_packed_size() const
            requires has_only_trivially_copyable_types<OriginalType, Identifiers...>
        { return calc_packed_size<OriginalType, Identifiers...>(); }

        constexpr auto get_packed_size() const
            requires (!has_only_trivially_copyable_types<OriginalType, Identifiers...>)
        { return 0; }

        // Return the size of the combined packed ranges
        template<are_input_ranges... Rs>
            requires has_only_trivially_copyable_types<OriginalType, Identifiers...> && (sizeof...(Rs) != 0)
        inline auto get_packed_ranges_size(Rs... ranges) const 
        {
            return ranges_size(ranges...) * calc_packed_size<OriginalType, Identifiers...>();
        }

        // Return the size of the combined packed ranges (for elements with allocator-aware containers)
        template<are_input_ranges... Rs>
            requires (!has_only_trivially_copyable_types<OriginalType, Identifiers...>) && (sizeof...(Rs) != 0)
        inline auto get_packed_ranges_size(Rs... ranges) const 
        {
            return calc_packed_size_allocator_aware_container<OriginalType, Identifiers...>(ranges...);
        }


        // functions checks if a pattern is trivial, i.e. does contain all data members of a type anyway
        consteval static auto is_trivial_pattern()
        {
            constexpr auto N = are_valid_identifiers<Identifiers...>() ? sizeof...(Identifiers) : get_total_nb_members<OriginalType>();
            constexpr auto indices = std::make_index_sequence<N>{};

            // check if an identifier occurs twice
            constexpr bool no_duplicates = (are_no_duplicates<Identifiers...>(indices));
            // check if there are as many identifiers, as data members (if there are wrong identifiers, compilation will fail earlier)
            constexpr bool same_size = N == get_total_nb_members<OriginalType>();

            // if both are true a pattern of a type contains all data members of that type
            return no_duplicates && same_size;
        }


        using base_type = OriginalType;
    
    private:
    
        template<bool Pack, size_t... Indices>
        constexpr size_t copy(std::byte* dest, std::byte const* src, size_t offset, std::index_sequence<Indices...>) const
        {
            constexpr auto offset_buffer = calc_offsets_in_buffer<OriginalType, Identifiers...>();
            constexpr auto offset_type = get_offsets<OriginalType, Identifiers...>();
            constexpr auto sizes = get_sizes<OriginalType, Identifiers...>();

            if constexpr (Pack)
                ((std::memcpy(dest + offset + offset_buffer[Indices], src + offset_type[Indices], sizes[Indices])), ...);
            else
                ((std::memcpy(dest + offset_type[Indices], src + offset + offset_buffer[Indices], sizes[Indices])), ...);
    
            // Cannot use get_packed_size() function here
            return offset + calc_packed_size<OriginalType, Identifiers...>();
        }
    
    
        template<bool Pack, size_t Index>
        auto copy_dynamic_impl(std::byte* dest, std::byte const* src, size_t& offset) const
        {
            constexpr auto offset_type = get_offsets<OriginalType, Identifiers...>();
            constexpr auto sizes = get_sizes<OriginalType, Identifiers...>();
            constexpr auto types = get_types<OriginalType, Identifiers...>();
    
            using T = typename [: types[Index] :];
    
            // this requires runtime information
            if constexpr (is_allocator_aware_container<T>)
            {
                using Value_Type = typename T::value_type;
    
                static_assert(std::is_trivially_copyable_v<Value_Type>);
    
                // we have to do code duplication here because of weird ternary operator behaviour
                if constexpr (Pack)
                { 
                    auto container_ptr = reinterpret_cast<T const*>(src + offset_type[Index]);
                    
                    auto size = container_ptr->size();
                    auto begin = container_ptr->begin();
                    auto end = container_ptr->end();
                    
                    auto mod = offset % alignof(Value_Type);
                    auto adjust_for_alignment = mod != 0 ? alignof(Value_Type) - mod : 0ul;
                    offset += adjust_for_alignment;
    
                    std::byte* destination = dest + offset;
    
                    std::copy(begin, end, reinterpret_cast<Value_Type*>(destination));

                    offset += size * sizeof(Value_Type);
                }
                else
                {
                    auto container_ptr = reinterpret_cast<T*>(dest + offset_type[Index]);
                    
                    auto size = container_ptr->size();
                    auto begin = container_ptr->begin();
                    auto end = container_ptr->end();
                    
                    auto mod = offset % alignof(Value_Type);
                    auto adjust_for_alignment = mod != 0 ? alignof(Value_Type) - mod : 0ul;
                    offset += adjust_for_alignment;

                    std::byte const* start = src + offset;
                    std::byte const* stop = src + offset + size * sizeof(Value_Type);
    
                    std::copy(reinterpret_cast<Value_Type const*>(start), reinterpret_cast<Value_Type const*>(stop), begin);

                    offset += size * sizeof(Value_Type);
                }
            }
            // this not
            else
            {
                auto mod = offset % alignof(T);
                auto adjust_for_alignment = mod != 0 ? alignof(T) - mod : 0ul;
                offset += adjust_for_alignment;
    
                if constexpr (Pack)
                    std::memcpy(dest + offset, src + offset_type[Index], sizes[Index]);
                else
                    std::memcpy(dest + offset_type[Index], src + offset, sizes[Index]);
    
                offset += sizes[Index];
            }
    
            return offset;
        }
    
    
        template<bool Pack, size_t... Indices>
        constexpr size_t copy_dynamic(std::byte* dest, std::byte const* src, size_t offset, std::index_sequence<Indices...>) const
        {
            // the comma operator return the last instance of copy, 
            // since in copy offset is passed by reference it is the same offset in functions called here
            return (copy_dynamic_impl<Pack, Indices>(dest, src, offset), ...);
        }
    };



    
    template<std::ranges::input_range R, typename P> requires std::ranges::view<R>
    class Pattern_View : public std::ranges::view_interface<Pattern_View<R, P>>
    {
    private:
        R                                         base_ {};
        P                                         pattern_;
        std::ranges::iterator_t<R>                iter_start_ {std::begin(base_)};
        std::ranges::iterator_t<R>                iter_end_ {std::end(base_)};
    public:
        Pattern_View() = default;
        
        constexpr Pattern_View(R base, P pattern)
            : base_(base)
            , pattern_(pattern)
            , iter_start_(std::begin(base_))
            , iter_end_(std::end(base_))
        {}
        
        constexpr R base() const &
        {return base_;}
        constexpr R base() && 
        {return std::move(base_);}
        
        constexpr auto begin() const
        {return iter_start_;}
        constexpr auto end() const
        {return iter_end_;}
        
        constexpr auto size() const requires std::ranges::sized_range<const R>
        { 
            return std::ranges::size(base_);
        }

        constexpr auto pack(std::byte* dest, void const* base, size_t offset) const
        {
            return pattern_.pack(dest, base, offset);
        }
        
        constexpr auto get_pattern() const
        {
            return pattern_;
        }

        using Pattern_Type = P;
        // using value_type = typename R::iter_value_t;
        using value_type = std::ranges::range_value_t<R>;
    };
 
    template<class R, typename P>
    Pattern_View(R&& base, P pattern)
        -> Pattern_View<std::ranges::views::all_t<R>, P>;


    namespace details
    {
        template<typename P>
        struct Pattern_View_Adaptor_Closure
        {
            P pattern_;
            constexpr Pattern_View_Adaptor_Closure(P pattern): pattern_(pattern)
            {}
    
            template <std::ranges::viewable_range R>
            constexpr auto operator()(R && r) const
            {
                return Pattern_View(std::forward<R>(r), pattern_);
            }

            using Pattern_Type = P;
        } ;
    
        struct Pattern_View_Adaptor
        {
            template<std::ranges::viewable_range R, typename P>
            constexpr auto operator () (R && r, P pattern)
            {
                return Pattern_View( std::forward<R>(r), pattern) ;
            }
    
            template<typename P>
            constexpr auto operator () (P pattern)
            {
                return Pattern_View_Adaptor_Closure(pattern);
            }
        };
    
        template <std::ranges::viewable_range R, typename P>
        constexpr auto operator | (R&& r, Pattern_View_Adaptor_Closure<P> const & a)
        {
            return a(std::forward<R>(r));
        }
    }

    details::Pattern_View_Adaptor pattern_view;
};


#endif
#ifndef MPPI_DATAPATTERN_HPP
#define MPPI_DATAPATTERN_HPP

#include <ranges>
#include <concepts>
#include <type_traits>
#include <cstddef>
#include <cstring>


namespace mppi
{
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
    class Pattern
    {
    public:
        Pattern(Origin* origin, Members&... members)
        {
            // determine size of one packed element
            constexpr size_t offset = 0;
            constexpr auto tmp_size = calc_size<offset>(Members{}...);
            // adjust for alignment of first element again
            _size = adjust_for_first_element<tmp_size, Members...>();
            // fill buffer of offsets
            fill_displs_buffer<0>(origin, members...);
        }
    
        size_t pack(std::byte* dest, Origin const* base, size_t offset) const
        {
            constexpr size_t alignment = 0;
            return copy<alignment, 0, true>(dest, reinterpret_cast<std::byte const*>(base), offset, Members{}...);
        }
    
        size_t unpack(Origin* base, std::byte const* src, size_t offset) const
        {
            constexpr size_t alignment = 0;
            return copy<alignment, 0, false>(reinterpret_cast<std::byte*>(base), src, offset, Members{}...);
        }
    
        auto get_size() const { return _size; }
        using base_type = Origin;
    
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
        constexpr size_t copy(std::byte* dest, std::byte const* src, size_t offset, T const& head) const
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
        constexpr size_t copy(std::byte* dest, std::byte const* src, size_t offset, T const& head, Ts const&... tail) const
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




    template<std::ranges::input_range R, typename... PatternArgs> 
        requires std::ranges::view<R>
    class Pattern_View : public std::ranges::view_interface<Pattern_View<R, PatternArgs...>>
    {
    private:
        R                                         base_ {};
        Pattern<PatternArgs...>                   pattern_;
        std::ranges::iterator_t<R>                iter_start_ {std::begin(base_)};
        std::ranges::iterator_t<R>                iter_end_ {std::end(base_)};
    public:
        Pattern_View() = default;
        
        constexpr Pattern_View(R base, Pattern<PatternArgs...> pattern)
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

        using Pattern_Type = Pattern<PatternArgs...>;
        // using value_type = typename R::iter_value_t;
        using value_type = std::ranges::range_value_t<R>;
    };
 
    template<class R, typename... PatternArgs>
    Pattern_View(R&& base, Pattern<PatternArgs...> pattern)
        -> Pattern_View<std::ranges::views::all_t<R>, PatternArgs...>;


    namespace details
    {
        template<typename... PatternArgs>
        struct Pattern_View_Adaptor_Closure
        {
            Pattern<PatternArgs...> pattern_;
            constexpr Pattern_View_Adaptor_Closure(Pattern<PatternArgs...> pattern): pattern_(pattern)
            {}

            template <std::ranges::viewable_range R>
            constexpr auto operator()(R && r) const
            {
                return Pattern_View(std::forward<R>(r), pattern_);
            }

            using Pattern_Type = Pattern<PatternArgs...>;
        } ;
    
        struct Pattern_View_Adaptor
        {
            template<std::ranges::viewable_range R, typename... PatternArgs>
            constexpr auto operator () (R && r, Pattern<PatternArgs...> pattern)
            {
                return Pattern_View( std::forward<R>(r), pattern) ;
            }
    
            template<typename... PatternArgs>
            constexpr auto operator () (Pattern<PatternArgs...> pattern)
            {
                return Pattern_View_Adaptor_Closure(pattern);
            }
        };
    
        template <std::ranges::viewable_range R, typename... PatternArgs>
        constexpr auto operator | (R&& r, Pattern_View_Adaptor_Closure<PatternArgs...> const & a)
        {
            return a(std::forward<R>(r));
        }
    }

    details::Pattern_View_Adaptor pattern_view;
};


#endif
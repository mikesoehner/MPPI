#ifndef MPPI_DATAPATTERN_HPP
#define MPPI_DATAPATTERN_HPP

#ifdef USE_HIP
    #include <hip/hip_runtime.h>
#endif

#include <ranges>
#include <concepts>
#include <type_traits>
#include <cstddef>
#include <cstring>

#include "kernel.hpp"



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

    template<typename... Members>
    consteval auto calc_pattern_size()
    {
        constexpr size_t offset = 0;
        constexpr auto tmp_size = calc_size<offset>(Members{}...);
        return adjust_for_first_element<tmp_size, Members...>();
    }

    template<typename Origin, typename... Members>
    class Pattern;


    template<typename T>
    consteval int segments_of_type()
    {
        return (sizeof(T)+(segment_size-1)) / segment_size;
    }
    
    template<typename Origin, typename... Members>
    consteval int free_calc_nb_segments(Pattern<Origin, Members...>)
    { 
        return ((segments_of_type<Members>()) + ...); 
    }
    
    template<typename... Members>
    consteval int free_calc_nb_segments() 
    {
        return ((segments_of_type<Members>()) + ...); 
    }




    template<int I, int Partitions, typename Array, typename Current>
        requires (static_cast<int>(sizeof(Current)) - Partitions*segment_size <= segment_size)
    consteval auto new_index()
    {
        return I+1;
    }

    template<int I, int Partitions, typename Array, typename Current>        
        requires (static_cast<int>(sizeof(Current)) - Partitions*segment_size > segment_size)
    consteval auto new_index()
    {
        return new_index<I+1, Partitions+1, Array, Current>();
    }


    template<int I, int Partitions, typename Array, typename Current>
        requires (static_cast<int>(sizeof(Current)) - Partitions*segment_size <= segment_size)
    consteval auto fill_segment(Array arr)
    {
        arr[I] = static_cast<int>(sizeof(Current)) - Partitions*segment_size;
        return arr;
    }

    template<int I, int Partitions, typename Array, typename Current>        
        requires (static_cast<int>(sizeof(Current)) - Partitions*segment_size > segment_size)
    consteval auto fill_segment(Array arr)
    {
        arr[I] = segment_size;
        return fill_segment<I+1, Partitions+1, Array, Current>(arr);
    }


    template<int I, typename Array, typename Current, typename... Members>
        requires (sizeof...(Members) == 0)
    consteval auto fill_arr(Array arr)
    {
        return fill_segment<I, 0, Array, Current>(arr);
    }

    template<int I, typename Array, typename Current, typename... Members>
        requires (sizeof...(Members) > 0)
    consteval auto fill_arr(Array arr)
    {
        constexpr int J = new_index<I, 0, Array, Current>();

        return fill_arr<J, Array, Members...>(fill_segment<I, 0, Array, Current>(arr));
    }

    template<typename Origin, typename... Members>
    consteval auto get_segment_sizes()
    {
        constexpr std::array<int, free_calc_nb_segments<Members...>()> arr{};

        return fill_arr<0, std::array<int, free_calc_nb_segments<Members...>()>, Members...>(arr);
    }


    template<int Past_Segments, typename Origin, typename Current>
    inline int seg_to_memb(int i, Origin& origin, Current& curr)
    {
        const int nth_segment_of_type = i - Past_Segments;
        constexpr int segments_per_type = segments_of_type<Current>();
        constexpr int nb_bytes_copy = std::min(segment_size, (int) sizeof(Current));

        return ((int) ((char*) &curr - (char*) &origin) + (int) sizeof(Current)) - 
                ((segments_per_type - nth_segment_of_type) * nb_bytes_copy);
    };

    template<int Past_Segments, typename Origin, typename Current, typename... Members>
    inline int seg_to_memb(int i, Origin& origin, Current& curr, Members&... members)
    {
        const int nth_segment_of_type = i - Past_Segments;
        constexpr int segments_per_type = segments_of_type<Current>();
        constexpr int nb_bytes_copy = std::min(segment_size, (int) sizeof(Current));

        return (i * segment_size - (int) sizeof(Current) < (int) ((char*) &curr - (char*) &origin))
                ? ((int) ((char*) &curr - (char*) &origin) + (int) sizeof(Current)) - 
                ((segments_per_type - nth_segment_of_type) * nb_bytes_copy)
                : seg_to_memb<Past_Segments + ((int) sizeof(Current) / segment_size)>(i, origin, members...);
    };


    
    
    template<typename Origin, typename... Members>
    class Pattern
    {
    public:
        Pattern() = default;
        Pattern(Origin* origin, Members&... members)
        {
            // determine size of one packed element
            // constexpr size_t offset = 0;
            // constexpr auto tmp_size = calc_size<offset>(Members{}...);
            // // adjust for alignment of first element again
            // _size = adjust_for_first_element<tmp_size, Members...>();
            // fill buffer of offsets
            fill_displs_buffer<0>(origin, members...);

            std::array<int, free_calc_nb_segments<Members...>()> segment_offsets;
            for (auto i = 0; i < segment_offsets.size(); i++)
            {
                segment_offsets[i] = seg_to_memb<0>(i, *origin, members...);
                // std::cout << "Segment Offsets: " << i << ": " << _segment_offsets[i] << std::endl;
            }

            constexpr auto segment_sizes = get_segment_sizes<Origin, Members...>();

            gpu_malloc(reinterpret_cast<void**>(&_segment_offsets), free_calc_nb_segments<Members...>() * sizeof(int));
            gpu_malloc(reinterpret_cast<void**>(&_segment_sizes), free_calc_nb_segments<Members...>() * sizeof(int));

            gpu_copyH2D(_segment_offsets, segment_offsets.data(), segment_offsets.size() * sizeof(int));
            gpu_copyH2D(_segment_sizes, segment_sizes.data(), segment_sizes.size() * sizeof(int));
        }

        ~Pattern()
        {
            gpu_free(_segment_offsets);
            gpu_free(_segment_sizes);
        }
    
        size_t pack(std::byte* dest, Origin const* base, size_t offset) const
        {
            constexpr size_t alignment = 0;
            return copy<alignment, 0, true>(dest, reinterpret_cast<std::byte const*>(base), offset, Members{}...);
        }
    
        template<typename Range>
        auto pack_APU(std::byte* dest, Range const& src) const
        {
            // return copy_APU<true>(dest, base, segment);

            int const nb_segments = calc_nb_segments();
            int const total_nb_segments = nb_segments * src.size();
            int constexpr nb_threads = 768;
            int blocksize = (src.size()*nb_segments + nb_threads-1) / nb_threads;
            blocksize = std::min(blocksize, 300);

            gpu_pack_interface(blocksize, nb_segments, (nb_threads+nb_segments-1)/nb_segments,
                dest, reinterpret_cast<std::byte const*>(src.data()), 
                _segment_offsets, _segment_sizes, total_nb_segments, get_size(), sizeof(typename Range::value_type));
        }

    
        size_t unpack(Origin* base, std::byte const* src, size_t offset) const
        {
            constexpr size_t alignment = 0;
            return copy<alignment, 0, false>(reinterpret_cast<std::byte*>(base), src, offset, Members{}...);
        }

        template<typename Range>
        auto unpack_APU(Range& dest, std::byte const* src) const
        {
            // return copy_APU<false>(base, src, segment);

            int const nb_segments = calc_nb_segments();
            int const total_nb_segments = nb_segments * dest.size();
            int constexpr nb_threads = 768;
            int blocksize = (dest.size()*nb_segments + nb_threads-1) / nb_threads;
            
            blocksize = std::min(blocksize, 300);

            gpu_unpack_interface(blocksize, nb_segments, (nb_threads+nb_segments-1)/nb_segments, 
                 reinterpret_cast<std::byte*>(dest.data()), src, _segment_offsets, _segment_sizes, total_nb_segments, get_size(), sizeof(typename Range::value_type));
        }
    
        auto get_size() const { return calc_pattern_size<Members...>(); }

        constexpr int calc_nb_segments() const { return ((segments_of_type<Members>()) + ...); }

        constexpr auto get_offset_array() const 
        {
            return _displacements;
        }

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
        inline size_t copy(std::byte* dest, std::byte const* src, size_t offset, T const& head) const
        {
            // check if offset matches alignment of head
            constexpr auto mod = Alignment % alignof(T);
            constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;
            
            offset += adjust_for_alignment;
            
            if constexpr (Pack)
                memcpy(dest + offset, src + _displacements[I], sizeof(T));
                // reinterpret_cast<T*>(dest + offset)[0] = reinterpret_cast<const T*>(src + _displacements[I])[0];
            else
                memcpy(dest + _displacements[I], src + offset, sizeof(T));
                // reinterpret_cast<T*>(dest + _displacements[I])[0] = reinterpret_cast<const T*>(src + offset)[0];
    
            return offset + sizeof(T);
        }
        
        template<size_t Alignment, int I, bool Pack, typename T, typename... Ts>
        inline size_t copy(std::byte* dest, std::byte const* src, size_t offset, T const& head, Ts const&... tail) const
        {
            // check if offset matches alignment of head
            constexpr auto mod = Alignment % alignof(T);
            constexpr auto adjust_for_alignment = mod != 0 ? alignof(T) - mod :  0ul;
            
            offset += adjust_for_alignment;
            
            if constexpr (Pack)
                memcpy(dest + offset, src + _displacements[I], sizeof(T));
                // reinterpret_cast<T*>(dest + offset)[0] = reinterpret_cast<const T*>(src + _displacements[I])[0];
            else
                memcpy(dest + _displacements[I], src + offset, sizeof(T));
                // reinterpret_cast<T*>(dest + _displacements[I])[0] = reinterpret_cast<const T*>(src + offset)[0];
    
            offset += sizeof(T);
    
            return copy<Alignment + adjust_for_alignment + sizeof(T), I+1, Pack>(dest, src, offset, tail...);
        }

    
        std::array<std::ptrdiff_t, sizeof...(Members)> _displacements;
        // size_t _size {0};
        // std::array<int, free_calc_nb_segments<Members...>()> _segment_offsets;
        int* _segment_offsets = nullptr;
        int* _segment_sizes = nullptr;
    };


    






    template<int I, int Index, int Head>
    consteval int search()
    {
        return Head;
    }

    template<int I, int Index, int Head, int... Tail>
        requires (sizeof...(Tail) > 0)
    consteval int search()
    {
        if (I == Index)
            return Head;

        return search<I, Index+1, Tail...>();
    }


    template<int... Args>
    class Sizes 
    {
    public:
        template<int I>
        static constexpr int get() { return search<I, 0, Args...>(); }
        static constexpr int total_size() { return ((Args) * ...); }
        template<int I>
        static constexpr int dim_stride() { return calc_stride<I+1>(); }

        template<typename T, int Start, int NbCopy, typename Subsize>
        static void copy(std::byte* dst, std::byte const* src) 
        {
            int offset = 0;
            do_copy<T, 0, NbCopy, Subsize>(dst, src + Start*sizeof(T), offset, src);
        }

        template<typename T, int Start, int NbCopy, typename Subsize>
        static void uncopy(std::byte* dst, std::byte const* src) 
        {
            int offset = 0;
            do_uncopy<T, 0, NbCopy, Subsize>(dst+ Start*sizeof(T), src, offset);
        }

    private:
        template<int I>
        static constexpr int calc_stride()
        {
            return search<I, 0, Args...>() * calc_stride<I+1>();
        }
        
        template<int I>
            requires (I == sizeof...(Args)-1)
        static constexpr int calc_stride()
        {
            return search<I, 0, Args...>();
        }
        

        template<typename T, int I, int NbCopy, typename Subsize>
        static void do_copy(std::byte* dst, std::byte const* src, int& offset, std::byte const* org_src)
        {
            constexpr int stride = dim_stride<I>();
            // std::cout << "Stride: " << stride << std::endl;

            for(auto i = 0; i < Subsize::template get<I>(); i++)
            {
                do_copy<T, I+1, NbCopy, Subsize>(dst, src + i*stride*sizeof(T), offset, org_src);
            }
        }

        template<typename T, int I, int NbCopy, typename Subsize>
            requires (I == sizeof...(Args) - 1)
        static void do_copy(std::byte* dst, std::byte const* src, int& offset, std::byte const* org_src)
        {
            // std::cout << (src - org_src) << std::endl;
            std::memcpy(dst + offset*sizeof(T), src, NbCopy*sizeof(T));
            offset += NbCopy;
        }


        template<typename T, int I, int NbCopy, typename Subsize>
        static void do_uncopy(std::byte* dst, std::byte const* src, int& offset)
        {
            constexpr int stride = dim_stride<I>();

            for(auto i = 0; i < Subsize::template get<I>(); i++)
            {
                do_uncopy<T, I+1, NbCopy, Subsize>(dst + i*stride*sizeof(T), src, offset);
            }
        }

        template<typename T, int I, int NbCopy, typename Subsize>
            requires (I == sizeof...(Args) - 1)
        static void do_uncopy(std::byte* dst, std::byte const* src, int& offset)
        {
            std::memcpy(dst, src + offset*sizeof(T), NbCopy*sizeof(T));
            offset += NbCopy;
        }
    };

    template<int... Args>
    class Subsizes 
    {
    public:
        template<int I>
        static constexpr int get() { return search<I, 0, Args...>(); }
        static constexpr int nb_strides() { return ((Args) * ...) / search<0,0,Args...>(); }
        static constexpr int total_subsize() { return ((Args) * ...); }
        static constexpr int consec_nb_copy() { return search<sizeof...(Args)-1, 0, Args...>(); }
    };

    template<int... Args>
    class Starts 
    {
    public:
        template<int I>
        static constexpr int get() { return search<I, 0, Args...>(); }
        
        template<typename Size>
        static constexpr int start()
        {
            constexpr auto end = sizeof...(Args)-1;
            constexpr auto dim_size = Size::template get<end>();
            return search<end, 0, Args...>() + calc_start<end-1, dim_size, Size>();
        }

    private:
        template<int I, int Prev, typename Size>
        static constexpr int calc_start()
        {
            constexpr auto dim_size = Size::template get<I>();
            constexpr auto start_point = search<I, 0, Args...>();

            return start_point*Prev + calc_start<I-1, Prev*dim_size, Size>();
        }

        template<int I, int Prev, typename Size>
            requires (I == 0)
        static constexpr int calc_start()
        {
            constexpr auto start_point = search<I, 0, Args...>();
            return start_point*Prev;
        }
    };

    template<typename T, typename Size, typename Subsize, typename Start>
        requires (std::is_fundamental_v<T>)
    class Pattern<T, Size, Subsize, Start>
    {
    public:
        Pattern() = default;

        inline size_t pack(std::byte* dest, void const* base, size_t offset) const 
        {
            int constexpr starts = Start::template start<Size>();
            // std::cout << "Start: " << starts << std::endl;
            int constexpr consec_nb_copy = Subsize::consec_nb_copy();
            // std::cout << "consec_nb_copy: " << consec_nb_copy << std::endl;

            Size::template copy<T, starts, consec_nb_copy, Subsize>(dest, reinterpret_cast<std::byte const*>(base));
            return Size::total_size();
        }
        template<typename Range>
        inline auto pack_APU(std::byte* dest, Range const& src) const {}

        inline size_t unpack(void* base, std::byte const* src, size_t offset) const 
        {
            int constexpr starts = Start::template start<Size>();
            int constexpr consec_nb_copy = Subsize::consec_nb_copy();

            Size::template uncopy<T, starts, consec_nb_copy, Subsize>(reinterpret_cast<std::byte*>(base), src);
            return Size::total_size();
        }
        template<typename Range>
        inline auto unpack_APU(Range& dest, std::byte const* src) const {}

        inline auto get_size() const { return (static_cast<double>(Subsize::total_subsize()) / static_cast<double>(Size::total_size()) + 1.0) * sizeof(T); }

        constexpr int calc_nb_segments() const { return 0; }

        constexpr auto get_offset_array() const 
        {
            // return _displacements;
        }
    private:    
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
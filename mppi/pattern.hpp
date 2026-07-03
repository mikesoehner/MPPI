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



    // Logic to calculate gpu-used segment offsets
    template<int I, int Partitions, typename Array, typename Current>
        requires (static_cast<int>(sizeof(Current)) - Partitions*gpu_kernel::segment_size <= gpu_kernel::segment_size)
    consteval auto new_index()
    {
        return I+1;
    }

    template<int I, int Partitions, typename Array, typename Current>        
        requires (static_cast<int>(sizeof(Current)) - Partitions*gpu_kernel::segment_size > gpu_kernel::segment_size)
    consteval auto new_index()
    {
        return new_index<I+1, Partitions+1, Array, Current>();
    }

    // Logic to calculate gpu-used segment offsets
    template<int I, int Partitions, typename Array, typename Current>
        requires (static_cast<int>(sizeof(Current)) - Partitions*gpu_kernel::segment_size <= gpu_kernel::segment_size)
    consteval auto fill_segment(Array arr)
    {
        arr[I] = static_cast<int>(sizeof(Current)) - Partitions*gpu_kernel::segment_size;
        return arr;
    }

    template<int I, int Partitions, typename Array, typename Current>        
        requires (static_cast<int>(sizeof(Current)) - Partitions*gpu_kernel::segment_size > gpu_kernel::segment_size)
    consteval auto fill_segment(Array arr)
    {
        arr[I] = gpu_kernel::segment_size;
        return fill_segment<I+1, Partitions+1, Array, Current>(arr);
    }

    // Logic to calculate gpu-used segment offsets
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


    template<typename T>
    consteval int segments_of_type()
    {
        return (sizeof(T)+(gpu_kernel::segment_size-1)) / gpu_kernel::segment_size;
    }

    
    // Main function responsible for calculating gpu-used segment offsets
    template<int Past_Segments, typename Origin, typename Current>
    inline int seg_to_memb(int i, Origin& origin, Current& curr)
    {
        const int nth_segment_of_type = i - Past_Segments;
        constexpr int segments_per_type = segments_of_type<Current>();
        constexpr int nb_bytes_copy = std::min(gpu_kernel::segment_size, (int) sizeof(Current));

        return ((int) ((char*) &curr - (char*) &origin) + (int) sizeof(Current)) - 
                ((segments_per_type - nth_segment_of_type) * nb_bytes_copy);
    };

    template<int Past_Segments, typename Origin, typename Current, typename... Members>
    inline int seg_to_memb(int i, Origin& origin, Current& curr, Members&... members)
    {
        const int nth_segment_of_type = i - Past_Segments;
        constexpr int segments_per_type = segments_of_type<Current>();
        constexpr int nb_bytes_copy = std::min(gpu_kernel::segment_size, (int) sizeof(Current));

        return (i * gpu_kernel::segment_size - (int) sizeof(Current) < (int) ((char*) &curr - (char*) &origin))
                ? ((int) ((char*) &curr - (char*) &origin) + (int) sizeof(Current)) - 
                ((segments_per_type - nth_segment_of_type) * nb_bytes_copy)
                : seg_to_memb<Past_Segments + ((int) sizeof(Current) / gpu_kernel::segment_size)>(i, origin, members...);
    };


    // Struct that helps with extracting the Members from a Pattern.
    template<typename T>
    struct pattern_wrapper_struct;

    template<typename Origin, typename... Members>
    struct pattern_wrapper_struct<Pattern<Origin, Members...>>
    {
        static consteval int calc_nb_segments_impl()
        {
            return ((segments_of_type<Members>()) + ...);
        }

        static consteval auto get_segment_sizes_impl()
        {
            constexpr auto nb_segments = calc_nb_segments_impl();
            constexpr std::array<int, nb_segments> arr{};

            return fill_arr<0, std::array<int, nb_segments>, Members...>(arr);
        }

        static consteval auto divisible_by_4_impl()
        {
            return ((sizeof(Members) % 4 == 0) && ...);
        }
    };

    template<typename T>
    consteval int free_calc_nb_segments()
    {
        return pattern_wrapper_struct<T>::calc_nb_segments_impl();
    }

    template<typename T>
    consteval auto get_segment_sizes_impl()
    {
        return pattern_wrapper_struct<T>::get_segment_sizes_impl();
    }

    template<typename T>
    consteval bool divisible_by_4()
    {
        return pattern_wrapper_struct<T>::divisible_by_4_impl();
    }


    
    
    template<typename Origin, typename... Members>
    class Pattern
    {
    public:
        Pattern(Origin* origin, Members&... members)
        {
            // Fill buffer of offsets
            fill_displs_buffer<0>(origin, members...);

            // Fill gpu-used segment offsets. It is suboptimal to have to this in the constructor
            // even when there is no GPU involved, though this solution as Reflection would
            // take it out of the constructor.
            for (auto i = 0; i < _segment_offsets.size(); i++)
                _segment_offsets[i] = seg_to_memb<0>(i, *origin, members...);
        }

        // Implementation of the packing functionality.
        inline size_t pack_impl(std::byte* dst, Origin const* base, size_t offset) const
        {
            constexpr size_t alignment = 0;
            return copy<alignment, 0, true>(dst, reinterpret_cast<std::byte const*>(base), offset, Members{}...);
        }

        // Interface of the packing functionality, which allows each pattern to treat packing differently.
        template<typename V>
        inline size_t pack(std::byte* dst, V const view, size_t offset) const
        {
            constexpr size_t alignment = 0;
            return copy<alignment, 0, true>(dest, reinterpret_cast<std::byte const*>(base), offset, Members{}...);
        }
    
        // Packing done on the GPU.
        template<typename Range>
        inline auto pack_GPU(std::byte* dst, Range const& src, int const* segment_offsets, int const* segment_sizes) const
        {
            int const nb_segments = calc_nb_segments();
            int const total_nb_segments = nb_segments * src.size();
            int constexpr nb_threads = 768;
            int blocksize = (total_nb_segments + nb_threads-1) / nb_threads;
            blocksize = std::min(blocksize, 300);

            // check for possible optimization
            if constexpr (divisible_by_4<Pattern<Origin, Members...>>())
                gpu_kernel::gpu_pack_4_interface(blocksize, nb_segments, (nb_threads+nb_segments-1)/nb_segments,
                    dst, reinterpret_cast<std::byte const*>(src.data()), 
                    segment_offsets, total_nb_segments, get_size(), sizeof(typename Range::value_type));
            else
                gpu_kernel::gpu_pack_interface(blocksize, nb_segments, (nb_threads+nb_segments-1)/nb_segments,
                    dst, reinterpret_cast<std::byte const*>(src.data()), 
                    segment_offsets, segment_sizes, total_nb_segments, get_size(), sizeof(typename Range::value_type));
        }

        // Same as for pack_impl.
        inline size_t unpack_impl(Origin* base, std::byte const* src, size_t offset) const
        {
            constexpr size_t alignment = 0;
            return copy<alignment, 0, false>(reinterpret_cast<std::byte*>(base), src, offset, Members{}...);
        }

        // Same as for pack.
        template<typename V>
        inline size_t unpack(V view, std::byte const* dst, size_t offset) const
        {
            for (auto& element : view)
                offset = unpack_impl(&element, dst, offset);
            
            return offset; 
        }

        // Same as for pack_GPU.
        template<typename Range>
        inline auto unpack_GPU(Range& dst, std::byte const* src, int const* segment_offsets, int const* segment_sizes) const
        {
            int const nb_segments = calc_nb_segments();
            int const total_nb_segments = nb_segments * dst.size();
            int constexpr nb_threads = 768;
            int blocksize = (total_nb_segments + nb_threads-1) / nb_threads;
            blocksize = std::min(blocksize, 300);

            // check for possible optimization
            if constexpr (divisible_by_4<Pattern<Origin, Members...>>())
                gpu_kernel::gpu_unpack_4_interface(blocksize, nb_segments, (nb_threads+nb_segments-1)/nb_segments, 
                    reinterpret_cast<std::byte*>(dst.data()), src, 
                    segment_offsets, total_nb_segments, get_size(), sizeof(typename Range::value_type));
            else
                gpu_kernel::gpu_unpack_interface(blocksize, nb_segments, (nb_threads+nb_segments-1)/nb_segments, 
                    reinterpret_cast<std::byte*>(dst.data()), src, 
                    segment_offsets, segment_sizes, total_nb_segments, get_size(), sizeof(typename Range::value_type));
        }
    
        inline auto get_size() const { return calc_pattern_size<Members...>(); }

        // Segments are used in the gpu packing.
        constexpr int calc_nb_segments() const { return ((segments_of_type<Members>()) + ...); }

        auto get_segment_sizes() const
        {
            return get_segment_sizes_impl<Pattern<Origin, Members...>>();
        }

        auto get_segment_offsets() const
        {
            return _segment_offsets;
        }

        template<typename... Ts>
        inline auto get_buffer_size(Ts... views) const
        {
            return ((get_size() * views.size()) + ...);
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
                std::memcpy(dst + offset, src + _displacements[I], sizeof(T));
            else
                std::memcpy(dst + _displacements[I], src + offset, sizeof(T));
    
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
                std::memcpy(dst + offset, src + _displacements[I], sizeof(T));
            else
                std::memcpy(dst + _displacements[I], src + offset, sizeof(T));
    
            offset += sizeof(T);
    
            return copy<Alignment + adjust_for_alignment + sizeof(T), I+1, Pack>(dst, src, offset, tail...);
        }

        // Data held by this Pattern, which should become unnecessary once Reflection is used.
        std::array<std::ptrdiff_t, sizeof...(Members)> _displacements;
        std::array<int, free_calc_nb_segments<Pattern<Origin, Members...>>()> _segment_offsets;
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
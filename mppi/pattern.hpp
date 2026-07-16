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

#include "custom_concepts.hpp"
#include <gpu_kernels/kernel.hpp>



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
            for (auto const& element : view)
                offset = pack_impl(dst, &element, offset);
            
            return offset; 
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
        inline size_t copy(std::byte* dst, std::byte const* src, size_t offset, T const& head) const
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
        inline size_t copy(std::byte* dst, std::byte const* src, size_t offset, T const& head, Ts const&... tail) const
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


    





    // Free function used by Subarray Pattern.
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

    // Helper class for Subarray Pattern
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
            do_copy<T, 0, NbCopy, Subsize>(dst, src + Start*sizeof(T), offset);
        }

        template<typename T, int Start, typename Subsize, typename Arr>
        static auto get_offsets_arr(int dst, int src, Arr& arr) 
        {
            int offset = 0;
            get_offsets_arr_impl<T, 0, Subsize>(dst, src + Start, offset, arr);
        }

        template<typename T, int Start, typename Subsize, typename Arr>
        static auto get_sizes_arr(int dst, int src, Arr& arr) 
        {
            int offset = 0;
            get_sizes_arr_impl<T, 0, Subsize>(dst, src + Start, offset, arr);
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
        static void do_copy(std::byte* dst, std::byte const* src, int& offset)
        {
            constexpr int stride = dim_stride<I>();
            // std::cout << "Stride: " << stride << std::endl;

            for(auto i = 0; i < Subsize::template get<I>(); i++)
            {
                do_copy<T, I+1, NbCopy, Subsize>(dst, src + i*stride*sizeof(T), offset);
            }
        }

        template<typename T, int I, int NbCopy, typename Subsize>
            requires (I == sizeof...(Args) - 1)
        static void do_copy(std::byte* dst, std::byte const* src, int& offset)
        {
            std::memcpy(dst + offset*sizeof(T), src, NbCopy*sizeof(T));
            offset += NbCopy;
        }
        

        template<typename T, int I, typename Subsize, typename Arr>
        static void get_offsets_arr_impl(int dst, int src, int& offset, Arr& arr)
        {
            constexpr int stride = dim_stride<I>();

            for(auto i = 0; i < Subsize::template get<I>(); i++)
            {
                get_offsets_arr_impl<T, I+1, Subsize>(dst, src + i*stride, offset, arr);
            }
        }

        template<typename T, int I, typename Subsize, typename Arr>
            requires (I == sizeof...(Args) - 1)
        static void get_offsets_arr_impl(int dst, int src, int& offset, Arr& arr)
        {
            auto constexpr nb_elements = Subsize::template get<I>();
            auto constexpr divisible = sizeof(T) / gpu_kernel::segment_size;
            auto constexpr modulo = sizeof(T) % gpu_kernel::segment_size;
            auto constexpr mod_correction = modulo > 0 ? 1 : 0;

            auto index = dst + offset;

            for (auto i = 0; i < nb_elements; i++)
            {
                for (auto j = 0; j < divisible; j++)
                {
                    arr[index++] = (src + i) * sizeof(T) + j * gpu_kernel::segment_size;
                }
                if constexpr (mod_correction)
                    arr[index++] = (src + i) * sizeof(T) + divisible * gpu_kernel::segment_size;
            }

            offset += nb_elements * (divisible + mod_correction);
        }
        

        template<typename T, int I, typename Subsize, typename Arr>
        static void get_sizes_arr_impl(int dst, int src, int& offset, Arr& arr)
        {
            constexpr int stride = dim_stride<I>();

            for(auto i = 0; i < Subsize::template get<I>(); i++)
            {
                get_sizes_arr_impl<T, I+1, Subsize>(dst, src + i*stride, offset, arr);
            }
        }

        template<typename T, int I, typename Subsize, typename Arr>
            requires (I == sizeof...(Args) - 1)
        static void get_sizes_arr_impl(int dst, int src, int& offset, Arr& arr)
        {
            auto constexpr nb_elements = Subsize::template get<I>();
            auto constexpr divisible = sizeof(T) / gpu_kernel::segment_size;
            auto constexpr modulo = sizeof(T) % gpu_kernel::segment_size;
            auto constexpr mod_correction = modulo > 0 ? 1 : 0;

            auto index = dst + offset;

            for (auto i = 0; i < nb_elements; i++)
            {
                for (auto j = 0; j < divisible; j++)
                {
                    arr[index++] = gpu_kernel::segment_size;
                }
                if constexpr (mod_correction)
                    arr[index++] = modulo;
            }

            offset += nb_elements * (divisible + mod_correction);
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

    // Helper class for Subarray Pattern
    template<int... Args>
    class Subsizes 
    {
    public:
        template<int I>
        static constexpr int get() { return search<I, 0, Args...>(); }
        static constexpr int nb_strides() { return ((Args) * ...) / search<0,0,Args...>(); }
        static constexpr int total_subsize() { return ((Args) * ...); }
        static constexpr int consec_nb_copy() { return search<sizeof...(Args)-1, 0, Args...>(); }
        static constexpr int nb_dims() { return sizeof...(Args); }
    };

    // Helper class for Subarray Pattern
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

    // This function is only necessary in the non-Reflection version.
    template<typename T, typename Subsize>
    consteval auto free_calc_nb_segments_subarray()
    {
        auto constexpr divisible = sizeof(T) / gpu_kernel::segment_size;
        auto constexpr modulo = sizeof(T) % gpu_kernel::segment_size;

        auto constexpr mod_correction = modulo > 0 ? 1 : 0;
    
        return Subsize::total_subsize() * (divisible + mod_correction);
    }



    // Pattern that uses MPI's subarray interface.
    template<typename T, is_sizes Size, is_subsizes Subsize, is_starts Start>
        requires are_same_nb_args<Size, Subsize, Start>
    class Pattern<T, Size, Subsize, Start>
    {
    public:
        inline size_t pack_impl(std::byte* dst, void const* base, size_t offset) const 
        {
            int constexpr starts = Start::template start<Size>();
            int constexpr consec_nb_copy = Subsize::consec_nb_copy();

            Size::template copy<T, starts, consec_nb_copy, Subsize>(dst, reinterpret_cast<std::byte const*>(base));
            return Size::total_size();
        }

        // TODO: Maybe harden this function, so only consecutive memory containers can use it
        template<typename V>
        inline size_t pack(std::byte* dst, V const view, size_t offset) const
        {
            offset = pack_impl(dst, view.data(), offset);
            
            return offset; 
        }

        template<typename Range>
        inline auto pack_GPU(std::byte* dst, Range const& src, int const* segment_offsets, int const* segment_sizes) const 
        {
            int const nb_segments = calc_nb_segments();
            int const total_nb_segments = nb_segments;
            int constexpr nb_threads = 768;
            int blocksize = (total_nb_segments + nb_threads-1) / nb_threads;
            blocksize = std::min(blocksize, 300);

            // check for possible optimization
            if constexpr (sizeof(T) % 4 == 0)
                gpu_kernel::gpu_pack_4_interface(blocksize, nb_segments, (nb_threads+nb_segments-1)/nb_segments,
                    dst, reinterpret_cast<std::byte const*>(src.data()),
                    segment_offsets, total_nb_segments, get_size(), sizeof(typename Range::value_type));
            else
                gpu_kernel::gpu_pack_interface(blocksize, nb_segments, (nb_threads+nb_segments-1)/nb_segments,
                    dst, reinterpret_cast<std::byte const*>(src.data()),
                    segment_offsets, segment_sizes, total_nb_segments, get_size(), sizeof(typename Range::value_type));
        }


        inline size_t unpack_impl(void* base, std::byte const* src, size_t offset) const 
        {
            int constexpr starts = Start::template start<Size>();
            int constexpr consec_nb_copy = Subsize::consec_nb_copy();

            Size::template uncopy<T, starts, consec_nb_copy, Subsize>(reinterpret_cast<std::byte*>(base), src);
            return Size::total_size();
        }

        // TODO: Maybe harden this function, so only consecutive memory containers can use it
        template<typename V>
        inline size_t unpack(V view, std::byte const* src, size_t offset) const
        {
            offset = unpack_impl(view.data(), src, offset);
            
            return offset; 
        }

        template<typename Range>
        inline auto unpack_GPU(Range& dst, std::byte const* src, int const* segment_offsets, int const* segment_sizes) const
        {
            int const nb_segments = calc_nb_segments();
            int const total_nb_segments = nb_segments;
            int constexpr nb_threads = 768;
            int blocksize = (total_nb_segments + nb_threads-1) / nb_threads;
            blocksize = std::min(blocksize, 300);

            // check for possible optimization
            if constexpr (sizeof(T) % 4 == 0)
                gpu_kernel::gpu_unpack_4_interface(blocksize, nb_segments, (nb_threads+nb_segments-1)/nb_segments, 
                    reinterpret_cast<std::byte*>(dst.data()), src, 
                    segment_offsets, total_nb_segments, get_size(), sizeof(typename Range::value_type));
            else
                gpu_kernel::gpu_unpack_interface(blocksize, nb_segments, (nb_threads+nb_segments-1)/nb_segments, 
                    reinterpret_cast<std::byte*>(dst.data()), src, 
                    segment_offsets, segment_sizes, total_nb_segments, get_size(), sizeof(typename Range::value_type));
        }

        inline auto get_size() const
        {
            return sizeof(T);
        }

        constexpr int calc_nb_segments() const 
        {
            auto constexpr divisible = sizeof(T) / gpu_kernel::segment_size;
            auto constexpr modulo = sizeof(T) % gpu_kernel::segment_size;

            auto constexpr mod_correction = modulo > 0 ? 1 : 0;
        
            return Subsize::total_subsize() * (divisible + mod_correction);
        }

        constexpr auto get_segment_sizes() const
        {
            auto constexpr nb_segments = free_calc_nb_segments_subarray<T, Subsize>();
            std::array<int, nb_segments> arr;
            int constexpr starts = Start::template start<Size>();

            Size::template get_sizes_arr<T, starts, Subsize, decltype(arr)>(0, 0, arr);

            return arr;
        }

        constexpr auto get_segment_offsets() const
        {
            auto constexpr nb_segments = free_calc_nb_segments_subarray<T, Subsize>();
            std::array<int, nb_segments> arr;

            int constexpr starts = Start::template start<Size>();

            Size::template get_offsets_arr<T, starts, Subsize, decltype(arr)>(0, 0, arr);

            return arr;
        }

        template<typename... Ts>
        inline auto get_buffer_size(Ts...) const
        {
            return Subsize::total_subsize() * sizeof(T);
        }
    private:    
    };


    // Range adapter class for usage of Patterns.
    template<std::ranges::view View, typename... PatternArgs>
    class PatternView : public std::ranges::view_interface<PatternView<View, PatternArgs...>>
    {
    public:
        PatternView(View view, Pattern<PatternArgs...> pattern)
            : view_(std::move(view)), pattern_(std::move(pattern))
        {}

        constexpr auto begin() const
        {
            return std::begin(view_);
        }
        
        constexpr auto end() const
        {
            return std::end(view_);
        }
        
        constexpr auto get_pattern() const
        {
            return pattern_;
        }

        using Pattern_Type = Pattern<PatternArgs...>;
        using value_type = std::ranges::range_value_t<View>;

    private:
        View view_;
        Pattern<PatternArgs...> pattern_;
    };

    template<typename... PatternArgs>
    class Pattern_View_Adaptor
    {
    public:
        explicit Pattern_View_Adaptor(Pattern<PatternArgs...> pattern)
            : pattern_(std::move(pattern))
        {}

        template<std::ranges::viewable_range R>
        auto operator()(R&& r) const 
        {
            return PatternView(
                std::views::all(std::forward<R>(r)), pattern_);
        }

    private:
        Pattern<PatternArgs...> pattern_;
    };

    template<typename... PatternArgs>
    auto pattern_view(Pattern<PatternArgs...> pattern) 
    {
        return Pattern_View_Adaptor<PatternArgs...>(std::move(pattern));
    }

    template<std::ranges::viewable_range R, typename... PatternArgs>
    auto operator| (R&& r, const Pattern_View_Adaptor<PatternArgs...>& adaptor) 
    {
        return adaptor(std::forward<R>(r));
    }
};


#endif
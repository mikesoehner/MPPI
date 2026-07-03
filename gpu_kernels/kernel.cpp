#include "kernel.hpp"

namespace gpu_kernel
{
    #if defined USE_CUDA || defined USE_HIP
    #define GLOBAL __global__
    #else
    #define GLOBAL
    #endif

    GLOBAL void gpu_pack(std::byte* out_buffer, std::byte const* in_range, 
                                int const* segment_offsets, int const* segment_sizes, 
                                size_t size, int pattern_size, int range_type_size)
    {
    #if defined USE_CUDA || defined USE_HIP
        int const segment = threadIdx.x;
        int const element = threadIdx.y;
        int const nb_threads_per_block = blockDim.x*blockDim.y;

        int const global_segment = segment + element*blockDim.x + blockIdx.x*nb_threads_per_block;

        for (int i = global_segment; i < size; i += nb_threads_per_block*gridDim.x)
        {
            int const global_element = i / blockDim.x;
        
            int src_offset = global_element * range_type_size;
            int dst_offset = global_element * pattern_size;

            // TODO: The way that the destination location is calculated might lead to alignment issues,
            // if chunks larger than 4 bytes are copied.
            memcpy(out_buffer + dst_offset + segment*segment_size,
                    in_range + src_offset + segment_offsets[segment], 
                    segment_sizes[segment]);
        }
    #endif
    }


    GLOBAL void gpu_unpack(std::byte* out_range, std::byte const* in_buffer, 
                                int const* segment_offsets, int const* segment_sizes, 
                                size_t size, int pattern_size, int range_type_size)
    {
    #if defined USE_CUDA || defined USE_HIP
        int const segment = threadIdx.x;
        int const element = threadIdx.y;
        int const nb_threads_per_block = blockDim.x*blockDim.y;

        int const global_segment = segment + element*blockDim.x + blockIdx.x*nb_threads_per_block;

        for (int i = global_segment; i < size; i += nb_threads_per_block*gridDim.x)
        {
            int const element_id = i / blockDim.x;

            int src_offset = element_id * pattern_size;
            int dst_offset = element_id * range_type_size;

            memcpy(out_range + dst_offset + segment_offsets[segment],
                in_buffer + src_offset + segment*segment_size,
                segment_sizes[segment]);
        }
    #endif
    }


    void gpu_pack_interface(int blocksize, int nb_segments, int nb_threadblocks,
                                std::byte* out_buffer, std::byte const* in_range, 
                                int const* segment_offsets, int const* segment_sizes, 
                                size_t size, int pattern_size, int range_type_size)
    {
    #if defined USE_CUDA || defined USE_HIP
        gpu_pack<<<blocksize, dim3(nb_segments, nb_threadblocks, 1)>>>
            (out_buffer, in_range, segment_offsets, segment_sizes, size, pattern_size, range_type_size);
    #endif
    }

    void gpu_unpack_interface(int blocksize, int nb_segments, int nb_threadblocks,
                                std::byte* out_range, std::byte const* in_buffer,
                                int const* segment_offsets, int const* segment_sizes,
                                size_t size, int pattern_size, int range_type_size)
    {
    #if defined USE_CUDA || defined USE_HIP
        gpu_unpack<<<blocksize, dim3(nb_segments, nb_threadblocks, 1)>>>
            (out_range, in_buffer, segment_offsets, segment_sizes, size, pattern_size, range_type_size);
    #endif
    }



    // gpu packing kernel optimized for pattern that are divisible by 4 bytes
    GLOBAL void gpu_pack_4(std::byte* out_buffer, std::byte const* in_range,
                                int const* segment_offsets,
                                size_t size, int pattern_size, int range_type_size)
    {
    #if defined USE_CUDA || defined USE_HIP
        int const segment = threadIdx.x;
        int const element = threadIdx.y;
        int const nb_threads_per_block = blockDim.x*blockDim.y;

        int const global_segment = segment + element*blockDim.x + blockIdx.x*nb_threads_per_block;

        for (int i = global_segment; i < size; i += nb_threads_per_block*gridDim.x)
        {
            int const global_element = i / blockDim.x;
        
            int src_offset = global_element * range_type_size;
            int dst_offset = global_element * pattern_size;
            
            auto* ptr_out = reinterpret_cast<int*>(out_buffer + dst_offset + segment*segment_size);
            auto const* ptr_in = reinterpret_cast<int const*>(in_range + src_offset + segment_offsets[segment]);

            ptr_out[0] = ptr_in[0];
        }
    #endif
    }

    // gpu unpacking kernel optimized for pattern that are divisible by 4 bytes
    GLOBAL void gpu_unpack_4(std::byte* out_range, std::byte const* in_buffer,
                                int const* segment_offsets,
                                size_t size, int pattern_size, int range_type_size)
    {
    #if defined USE_CUDA || defined USE_HIP
        int const segment = threadIdx.x;
        int const element = threadIdx.y;
        int const nb_threads_per_block = blockDim.x*blockDim.y;

        int const global_segment = segment + element*blockDim.x + blockIdx.x*nb_threads_per_block;

        for (int i = global_segment; i < size; i += nb_threads_per_block*gridDim.x)
        {
            int const element_id = i / blockDim.x;

            int src_offset = element_id * pattern_size;
            int dst_offset = element_id * range_type_size;
                
            auto* ptr_out = reinterpret_cast<int*>(out_range + dst_offset + segment_offsets[segment]);
            auto const* ptr_in = reinterpret_cast<int const*>(in_buffer + src_offset + segment*segment_size);

            ptr_out[0] = ptr_in[0];
        }
    #endif
    }


    void gpu_pack_4_interface(int blocksize, int nb_segments, int nb_threadblocks,
                                std::byte* out_buffer, std::byte const* in_range, 
                                int const* segment_offsets,
                                size_t size, int pattern_size, int range_type_size)
    {
    #if defined USE_CUDA || defined USE_HIP
        gpu_pack_4<<<blocksize, dim3(nb_segments, nb_threadblocks, 1)>>>
            (out_buffer, in_range, segment_offsets, size, pattern_size, range_type_size);
    #endif
    }

    void gpu_unpack_4_interface(int blocksize, int nb_segments, int nb_threadblocks,
                                std::byte* out_range, std::byte const* in_buffer,
                                int const* segment_offsets,
                                size_t size, int pattern_size, int range_type_size)
    {
    #if defined USE_CUDA || defined USE_HIP
        gpu_unpack_4<<<blocksize, dim3(nb_segments, nb_threadblocks, 1)>>>
            (out_range, in_buffer, segment_offsets, size, pattern_size, range_type_size);
    #endif
    }
}
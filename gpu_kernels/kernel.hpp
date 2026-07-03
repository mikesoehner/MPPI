#ifndef MPPI_GPUKERNEL_KERNEL_H
#define MPPI_GPUKERNEL_KERNEL_H

#include <cstddef>

#ifdef USE_HIP
#include <hip/hip_runtime.h>
#endif


#include "utility_functions.hpp"


namespace gpu_kernel
{
    constexpr int segment_size = 4;

    // GLOBAL void gpu_pack(std::byte* out_buffer, std::byte const* in_range, 
    //                             int const* segment_offsets, int const* segment_sizes, 
    //                             size_t size, int pattern_size, int range_type_size);


    // GLOBAL void gpu_unpack(std::byte* out_range, std::byte const* in_buffer, 
    //                             int const* segment_offsets, int const* segment_sizes, 
    //                             size_t size, int pattern_size, int range_type_size);

    void gpu_pack_interface(int blocksize, int nb_segments, int nb_threadblocks,
                                std::byte* out_buffer, std::byte const* in_range, 
                                int const* segment_offsets, int const* segment_sizes, 
                                size_t size, int pattern_size, int range_type_size);

    void gpu_unpack_interface(int blocksize, int nb_segments, int nb_threadblocks,
                                std::byte* out_range, std::byte const* in_buffer,
                                int const* segment_offsets, int const* segment_sizes, 
                                size_t size, int pattern_size, int range_type_size);


    void gpu_pack_4_interface(int blocksize, int nb_segments, int nb_threadblocks,
                                std::byte* out_buffer, std::byte const* in_range, 
                                int const* segment_offsets,
                                size_t size, int pattern_size, int range_type_size);

    void gpu_unpack_4_interface(int blocksize, int nb_segments, int nb_threadblocks,
                                std::byte* out_range, std::byte const* in_buffer,
                                int const* segment_offsets,
                                size_t size, int pattern_size, int range_type_size);
}
#endif
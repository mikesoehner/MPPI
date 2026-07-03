#ifndef MPPI_GPUKERNEL_UTILITY_FUNCTIONS_H
#define MPPI_GPUKERNEL_UTILITY_FUNCTIONS_H

#include <cstddef>
#include <iostream>

#ifdef USE_HIP
#include <hip/hip_runtime.h>
#endif

namespace gpu_kernel
{
    void gpu_malloc(void** ptr, size_t bytes);
    void gpu_malloc_managed(void** ptr, size_t bytes);
    void gpu_free(void* ptr);
    void gpu_device_synchronize();
    void gpu_copy_H2D(void* dst, void const* src, size_t size);
    void gpu_copy_D2D(void* dst, void const* src, size_t size);
    void gpu_copy_D2H(void* dst, void const* src, size_t size);
    bool is_gpu_pointer(void* ptr);
}

#endif
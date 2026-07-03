#include "utility_functions.hpp"

namespace gpu_kernel
{
    #ifdef USE_HIP
    #define HIP_CHECK(condition)                                                                \
        {                                                                                       \
            const hipError_t error = condition;                                                 \
            if(error != hipSuccess)                                                             \
            {                                                                                   \
                std::cerr << "An error encountered: \"" << hipGetErrorString(error) << "\" at " \
                        << __FILE__ << ':' << __LINE__ << std::endl;                          \
                std::exit(1);                                                                   \
            }                                                                                   \
        }
    #elif USE_CUDA
    #define CUDA_CHECK(condition)                                                               \
        {                                                                                       \
            const cudaError_t error = condition;                                                \
            if(error != cudaSuccess)                                                            \
            {                                                                                   \
                std::cerr << "An error encountered: \"" << cudaGetErrorString(error) << "\" at "\
                        << __FILE__ << ':' << __LINE__ << std::endl;                          \
                std::exit(1);                                                                   \
            }                                                                                   \
        }
    #endif



    void gpu_malloc(void** ptr, size_t bytes)
    {
    #ifdef USE_HIP
        HIP_CHECK(hipMalloc(ptr, bytes));
    #elif USE_CUDA
        CUDA_CHECK(cudaMalloc(ptr, bytes));
    #endif
    }

    void gpu_malloc_managed(void** ptr, size_t bytes)
    {
    #ifdef USE_HIP
        HIP_CHECK(hipMallocManaged(ptr, bytes));
    #elif USE_CUDA
        CUDA_CHECK(cudaMallocManaged(ptr, bytes));
    #endif
    }

    void gpu_free(void* ptr)
    {
    #ifdef USE_HIP
        HIP_CHECK(hipFree(ptr));
    #elif USE_CUDA
        CUDA_CHECK(cudaFree(ptr));
    #endif
    }

    void gpu_device_synchronize()
    {
    #ifdef USE_HIP
        HIP_CHECK(hipDeviceSynchronize());
    #elif USE_CUDA
        CUDA_CHECK(cudaDeviceSynchronize());
    #endif
    }

    bool is_gpu_pointer(void* ptr)
    {
    #ifdef USE_HIP
        hipPointerAttribute_t attributes;
        HIP_CHECK(hipPointerGetAttributes(&attributes, ptr));

        return attributes.device > -1;
    #elif USE_CUDA
        cudaPointerAttributes attributes;
        CUDA_CHECK(cudaPointerGetAttributes(&attributes, ptr));

        return attributes.device > -1;
    #endif
        return false;
    }


    void gpu_copy_H2D(void* dst, void const* src, size_t size)
    {
    #ifdef USE_HIP
        HIP_CHECK(hipMemcpy(dst, src, size, hipMemcpyHostToDevice));
    #elif USE_CUDA
        CUDA_CHECK(cudaMemcpy(dst, src, size, cudaMemcpyHostToDevice));
    #endif   
    }


    void gpu_copy_D2D(void* dst, void const* src, size_t size)
    {
    #ifdef USE_HIP
        HIP_CHECK(hipMemcpy(dst, src, size, hipMemcpyDeviceToDevice));
    #elif USE_CUDA
        CUDA_CHECK(cudaMemcpy(dst, src, size, cudaMemcpyDeviceToDevice));
    #endif   
    }


    void gpu_copy_D2H(void* dst, void const* src, size_t size)
    {
    #ifdef USE_HIP
        HIP_CHECK(hipMemcpy(dst, src, size, hipMemcpyDeviceToHost));
    #elif USE_CUDA
        CUDA_CHECK(cudaMemcpy(dst, src, size, cudaMemcpyDeviceToHost));
    #endif   
    }
}
if(ENABLE_GPU)
  if(CMAKE_CUDA_COMPILER)
    find_package(CUDAToolkit REQUIRED)
    enable_language(CUDA)
    message(STATUS "CUDA supported")
    add_compile_definitions(USE_CUDA)
    set_source_files_properties(utility_functions.cpp PROPERTIES LANGUAGE CUDA)
    set_source_files_properties(kernel.cpp PROPERTIES LANGUAGE CUDA)
  elseif(CMAKE_HIP_COMPILER)
    enable_language(HIP)
    message(STATUS "HIP supported")
    add_compile_definitions(USE_HIP)
    set_source_files_properties(utility_functions.cpp PROPERTIES LANGUAGE HIP)
    set_source_files_properties(kernel.cpp PROPERTIES LANGUAGE HIP)
  else()
    message(STATUS "No GPU supported")
  endif()
else()
  message(STATUS "GPU support disabled")
endif()


add_library(gpu_kernel kernel.cpp utility_functions.cpp)
add_library(gpu_kernel::gpu_kernel ALIAS gpu_kernel)
target_include_directories(gpu_kernel 
    PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)


# link gpu kernels to MPPI interface library
if(CMAKE_CUDA_COMPILER)
  target_link_libraries(mppi
      INTERFACE gpu_kernel::gpu_kernel
                CUDA::cudart)
else()
  target_link_libraries(mppi
      INTERFACE gpu_kernel::gpu_kernel)
endif()

target_include_directories(mppi
    INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)


install(TARGETS gpu_kernel 
        EXPORT gpu_kernel
        DESTINATION ${CMAKE_INSTALL_PREFIX})

install(EXPORT gpu_kernel
        DESTINATION ${CMAKE_INSTALL_PREFIX}/cmake/gpu_kernel
        NAMESPACE gpu_kernel::)


install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

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


add_library(gpu_kernels kernel.cpp utility_functions.cpp)
add_library(gpu_kernels::gpu_kernels ALIAS gpu_kernels)

target_include_directories(gpu_kernels 
    PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)


# link gpu kernels to MPPI interface library
if(CMAKE_CUDA_COMPILER)
  target_link_libraries(mppi
      INTERFACE gpu_kernels::gpu_kernels
                CUDA::cudart)
else()
  target_link_libraries(mppi
      INTERFACE gpu_kernels::gpu_kernels)
endif()

#target_include_directories(mppi
#    INTERFACE
#    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
#    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)


install(TARGETS gpu_kernels
        EXPORT mppi
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        #DESTINATION ${CMAKE_INSTALL_PREFIX}
	#        DESTINATION ${CMAKE_INSTALL_PREFIX}/cmake/gpu_kernels
)


#install(EXPORT gpu_kernels
#        DESTINATION ${CMAKE_INSTALL_PREFIX}/cmake/gpu_kernels
#        NAMESPACE gpu_kernels::)


install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
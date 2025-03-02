include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

find_package(MPI REQUIRED)

add_executable(latency_benchmark latency_benchmark.cpp)
target_link_libraries(latency_benchmark
    PRIVATE 
        MPI::MPI_CXX)
target_compile_features(latency_benchmark
        PRIVATE
            cxx_std_23)

add_executable(latency_benchmark_control latency_benchmark_control.cpp)
target_link_libraries(latency_benchmark_control
    PRIVATE 
        MPI::MPI_CXX)
target_compile_features(latency_benchmark_control
        PRIVATE
            cxx_std_23)


add_executable(bandwidth_benchmark_control bandwidth_benchmark_control.cpp)
target_link_libraries(bandwidth_benchmark_control
    PRIVATE 
        MPI::MPI_CXX)
target_compile_features(bandwidth_benchmark_control
        PRIVATE
            cxx_std_23)
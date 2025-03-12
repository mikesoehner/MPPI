find_package(MPI REQUIRED)

add_executable(mppi
    "main.cpp"
)

target_include_directories(mppi
    PUBLIC    
        ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(mppi
    PRIVATE
        MPI::MPI_CXX)

target_compile_features(mppi
    PRIVATE
        cxx_std_23)
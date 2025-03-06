cmake_minimum_required(VERSION 3.17)

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")

project(mppi
    LANGUAGES CXX)

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(benchmarks)
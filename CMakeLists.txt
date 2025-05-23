cmake_minimum_required(VERSION 3.17)

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -freflection -fno-access-contexts -std=c++26")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")

project(mppi
    LANGUAGES CXX)

find_package(MPI REQUIRED)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(mppi INTERFACE)
target_include_directories(mppi
    INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/mppi>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

add_library(mppi::mppi ALIAS mppi)
target_link_libraries(mppi INTERFACE MPI::MPI_CXX)
target_compile_features(mppi INTERFACE cxx_std_23)

# set Release as the default build type if it is not yet set.
if(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)
    
# only combile tests in debug mode
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_subdirectory(tests)
endif()

add_subdirectory(benchmarks)


install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/cmake/mppiConfig.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mppi)

install(DIRECTORY mppi DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(TARGETS mppi EXPORT mppiTargets)

export(EXPORT mppiTargets
    NAMESPACE mppi::
    FILE mppiTargets.cmake)

install(EXPORT mppiTargets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mppi
    NAMESPACE mppi::
    FILE mppiTargets.cmake)
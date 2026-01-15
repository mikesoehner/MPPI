cmake_minimum_required(VERSION 3.27)


project(mppi
    VERSION 0.10.2
    LANGUAGES CXX)

find_package(MPI REQUIRED)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
endif()

add_library(mppi INTERFACE)
target_include_directories(mppi
    INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/mppi>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

add_library(mppi::mppi ALIAS mppi)
target_link_libraries(mppi INTERFACE MPI::MPI_CXX)
target_compile_features(mppi INTERFACE cxx_std_26)
    
if(BUILD_TESTING)
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
find_package(MPI REQUIRED)

message(STATUS "Run: ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} ${MPIEXEC_MAX_NUMPROCS} ${MPI_HOME}")

add_executable(mppi
    "main.cpp"
)

# set Release as the default build type if it is not yet set.
if(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

target_include_directories(mppi
    PUBLIC    
        ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(mppi
    PRIVATE
        MPI::MPI_CXX)

target_compile_features(mppi
    PRIVATE
        cxx_std_23)
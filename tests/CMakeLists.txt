include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

find_package(Catch2 3 REQUIRED)
find_package(MPI REQUIRED)
# These tests can use the Catch2-provided main
add_executable(amodes_tests amodes_tests.cpp)
target_link_libraries(amodes_tests 
    PRIVATE 
        Catch2::Catch2WithMain)

# These tests need their own main (to init MPI)
add_executable(custom-main-tests test_main.cpp)
target_link_libraries(custom-main-tests 
    PRIVATE 
        Catch2::Catch2
        MPI::MPI_CXX)

target_compile_features(amodes_tests
    PRIVATE
        cxx_std_23)

include(CTest)
include(Catch)
catch_discover_tests(custom-main-tests)
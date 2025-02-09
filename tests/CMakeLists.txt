include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

find_package(Catch2 3 REQUIRED)
find_package(MPI REQUIRED)
# These tests can use the Catch2-provided main
add_executable(amodes_tests amodes_tests.cpp)
target_link_libraries(amodes_tests 
    PRIVATE 
        Catch2::Catch2WithMain)
target_compile_features(amodes_tests
        PRIVATE
            cxx_std_23)


# These tests need their own main (to init MPI)
add_executable(custom-main-tests test_main.cpp)
target_link_libraries(custom-main-tests 
    PRIVATE 
        Catch2::Catch2
        MPI::MPI_CXX)


add_executable(amodes_failing_tests amodes_failing_tests.cpp)
target_compile_features(amodes_failing_tests
                        PRIVATE
                            cxx_std_23)
# Avoid building these targets normally
set_target_properties(amodes_failing_tests PROPERTIES
                        EXCLUDE_FROM_ALL TRUE
                        EXCLUDE_FROM_DEFAULT_BUILD TRUE)
# Add the tests.  These invoke "cmake --build ..." which is a
# cross-platform way of building the given target.
add_test(NAME amodes_failing_tests
            COMMAND ${CMAKE_COMMAND} --build . --target amodes_failing_tests --config $<CONFIGURATION>
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(amodes_failing_tests PROPERTIES WILL_FAIL TRUE)


include(CTest)
include(Catch)
catch_discover_tests(custom-main-tests)
catch_discover_tests(amodes_tests)
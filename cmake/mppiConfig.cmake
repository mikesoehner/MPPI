include(CMakeFindDependencyMacro)

find_dependency(MPI)

include(${CMAKE_CURRENT_LIST_DIR}/mppiTargets.cmake)

# check_required_components(mppi)
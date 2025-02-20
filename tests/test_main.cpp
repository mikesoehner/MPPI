#include <catch2/catch_session.hpp>
#include <mpi.h>
// #include "test.hpp"
#include "send_tests.hpp"
#include "communicator.hpp"
#include <numeric>
#include <vector>


int main(int argc, char* argv[])
{
    // Initialize MPI environment
    MPI_Init(&argc, &argv);
    
    int result = Catch::Session().run(argc, argv);

    MPI_Finalize();
    return result;
}
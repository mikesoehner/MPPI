#include <catch2/catch_session.hpp>
#include <mpi.h>
#include "test.hpp"

int main(int argc, char* argv[])
{
    // Initialize MPI environment
    MPI_Init(&argc, &argv);
    int result = Catch::Session().run(argc, argv);
    MPI_Finalize();

    return result;
}
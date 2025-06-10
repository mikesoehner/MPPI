#include <iostream>
#include "communicator.hpp"

int main(int argc, char** argv)
{
    MPI_Init(&argc, &argv);

    mppi::Communicator comm;

    if(comm.get_size() != 2)
    {
        std::cerr << "This test requires exactly two processes\n";
        MPI_Finalize();
        return 1;
    }

    // print preamble
    if(comm.get_rank() == 0)
        std::cout << "# OSU Inpired MPI Bandwidth Test\n";

    std::vector<MPI_Datatype> mpi_types;
    mpi_types.emplace_back(MPI_CHAR);
    mpi_types.emplace_back(MPI_INT);
    mpi_types.emplace_back(MPI_FLOAT);
    
    // loop through different datatypes that should be benchmarked
    for (auto mpi_type : mpi_types)
    {
        int type_size {};
        MPI_Type_size(mpi_type, &type_size);
        size_t min_message_size = type_size;
        size_t max_message_size = 4'194'304;

        char type_name[128];
        int resultlen {};
        MPI_Type_get_name(mpi_type, type_name, &resultlen);
        if (comm.get_rank() == 0)
        {
            fprintf(stdout, "# Datatype: %s.\n", type_name);
            fprintf(stdout, "%-*s", 10, "# Size");
            fprintf(stdout, "%*s", 20, "Bandwidth (MB/s)\n");
        }
        
            // loop through message sizes
        for (size_t size = min_message_size; size <= max_message_size; size *= 2)
        {
            // get number of elements in message
            auto nb_elements = size / type_size;
            //
            constexpr auto window_size = 64;
            MPI_Request request[window_size];
            MPI_Status reqstat[window_size];
        
            std::vector<std::byte> send_buf(size);
            std::vector<std::byte> recv_buf(size);

            MPI_Barrier(MPI_COMM_WORLD);

            double time_total = 0.0;
            size_t nb_iterations = 10'000;

            for (size_t iteration = 0; iteration < nb_iterations; iteration++)
            {
                if (comm.get_rank() == 0)
                {
                    double time_start = MPI_Wtime();

                    for (size_t j = 0; j < window_size; j++)
                        MPI_Isend(send_buf.data(), nb_elements, mpi_type, 1, 100, MPI_COMM_WORLD, request + j);

                    MPI_Waitall(window_size, request, reqstat);
                    // for (size_t j = 0; j < window_size; j++)
                    //     MPI_Wait(request+j, MPI_STATUS_IGNORE);

                    char c;
                    MPI_Recv(&c, 1, MPI_CHAR, 1, 101, MPI_COMM_WORLD, &reqstat[0]);

                    double time_end = MPI_Wtime();
                    time_total += time_end - time_start;
                }
                else if (comm.get_rank() == 1)
                {
                    for (size_t j = 0; j < window_size; j++)
                        MPI_Irecv(recv_buf.data(), nb_elements, mpi_type, 0, 100, MPI_COMM_WORLD, request + j);
                    
                    MPI_Waitall(window_size, request, reqstat);
                    // for (size_t j = 0; j < window_size; j++)
                    //     MPI_Wait(request+j, MPI_STATUS_IGNORE);
                    
                    char c = 'd';
                    MPI_Send(&c, 1, MPI_CHAR, 0, 101, MPI_COMM_WORLD);
                }
            }

            if (comm.get_rank() == 0)
            {
                double tmp_total = size / 1e6 * nb_iterations * window_size;

                fprintf(stdout, "%-*d", 10, static_cast<int>(size));
                fprintf(stdout, "%*.*f", 20, 2, tmp_total / time_total);
                fprintf(stdout, "\n");
                fflush(stdout);
            }
        }
    }

    MPI_Finalize();
    return 0;
}
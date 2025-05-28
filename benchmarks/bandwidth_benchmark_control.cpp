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

    MPI_Request request[1000];
    MPI_Status reqstat[1000];
    auto window_size = 64;

    auto internal_send_buf = new (std::align_val_t(8)) char[4194304];
    auto internal_recv_buf = new (std::align_val_t(8)) char[4194304];

    std::vector<char*> send_buf {internal_send_buf};
    std::vector<char*> recv_buf {internal_recv_buf};

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
        size_t max_message_size = 4194304;

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
            MPI_Barrier(MPI_COMM_WORLD);

            double time_total = 0.0;
            size_t nb_iterations = 10000;

            for (size_t iteration = 0; iteration < nb_iterations; iteration++)
            {
                if (comm.get_rank() == 0)
                {
                    double time_start = MPI_Wtime();

                    for (size_t j = 0; j < window_size; j++)
                        MPI_Isend(send_buf[0], nb_elements, mpi_type, 1, 100, MPI_COMM_WORLD, request + j);

                    MPI_Waitall(window_size, request, reqstat);
                    MPI_Recv(recv_buf[0], 1, MPI_CHAR, 1, 101, MPI_COMM_WORLD, &reqstat[0]);

                    double time_end = MPI_Wtime();
                    time_total += time_end - time_start;
                }
                else if (comm.get_rank() == 1)
                {
                    for (size_t j = 0; j < window_size; j++)
                        MPI_Irecv(recv_buf[0], nb_elements, mpi_type, 0, 100, MPI_COMM_WORLD, request + j);
                    
                    MPI_Waitall(window_size, request, reqstat);
                    MPI_Send(send_buf[0], 1, MPI_CHAR, 0, 101, MPI_COMM_WORLD);

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
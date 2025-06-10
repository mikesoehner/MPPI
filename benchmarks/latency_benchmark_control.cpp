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
        std::cout << "# OSU Inpired MPI Latency Test\n";

    std::vector<MPI_Datatype> mpi_types;
    mpi_types.emplace_back(MPI_CHAR);
    mpi_types.emplace_back(MPI_INT);
    mpi_types.emplace_back(MPI_DOUBLE);

    MPI_Status reqstat{};

    // loop through different datatypes that should be benchmarked
    for (auto mpi_type : mpi_types)
    {
        int type_size {};
        MPI_Type_size(mpi_type, &type_size);
        size_t min_message_size = type_size;
        constexpr size_t max_message_size = 4'194'304;

        char type_name[128];
        int resultlen {};
        MPI_Type_get_name(mpi_type, type_name, &resultlen);
        
        if (comm.get_rank() == 0)
        {
            fprintf(stdout, "# Datatype: %s.\n", type_name);
            fprintf(stdout, "%-*s", 10, "# Size");
            fprintf(stdout, "%*s", 20, "Avg Latency(us)\n");
        }
        
        // loop through message sizes
        for (size_t size = min_message_size; size <= max_message_size; size *= 2)
        {
            // get number of elements in message
            auto nb_elements = size / type_size;
            // init buffers
            std::vector<char> send_buf(size);
            std::vector<char> recv_buf(size);
            //
            MPI_Barrier(MPI_COMM_WORLD);

            double time_total = 0.0;
            size_t nb_iterations = 10000;

            for (size_t iteration = 0; iteration < nb_iterations; iteration++)
            {
                if (comm.get_rank() == 0)
                {
                    double time_start = MPI_Wtime();

                    MPI_Send(send_buf.data(), nb_elements, mpi_type, 1, 1, MPI_COMM_WORLD);
                    MPI_Recv(recv_buf.data(), nb_elements, mpi_type, 1, 1, MPI_COMM_WORLD, &reqstat);

                    double time_end = MPI_Wtime();
                    time_total += time_end - time_start;
                }
                else if (comm.get_rank() == 1)
                {
                    MPI_Recv(recv_buf.data(), nb_elements, mpi_type, 0, 1, MPI_COMM_WORLD, &reqstat);
                    MPI_Send(send_buf.data(), nb_elements, mpi_type, 0, 1, MPI_COMM_WORLD);
                }
            }

            if (comm.get_rank() == 0)
            {
                double latency = (time_total * 1e6) / (2.0 * nb_iterations);
                fprintf(stdout, "%-*d", 10, static_cast<int>(size));
                fprintf(stdout, "%*.*f", 20, 2, latency);
                fprintf(stdout, "\n");
                fflush(stdout);
            }
        }
    }

    MPI_Finalize();
    return 0;
}
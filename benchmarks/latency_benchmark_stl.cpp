#include <iostream>
#include <type_traits>
#include "communicator.hpp"


int main(int argc, char** argv)
{
    MPI_Init(&argc, &argv);

    Communicator comm;

    if(comm.get_size() != 2)
    {
        std::cerr << "This test requires exactly two processes\n";
        MPI_Finalize();
        return 1;
    }

    // print preamble
    if(comm.get_rank() == 0)
        std::cout << "# OSU Inpired MPI Latency Test\n";
    
    auto benchmark = [&comm]<typename T>(T t)
    {
        auto type_size = sizeof(T);
        
        size_t min_message_size = type_size;
        size_t max_message_size = 4'194'304 * 4;

        if (comm.get_rank() == 0)
        {
            if constexpr (std::is_same_v<T, char>)
                fprintf(stdout, "# Datatype: char.\n");
            else if constexpr (std::is_same_v<T, int>)
                fprintf(stdout, "# Datatype: int.\n");
            else if constexpr (std::is_same_v<T, double>)
                fprintf(stdout, "# Datatype: double.\n");
            else
            {
                MPI_Abort(MPI_COMM_WORLD, 1);
                return 1;
            }

            fprintf(stdout, "%-*s", 10, "# Size");
            fprintf(stdout, "%*s", 20, "Avg Latency(us)\n");
        }
        
        // loop through message sizes
        for (size_t size = min_message_size; size <= max_message_size; size *= 2)
        {
            // get number of elements in message
            auto nb_elements = size / type_size;
            // init buffers
            std::vector<T> send_buf(nb_elements);
            std::vector<T> recv_buf(nb_elements);
            //
            MPI_Barrier(MPI_COMM_WORLD);

            double time_total = 0.0;
            size_t nb_iterations = 10000;

            for (size_t iteration = 0; iteration < nb_iterations; iteration++)
            {
                if (comm.get_rank() == 0)
                {
                    double time_start = MPI_Wtime();

                    comm.send(1, Tag(1), send_buf);
                    comm.recv(1, Tag(1), recv_buf);

                    double time_end = MPI_Wtime();
                    time_total += time_end - time_start;
                }
                else if (comm.get_rank() == 1)
                {
                    comm.recv(0, Tag(1), recv_buf);
                    comm.send(0, Tag(1), send_buf);
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
    };

    auto wrapper = [benchmark]<typename... Ts>(Ts... types)
    {
        (benchmark(types), ...);
    };

    wrapper(char{}, int{}, double{});


    MPI_Finalize();
    return 0;
}
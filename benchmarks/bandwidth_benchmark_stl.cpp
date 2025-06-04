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

    auto benchmark = [&comm]<typename T>(T t)
    {   
        auto type_size = sizeof(T);

        size_t min_message_size = type_size;
        size_t max_message_size = 4194304;

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
            fprintf(stdout, "%*s", 20, "Bandwidth (MB/s)\n");
        }
        
        // loop through message sizes
        for (size_t size = min_message_size; size <= max_message_size; size *= 2)
        {
            // get number of elements in message
            auto nb_elements = size / type_size;

            constexpr auto window_size = 64;
            std::array<mppi::Request, window_size> requests;
            MPI_Status reqstat[window_size];

            std::vector<T> send_buf(nb_elements);
            std::vector<T> recv_buf(nb_elements);
            //
            MPI_Barrier(MPI_COMM_WORLD);

            double time_total = 0.0;
            size_t nb_iterations = 10'000;

            for (size_t iteration = 0; iteration < nb_iterations; iteration++)
            {
                if (comm.get_rank() == 0)
                {
                    double time_start = MPI_Wtime();

                    for (size_t j = 0; j < window_size; j++)
                        requests[j] = comm.isend(1, mppi::Tag(99), send_buf | std::ranges::views::all);

                    comm.waitall(requests);
                    // MPI_Waitall(window_size, request, reqstat);
                    char c;
                    comm.recv(1, mppi::Tag(101), c);
                    // MPI_Recv(recv_buf[0], 1, MPI_CHAR, 1, 101, MPI_COMM_WORLD, &reqstat[0]);

                    double time_end = MPI_Wtime();
                    time_total += time_end - time_start;
                }
                else if (comm.get_rank() == 1)
                {
                    for (size_t j = 0; j < window_size; j++)
                        requests[j] = comm.irecv(0, mppi::Tag(99), recv_buf | std::ranges::views::all);
                    
                    comm.waitall(requests);
                    // MPI_Waitall(window_size, request, reqstat);
                    char c = 'd';
                    comm.send(0, mppi::Tag(101), c);
                    // MPI_Send(send_buf[0], 1, MPI_CHAR, 0, 101, MPI_COMM_WORLD);

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
    };

    auto wrapper = [benchmark]<typename... Ts>(Ts... types)
    {
        (benchmark(types), ...);
    };

    wrapper(char{}, int{}, double{});

    MPI_Finalize();
    return 0;
}
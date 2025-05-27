#include <iostream>
#include <tuple>
#include "communicator.hpp"


class Molecule
{
public:
    Molecule() = default;

private:
    unsigned int _cid {};
    std::array<double, 3> _r;
    std::array<double, 3> _F;
    std::array<double, 3> _v;
    std::array<double, 3> _M;
    std::array<double, 3> _L;
    std::array<double, 3> _Vi;
    std::array<double, 3> _I;
    std::array<double, 3> _invI;
    std::array<double, 4> _q;
    unsigned long _id {};
    double _m {};
    unsigned _soa_index_lj {};
    unsigned _soa_index_c {};
    unsigned _soa_index_d {};
    unsigned _soa_index_q {};
};

class SimpleClass
{
public:
    SimpleClass() = default;

private:
    std::array<double, 2> _x;
    double _y;
    char _a;
    int _n;
};


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

    MPI_Status reqstat;

    constexpr mppi::DataPattern<SimpleClass, "_x", "_y", "_n"> simple_pattern;
    constexpr mppi::DataPattern<Molecule, "_id", "_cid", "_r", "_q"> mol_pattern;

    std::tuple data_patterns {simple_pattern, mol_pattern};

    constexpr auto size = std::tuple_size_v<decltype(data_patterns)>;
    
    auto benchmark = [&comm]<typename Pattern>(Pattern data_pattern)
    {
        auto type_size = data_pattern.get_size();
        
        size_t min_message_size = type_size;
        size_t max_message_size = 4'194'304 * 4;

        char type_name[128] = {"MPI_CUSTOM_TYPE"};
        // int resultlen {};
        // MPI_Type_get_name(mpi_type, type_name, &resultlen);
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
            using VectorType = typename Pattern::base_type;
            std::vector<VectorType> send_buf(nb_elements);
            std::vector<VectorType> recv_buf(nb_elements);
            //
            MPI_Barrier(MPI_COMM_WORLD);

            double time_total = 0.0;
            size_t nb_iterations = 10000;

            for (size_t iteration = 0; iteration < nb_iterations; iteration++)
            {
                if (comm.get_rank() == 0)
                {
                    double time_start = MPI_Wtime();

                    comm.send(1, mppi::Tag(1), data_pattern, send_buf);
                    comm.recv(1, mppi::Tag(1), data_pattern, recv_buf);

                    double time_end = MPI_Wtime();
                    time_total += time_end - time_start;
                }
                else if (comm.get_rank() == 1)
                {
                    comm.recv(0, mppi::Tag(1), data_pattern, recv_buf);
                    comm.send(0, mppi::Tag(1), data_pattern, send_buf);
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

    auto wrapper = [benchmark]<typename Tuple, std::size_t... Ids>(Tuple& tuple, std::index_sequence<Ids...>)
    {
        (benchmark(std::get<Ids>(tuple)), ...);
    };

    wrapper(data_patterns, std::make_index_sequence<size>{});


    MPI_Finalize();
    return 0;
}
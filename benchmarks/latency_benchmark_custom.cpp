#include <iostream>
#include <tuple>
#include "communicator.hpp"

class CustomType
{
public:
    CustomType() = default;

private:
    std::array<double, 2> _x;
    double _y;
    char _a;
    int _n;
};

class MoleculeType
{
public:
    MoleculeType() = default;

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

class ContinousType
{
public:
    std::array<char, 1024> _arr;
};

class NestedBase
{
public:
    char _c;
};

class NestedDerived : public NestedBase
{};

class NestedDerivedDerived : public NestedDerived
{};


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

    constexpr mppi::Pattern<ContinousType, "_arr"> cont_custom_pattern;
    constexpr mppi::Pattern<NestedDerivedDerived, "_c"> derived_pattern;

    constexpr mppi::Pattern<CustomType, "_x", "_y", "_n"> custom_pattern;
    constexpr mppi::Pattern<MoleculeType, "_id", "_cid", "_r", "_q"> mol_pattern;

    std::tuple patterns {cont_custom_pattern, derived_pattern, custom_pattern, mol_pattern};

    constexpr auto tuple_size = std::tuple_size_v<decltype(patterns)>;

    std::vector<std::array<char, 128>> type_names{{"Continous Type"}, {"Nested Type"}, {"Custom Type"}, {"Molecule Type"}};
    
    auto benchmark = [&comm]<typename Pattern>(Pattern pattern, std::array<char, 128>& type_name)
    {
        auto type_size = pattern.get_packed_size();
        
        size_t min_message_size = type_size;
        size_t max_message_size = 4'194'304;

        if (comm.get_rank() == 0)
        {
            fprintf(stdout, "# Datatype: %s.\n", type_name.data());
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

                    comm.send(mppi::Destination(1), mppi::Tag(1), pattern, send_buf);
                    comm.recv(mppi::Source(1), mppi::Tag(1), pattern, recv_buf);

                    double time_end = MPI_Wtime();
                    time_total += time_end - time_start;
                }
                else if (comm.get_rank() == 1)
                {
                    comm.recv(mppi::Source(0), mppi::Tag(1), pattern, recv_buf);
                    comm.send(mppi::Destination(0), mppi::Tag(1), pattern, send_buf);
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

    auto wrapper = [benchmark]<typename Tuple, std::size_t... Ids>(Tuple& tuple, std::vector<std::array<char, 128>>& type_names, std::index_sequence<Ids...>)
    {
        (benchmark(std::get<Ids>(tuple), type_names[Ids]), ...);
    };

    wrapper(patterns, type_names, std::make_index_sequence<tuple_size>{});


    MPI_Finalize();
    return 0;
}
#include <iostream>
#include "communicator.hpp"

class Molecule
{
public:
    Molecule() = default;

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

    // prepare custom mol type
    int B_mol[] = { 1, 1, 1, 3, 4 };
    MPI_Aint D_mol[] = {
        offsetof(Molecule, _id),
        offsetof(Molecule, _cid),
        offsetof(Molecule, _cid),
        offsetof(Molecule, _r),
        offsetof(Molecule, _q)
    };
    MPI_Datatype T_mol[] = {
        MPI_UNSIGNED_LONG,
        MPI_UNSIGNED,
        MPI_UNSIGNED,
        MPI_DOUBLE,
        MPI_DOUBLE
    };

    MPI_Datatype mpi_dt_mol;
    MPI_Type_create_struct(5, B_mol, D_mol, T_mol, &mpi_dt_mol);
    MPI_Type_commit(&mpi_dt_mol);

    // prepare custom simple type
    int B_simple[] = { 2, 1, 1 };
    MPI_Aint D_simple[] = {
        offsetof(SimpleClass, _x),
        offsetof(SimpleClass, _y),
        offsetof(SimpleClass, _n)
    };
    MPI_Datatype T_simple[] = {
        MPI_DOUBLE,
        MPI_DOUBLE,
        MPI_INT
    };

    MPI_Datatype mpi_dt_simple_not_resized;
    MPI_Datatype mpi_dt_simple;
    MPI_Type_create_struct(3, B_simple, D_simple, T_simple, &mpi_dt_simple_not_resized);
    MPI_Type_create_resized(mpi_dt_simple_not_resized, 0, sizeof(SimpleClass), &mpi_dt_simple);
    MPI_Type_commit(&mpi_dt_simple);

    std::vector<MPI_Datatype> mpi_types {mpi_dt_simple, mpi_dt_mol};

    std::vector<size_t> original_sizes {sizeof(SimpleClass), sizeof(Molecule)};

    // loop through different datatypes that should be benchmarked
    for (size_t i = 0; i < mpi_types.size(); i++)
    {
        auto mpi_type = mpi_types[i];

        int type_size {};
        MPI_Type_size(mpi_type, &type_size);
        size_t min_message_size = type_size;
        size_t max_message_size = 4'194'304 * 4;

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
            std::vector<std::byte> send_buf(nb_elements * original_sizes[i]);
            std::vector<std::byte> recv_buf(nb_elements * original_sizes[i]);
            //
            MPI_Barrier(MPI_COMM_WORLD);

            double time_total = 0.0;
            size_t nb_iterations = 10'000;

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
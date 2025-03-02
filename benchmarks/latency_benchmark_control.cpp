#include <iostream>
#include "communicator.hpp"

struct TestClass
{
public:
    TestClass() = default;
    TestClass(int a, int b, double c, std::array<float, 3> d, std::array<float, 3> e, std::array<float, 3> f)
        : _a(a), _b(b), _c(c), _d(d), _e(e), _f(f)
    {}

    int& get_a() { return _a; }
    int& get_b() { return _b; }
    double& get_c() { return _c; }
    std::array<float, 3>& get_d() { return _d; }
    std::array<float, 3>& get_e() { return _e; }
    std::array<float, 3>& get_f() { return _f; }

    int _a {};
    int _b {};
    double _c {};
    std::array<float, 3> _d {};
    std::array<float, 3> _e {};
    std::array<float, 3> _f {};
};

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

    MPI_Status reqstat;

    // prepare custom mpi type
    int B[] = {
        1, // 1 int
        1, // 1 int
        3, // 3 float's
        3 // 3 float's
    };
    MPI_Aint D[] = {
        offsetof(struct TestClass, _a),
        offsetof(struct TestClass, _b),
        offsetof(struct TestClass, _d),
        offsetof(struct TestClass, _f),
        sizeof(struct TestClass)
    };
    MPI_Datatype T[] = {
        MPI_INT,
        MPI_INT,
        MPI_FLOAT,
        MPI_FLOAT
    };

    MPI_Datatype mpi_dt_mystruct;
    MPI_Type_create_struct(4, B, D, T, &mpi_dt_mystruct);
    MPI_Type_commit(&mpi_dt_mystruct);

    std::vector<MPI_Datatype> mpi_types;
    mpi_types.emplace_back(mpi_dt_mystruct);
    // mpi_types.emplace_back(MPI_CHAR);
    // mpi_types.emplace_back(MPI_INT);
    // mpi_types.emplace_back(MPI_FLOAT);

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
            fprintf(stdout, "%*s", 20, "Avg Latency(us)\n");
        }
        
            // loop through message sizes
        for (size_t size = min_message_size; size <= max_message_size; size *= 2)
        {
            // get number of elements in message
            auto nb_elements = size / type_size;
            // init buffers
            std::vector<TestClass> send_buf(nb_elements);
            std::vector<TestClass> recv_buf(nb_elements);
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
#include <iostream>
#include "communicator.hpp"

class TestClass
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

private:
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

    std::vector<MPI_Datatype> mpi_types;
    // mpi_types.emplace_back(MPI_CHAR);
    mpi_types.emplace_back(MPI_DATATYPE_NULL);

    TestClass test;
    DataPattern data_pattern(&test, test.get_a(), test.get_b(), test.get_d(), test.get_f());

    // loop through different datatypes that should be benchmarked
    for (auto mpi_type : mpi_types)
    {
        int type_size {};
        if (mpi_type != MPI_DATATYPE_NULL)
            MPI_Type_size(mpi_type, &type_size);
        else
            type_size = data_pattern.get_size();
        
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
            std::vector<TestClass> send_buf(nb_elements);
            std::vector<TestClass> recv_buf(nb_elements);
            // std::vector<char> send_buf(size);
            // std::vector<char> recv_buf(size);
            //
            MPI_Barrier(MPI_COMM_WORLD);

            double time_total = 0.0;
            size_t nb_iterations = 10000;

            for (size_t iteration = 0; iteration < nb_iterations; iteration++)
            {
                if (comm.get_rank() == 0)
                {
                    double time_start = MPI_Wtime();

                    comm.send(1, Tag(1), data_pattern, send_buf);
                    comm.recv(1, Tag(1), data_pattern, recv_buf);

                    double time_end = MPI_Wtime();
                    time_total += time_end - time_start;
                }
                else if (comm.get_rank() == 1)
                {
                    comm.recv(0, Tag(1), data_pattern, recv_buf);
                    comm.send(0, Tag(1), data_pattern, send_buf);
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
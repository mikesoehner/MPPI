#include <iostream>
#include <tuple>
#include "communicator.hpp"

class TestClass
{
public:
    TestClass() = default;
    TestClass(int a, int b, double c, std::vector<float> d, std::array<float, 3> e, std::array<float, 3> f)
        : _a(a), _b(b), _c(c), _d(d), _e(e), _f(f)
    {}

    int& get_a() { return _a; }
    int& get_b() { return _b; }
    double& get_c() { return _c; }
    std::vector<float>& get_d() { return _d; }
    std::array<float, 3>& get_e() { return _e; }
    std::array<float, 3>& get_f() { return _f; }

private:
    int _a {};
    int _b {};
    double _c {};
    std::vector<float> _d {};
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

    // DataPattern is not dynamic because one of the requested members has a dynamic size
    // /*constexpr */DataPattern<TestClass, "_a", "_b", "_d", "_f"> data_pattern;

    auto type_size = 32;//data_pattern.get_size();
    
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
        std::vector<TestClass> send_buf(nb_elements, TestClass(1, 2, 3.0, {4.0f, 5.6f, 7.8f}, {6.2f, 5.7f, 8.4f}, {1.2f, 2.3f, 3.4f}));
        std::vector<TestClass> recv_buf(nb_elements, TestClass(1, 2, 3.0, {4.0f, 5.6f, 7.8f}, {6.2f, 5.7f, 8.4f}, {1.2f, 2.3f, 3.4f}));
        
        // DataPattern is not dynamic because one of the requested members has a dynamic size
        DataPattern<TestClass, "_a", "_b", "_d", "_f"> data_pattern(send_buf);
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


    MPI_Finalize();
    return 0;
}
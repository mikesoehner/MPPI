#ifndef MPPI_COMMUNICATOR_H
#define MPPI_COMMUNICATOR_H

#include "mpi.h"
#include "datatype.hpp"
#include "tag.hpp"
#include "parameter_pack_helpers.hpp"
#include <memory_resource>
#include <vector>



class Communicator
{
public:
    Communicator(MPI_Comm mpi_comm = MPI_COMM_WORLD)
        : _mpi_comm(mpi_comm), _buffer{}, _pool{&_buffer}
    {
        MPI_Comm_rank(_mpi_comm, &_rank);
        MPI_Comm_size(_mpi_comm, &_size);
    }

    auto get_rank() const { return _rank; }
    auto get_size() const { return _size; }

    template<typename... Ts>
    void send(int destination, Tag tag, Ts... ranges) 
    {
        Data data(Send{}, _pool, ranges...);
        MPI_Send(data.get_data(), data.get_count(), data.get_type(), destination, tag.get(), _mpi_comm); 
    }

    template<typename... Ts, are_input_ranges... Rs>
    void send(int destination, Tag tag, DataPattern<Ts...> const& data_pattern, Rs... ranges)
    {
        Data data(Send{}, _pool, data_pattern, ranges...);
        MPI_Send(data.get_data(), data.get_count(), data.get_type(), destination, tag.get(), _mpi_comm);
    }

    // TODO: It is bad to have these 3 separate functions that basically do the same
    template<are_fundamentals... Fs>
    auto recv(int source, Tag tag, Fs&... fundamentals)
    {
        Data data(Recv{}, _pool, fundamentals...);

        MPI_Recv(data.get_data(), data.get_count(), data.get_type(), source, tag.get(), _mpi_comm, MPI_STATUS_IGNORE);

        data.retrieve_data(fundamentals...);
    }

    template<are_only_ranges... Rs>
    auto recv(int source, Tag tag, Rs&... ranges)
    {
        Data data(Recv{}, _pool, ranges...);

        MPI_Recv(data.get_data(), data.get_count(), data.get_type(), source, tag.get(), _mpi_comm, MPI_STATUS_IGNORE);

        data.retrieve_data(ranges...);
    }

    template<are_only_views... Vs>
    auto recv(int source, Tag tag, Vs... views)
    {
        Data data(Recv{}, _pool, views...);

        MPI_Recv(data.get_data(), data.get_count(), data.get_type(), source, tag.get(), _mpi_comm, MPI_STATUS_IGNORE);

        data.retrieve_data(views...);
    }

    template<typename... Ts, are_only_ranges... Rs>
    auto recv(int source, Tag tag, DataPattern<Ts...> const& data_pattern, Rs&... ranges)
    {
        Data data(Recv{}, _pool, data_pattern, ranges...);

        MPI_Recv(data.get_data(), data.get_count(), data.get_type(), source, tag.get(), _mpi_comm, MPI_STATUS_IGNORE);

        data.retrieve_data(data_pattern , ranges...);
    }

private:
    // stores MPI container
    MPI_Comm _mpi_comm {};
    // stores MPI rank and size
    int _rank {};
    int _size {};

    // resource that only allocates and not frees memory
    std::pmr::monotonic_buffer_resource _buffer;
    // resource that serves as memory pool for send and recv buffers
    std::pmr::unsynchronized_pool_resource _pool;
};



// TODO: maybe combine this with a topology class
class Connection
{
public:
    Connection(Communicator comm, int other_rank)
        : _my_rank(comm.get_rank())
    {
        _other_ranks.emplace_back(other_rank);
    }

    template<typename... Ranks>
        requires (std::is_same_v<int, Ranks> && ...)
    Connection(Communicator comm, Ranks... ranks)
        : _my_rank(comm.get_rank())
    {
        _other_ranks.emplace_back(ranks...);
    }


private:
    int _my_rank{};
    std::vector<int> _other_ranks;
};

#endif
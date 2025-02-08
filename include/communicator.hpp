#ifndef MPPI_COMMUNICATOR_H
#define MPPI_COMMUNICATOR_H

#include "mpi.h"
#include "datatype.hpp"
#include "tag.hpp"
#include "parameter_pack_helpers.hpp"



// base case
template<typename C, typename R>
constexpr void copy_range(C& result_data, size_t offset, R head)
{
    auto range_size = std::ranges::distance(head);

    std::ranges::copy(result_data.begin() + offset, result_data.begin() + offset + range_size, head.begin());
}
// specialization case
template<typename C, typename R, typename... Rs>
constexpr void copy_range(C& result_data, size_t offset, R head, Rs... tail)
{
    auto range_size = std::ranges::distance(head);

    std::ranges::copy(result_data.begin() + offset, result_data.begin() + offset + range_size, head.begin());

    offset += range_size;
    copy_range(result_data, offset, tail...);
}
// free helper function
template<typename C, typename... Rs>
void free_copy_range(C& result_data, Rs... ranges)
{
    size_t offset = 0;
    copy_range(result_data, offset, ranges...);
}




class Connection;

template<typename... Vs>
class Views
{
public:
    Views(Vs... views) {}
};


class Communicator
{
public:
    Communicator() {
        _mpi_comm = MPI_COMM_WORLD;
    }

    Communicator(MPI_Comm mpi_comm)
        : _mpi_comm(mpi_comm)
    {}

    auto get_rank() const 
    { 
        int rank; 
        MPI_Comm_rank(_mpi_comm, &rank);
        return rank;
    }

    template<typename... Ts>
    void send(Data<Ts...> data, int destination) const 
    {
        MPI_Send(data.get_data(), data.get_count(), data.get_type(), destination, MPI_ANY_TAG, _mpi_comm); 
    }

    template<typename... Ts>
    void send(Data<Ts...> data, int destination, Tag tag) const 
    {
        MPI_Send(data.get_data(), data.get_count(), data.get_type(), destination, tag.get(), _mpi_comm); 
    }

    template<typename... Ts>
    auto recv(int source, Tag tag, Ts... ranges) const
    {
        typedef decltype(free_get_types<Ts...>()) BufferType;
        std::vector<typename Data<Ts...>::BufferType> result(free_ranges_size(ranges...));

        MPI_Recv(result.data(), result.size(), Type2MPI::transform(BufferType{}), source, tag.get(), _mpi_comm, MPI_STATUS_IGNORE);

        free_copy_range(result, ranges...);
    }

private:
    MPI_Comm _mpi_comm;
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
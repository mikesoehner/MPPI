#ifndef MPPI_COMMUNICATOR_H
#define MPPI_COMMUNICATOR_H


#include <memory_resource>
#include <vector>
#include <format>
#include <iostream>

#include "mpi.h"
#include "datatype.hpp"
#include "tag.hpp"
#include "parameter_pack_helpers.hpp"


class TrackAllocator : public std::pmr::memory_resource {
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);

        void* p = std::pmr::new_delete_resource()->allocate(bytes, alignment);
        if (rank == 0) std::cout << std::format("  do_allocate: {:6} bytes at {}\n", bytes, p);
        return p;
    }
 
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);

        if (rank == 0) std::cout << std::format("  do_deallocate: {:4} bytes at {}\n", bytes, p);
        return std::pmr::new_delete_resource()->deallocate(p, bytes, alignment);
    }
 
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return std::pmr::new_delete_resource()->is_equal(other);
    }
};


class MPIAllocator : public std::pmr::memory_resource {
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        void* p;
        MPI_Info info = MPI_INFO_NULL;
        MPI_Info_create(&info);
        // TODO: Is this size_t -> char conversion safe? Does it always fit?
        std::array<char, 10> chars;
        std::to_chars(chars.data(), chars.data() + chars.size(), alignment);
        MPI_Info_set(info, "mpi_minimum_memory_alignment", chars.data());
        MPI_Alloc_mem(static_cast<MPI_Aint>(bytes), info, &p);

        return p;
    }
 
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        MPI_Free_mem(p);
    }
 
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};


class Communicator
{
public:
    Communicator(MPI_Comm mpi_comm = MPI_COMM_WORLD)
       : _mpi_comm(mpi_comm), _buffer{&_mpi_allocator}, _pool{std::pmr::pool_options{1, 1024*1024*1024}, &_buffer}
    {
        MPI_Comm_rank(_mpi_comm, &_rank);
        MPI_Comm_size(_mpi_comm, &_size);
    }

    auto get_rank() const { return _rank; }
    auto get_size() const { return _size; }

    template<typename... Ts>
        requires are_fundamentals<Ts...>  || are_only_ranges<Ts...>
    void send(int destination, Tag tag, Ts&... ranges)
    {
        Data data(Send{}, _pool, ranges...);
        MPI_Send(data.get_data(), data.get_count(), data.get_type(), destination, tag.get(), _mpi_comm); 
    }

    template<are_only_views... Vs>
    void send(int destination, Tag tag, Vs... views) 
    {
        Data data(Send{}, _pool, views...);
        MPI_Send(data.get_data(), data.get_count(), data.get_type(), destination, tag.get(), _mpi_comm); 
    }

    template<typename... Ts, are_input_ranges... Rs>
    void send(int destination, Tag tag, DataPattern<Ts...> const& data_pattern, Rs&... ranges)
    {
        Data data(Send{}, _pool, data_pattern, ranges...);
        MPI_Send(data.get_data(), data.get_count(), data.get_type(), destination, tag.get(), _mpi_comm);
    }

    // TODO: It is bad to have these 2 separate functions that basically do the same
    template<typename... Ts>
        requires are_fundamentals<Ts...> || are_only_ranges<Ts...>
    auto recv(int source, Tag tag, Ts&... ranges)
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

    // uses MPI_Alloc_mem
    MPIAllocator _mpi_allocator;
    // for debugging purposes
    TrackAllocator _track_allocator;
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
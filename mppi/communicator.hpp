#ifndef MPPI_COMMUNICATOR_H
#define MPPI_COMMUNICATOR_H


#include <memory_resource>
#include <vector>
#include <format>
#include <iostream>
#include <memory>
#include <functional>

#include "mpi.h"
#include "datatype.hpp"
#include "tag.hpp"
#include "parameter_pack_helpers.hpp"
#include "source_destination.hpp"


namespace mppi 
{
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
    

    class Request
    {
    public:
        Request() = default;

        template<is_send_or_recv SR, typename D, are_input_ranges... Ranges>
        Request(SR, D data, MPI_Request request, Ranges&... ranges)
            : _mpi_request(request)
        {
            // if it is a recv request, we have to get the data back
            if constexpr (std::is_same_v<SR, Recv>)
            {
                auto data_ptr = std::make_unique<D>(std::move(data));
                auto tuple_ptr = std::make_unique<std::tuple<Ranges...>>(std::make_tuple(ranges...));

                _retrieve_data = [data_ptr = std::move(data_ptr), tuple_ptr = std::move(tuple_ptr)]
                {
                    std::apply([&](auto &&... args) { data_ptr->retrieve_data(args...); }, *tuple_ptr);
                };
            }
            else
            {
                _retrieve_data = [](){};
            }        
        }

        template<is_send_or_recv SR, typename D, typename... Ts, are_input_ranges... Ranges>
        Request(SR, D data, MPI_Request request, Pattern<Ts...> pattern, Ranges&... ranges)
            : _mpi_request(request)
        {
            // if it is a recv request, we have to get the data back
            if constexpr (std::is_same_v<SR, Recv>)
            {
                auto data_ptr = std::make_unique<D>(std::move(data));
                auto tuple_ptr = std::make_unique<std::tuple<Ranges...>>(std::make_tuple(ranges...));
                auto pattern_ptr = std::make_unique<Pattern<Ts...>>(std::move(pattern));

                _retrieve_data = [data_ptr = std::move(data_ptr), tuple_ptr = std::move(tuple_ptr), pattern_ptr = std::move(pattern_ptr)]
                {
                    std::apply([&](auto &&... args) { data_ptr->retrieve_data(*pattern_ptr, args...); }, *tuple_ptr);
                };
            }
            else
            {
                _retrieve_data = [](){};
            }        
        }

        auto& get() { return _mpi_request; }
        auto retrieve_data() { _retrieve_data(); }

    private:
        MPI_Request _mpi_request {};
        std::move_only_function<void()> _retrieve_data;
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
        void send(Destination destination, Tag tag, Ts&... ranges)
        {
            Data data(Send{}, _pool, ranges...);
            MPI_Send(data.get_data(), data.get_count(), data.get_type(), destination.get(), tag.get(), _mpi_comm); 
        }
    
        template<are_only_views... Vs>
        void send(Destination destination, Tag tag, Vs... views) 
        {
            Data data(Send{}, _pool, views...);
            MPI_Send(data.get_data(), data.get_count(), data.get_type(), destination.get(), tag.get(), _mpi_comm); 
        }
    
        template<typename... Ts, are_input_ranges... Rs>
        void send(Destination destination, Tag tag, Pattern<Ts...> const& pattern, Rs&... ranges)
        {
            Data data(Send{}, _pool, pattern, ranges...);
            MPI_Send(data.get_data(), data.get_count(), data.get_type(), destination.get(), tag.get(), _mpi_comm);
        }
    
        template<typename... Ts, are_only_views... Vs>
        void send(Destination destination, Tag tag, Pattern<Ts...> const& pattern, Vs... views)
        {
            Data data(Send{}, _pool, pattern, views...);
            MPI_Send(data.get_data(), data.get_count(), data.get_type(), destination.get(), tag.get(), _mpi_comm);
        }
    
        // TODO: It is bad to have these 2 separate functions that basically do the same
        template<typename... Ts>
            requires are_fundamentals<Ts...> || are_only_ranges<Ts...>
        auto recv(Source source, Tag tag, Ts&... ranges)
        {
            Data data(Recv{}, _pool, ranges...);
    
            MPI_Recv(data.get_data(), data.get_count(), data.get_type(), source.get(), tag.get(), _mpi_comm, MPI_STATUS_IGNORE);
    
            data.retrieve_data(ranges...);
        }
    
        template<are_only_views... Vs>
        auto recv(Source source, Tag tag, Vs... views)
        {
            Data data(Recv{}, _pool, views...);
    
            MPI_Recv(data.get_data(), data.get_count(), data.get_type(), source.get(), tag.get(), _mpi_comm, MPI_STATUS_IGNORE);
    
            data.retrieve_data(views...);
        }
    
        template<typename... Ts, are_only_ranges... Rs>
        auto recv(Source source, Tag tag, Pattern<Ts...> const& pattern, Rs&... ranges)
        {
            Data data(Recv{}, _pool, pattern, ranges...);
    
            MPI_Recv(data.get_data(), data.get_count(), data.get_type(), source.get(), tag.get(), _mpi_comm, MPI_STATUS_IGNORE);
    
            data.retrieve_data(pattern , ranges...);
        }
    
        template<typename... Ts, are_only_views... Vs>
        auto recv(Source source, Tag tag, Pattern<Ts...> const& pattern, Vs... views)
        {
            Data data(Recv{}, _pool, pattern, views...);
    
            MPI_Recv(data.get_data(), data.get_count(), data.get_type(), source.get(), tag.get(), _mpi_comm, MPI_STATUS_IGNORE);
    
            data.retrieve_data(pattern , views...);
        }

        template<typename... Ts>
            requires are_fundamentals<Ts...>  || are_only_ranges<Ts...>
        auto isend(Destination destination, Tag tag, Ts&... ranges)
        {
            // we first need to create the Data object
            // then we can link the buffer of Data to the MPI function
            // afterwards we create the Request object and return it
            // this way we trigger the guaranteed copy elision, 
            // because the Request object is not used in this function
            MPI_Request mpi_request;
            Data data(Send{}, _pool, ranges...);

            MPI_Isend(data.get_data(), data.get_count(), data.get_type(), destination.get(), tag.get(), _mpi_comm, &mpi_request);

            return Request(Send{}, std::move(data), std::move(mpi_request), ranges...);
        }

        template<are_only_views... Vs>
        auto isend(Destination destination, Tag tag, Vs... views)
        {
            MPI_Request mpi_request;
            Data data(Send{}, _pool, views...);

            MPI_Isend(data.get_data(), data.get_count(), data.get_type(), destination.get(), tag.get(), _mpi_comm, &mpi_request);

            return Request(Send{}, std::move(data), std::move(mpi_request), views...);
        }

        template<typename... Ts, typename... Rs>
            requires are_fundamentals<Rs...>  || are_only_ranges<Rs...>
        auto isend(Destination destination, Tag tag, Pattern<Ts...> const& pattern, Rs&... ranges)
        {
            MPI_Request mpi_request;
            Data data(Send{}, _pool, pattern, ranges...);

            MPI_Isend(data.get_data(), data.get_count(), data.get_type(), destination.get(), tag.get(), _mpi_comm, &mpi_request);

            return Request(Send{}, std::move(data), std::move(mpi_request), std::move(pattern), ranges...);
        }

        template<typename... Ts, are_only_views... Vs>
        auto isend(Destination destination, Tag tag, Pattern<Ts...> const& pattern, Vs... views)
        {
            MPI_Request mpi_request;
            Data data(Send{}, _pool, pattern, views...);

            MPI_Isend(data.get_data(), data.get_count(), data.get_type(), destination.get(), tag.get(), _mpi_comm, &mpi_request);

            return Request(Send{}, std::move(data), std::move(mpi_request), std::move(pattern), views...);
        }

        template<typename... Ts>
            requires are_fundamentals<Ts...> || are_only_ranges<Ts...>
        auto irecv(Source source, Tag tag, Ts&... ranges)
        {
            // we first need to create the Data object
            // then we can link the buffer of Data to the MPI function
            // afterwards we create the Request object and return it
            // this way we trigger the guaranteed copy elision, 
            // because the Request object is not used in this function
            MPI_Request mpi_request;
            Data data(Recv{}, _pool, ranges...);

            MPI_Irecv(data.get_data(), data.get_count(), data.get_type(), source.get(), tag.get(), _mpi_comm, &mpi_request);

            return Request(Recv{}, std::move(data), std::move(mpi_request), ranges...);
        }

        template<are_only_views... Vs>
        auto irecv(Source source, Tag tag, Vs... views)
        {
            MPI_Request mpi_request;
            Data data(Recv{}, _pool, views...);

            MPI_Irecv(data.get_data(), data.get_count(), data.get_type(), source.get(), tag.get(), _mpi_comm, &mpi_request);

            return Request(Recv{}, std::move(data), std::move(mpi_request), views...);
        }

        template<typename... Ts, typename... Rs>
            requires are_fundamentals<Rs...> || are_only_ranges<Rs...>
        auto irecv(Source source, Tag tag, Pattern<Ts...> const& pattern, Rs&... ranges)
        {
            MPI_Request mpi_request;
            Data data(Recv{}, _pool, pattern, ranges...);

            MPI_Irecv(data.get_data(), data.get_count(), data.get_type(), source.get(), tag.get(), _mpi_comm, &mpi_request);

            return Request(Recv{}, std::move(data), std::move(mpi_request), std::move(pattern), ranges...);
        }

        template<typename... Ts, are_only_views... Vs>
        auto irecv(Source source, Tag tag, Pattern<Ts...> const& pattern, Vs... views)
        {
            MPI_Request mpi_request;
            Data data(Recv{}, _pool, pattern, views...);

            MPI_Irecv(data.get_data(), data.get_count(), data.get_type(), source.get(), tag.get(), _mpi_comm, &mpi_request);

            return Request(Recv{}, std::move(data), std::move(mpi_request), std::move(pattern), views...);
        }

        auto wait(Request& request)
        {
            MPI_Wait(&request.get(), MPI_STATUS_IGNORE);
            request.retrieve_data();
        }

        // template<typename C>
        // auto waitall(C& requests)
        // {
        //     std::vector<bool> completed(requests.size(), false);
        //     bool done = false; 

        //     while (!done)
        //     {
        //         done = true;

        //         for (size_t i = 0; i < requests.size(); i++)
        //         {
        //             if (completed[i])
        //                 continue;
    
        //             int test;
        //             MPI_Test(&requests[i].get(), &test, MPI_STATUS_IGNORE);
                    
        //             if (test)
        //             {
        //                 completed[i] = true;
        //                 requests[i].retrieve_data();
        //             }
        //             else
        //                 done = false;
        //         }
        //     }
        // }
        template<typename C>
        auto waitall(C& requests)
        {
            for (auto& request : requests)
            {
                MPI_Wait(&request.get(), MPI_STATUS_IGNORE);
                request.retrieve_data();
            }
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
};

#endif

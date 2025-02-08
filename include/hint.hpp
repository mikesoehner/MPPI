#ifndef MPPI_HINT_H
#define MPPI_HINT_H

#include <type_traits>
#include <array>
#include <utility>


enum class Hint_Options
{
    shared_file_timeout, 
    rwlock_timeout, 
    noncoll_read_bufsize,
    noncoll_write_bufsize,
    coll_read_bufsize,
    coll_write_bufsize,
    mpiio_concurrency,
    mpiio_coll_contiguous
};


class Hint
{
public:
    Hint() = default;

    Hint(Hint_Options hint, const char* value)
        : _value(value)
    {
        switch (hint)
        {
        case Hint_Options::shared_file_timeout:
            _key = "shared_file_timeout";
            break;
        case Hint_Options::rwlock_timeout:
            _key = "rwlock_timeout";
            break;
        case Hint_Options::noncoll_read_bufsize:
            _key = "noncoll_read_bufsize";
            break;
        case Hint_Options::noncoll_write_bufsize:
            _key = "noncoll_write_bufsize";
            break;
        case Hint_Options::coll_read_bufsize:
            _key = "coll_read_bufsize";
            break;
        case Hint_Options::coll_write_bufsize:
            _key = "coll_write_bufsize";
            break;
        case Hint_Options::mpiio_concurrency:
            _key = "mpiio_concurrency";
            break;
        case Hint_Options::mpiio_coll_contiguous:
            _key = "mpiio_coll_contiguous";
            break;
        }
    }

    const char* get_key() const
    {
        return _key;
    }

    const char* get_value() const
    {
        return _value;
    }

private:
    const char* _key = nullptr;
    const char* _value = nullptr;
};

template <typename...> 
struct all_hints;

template <> 
struct all_hints<> : std::true_type 
{};

template <typename T, typename... Rest> 
struct all_hints<T, Rest...> : std::integral_constant<bool, std::is_same_v<T, Hint> && all_hints<Rest...>::value>
{};

template< typename... Ts>
concept are_hints =
   ( all_hints<Ts...>::value );


template<typename... Hints> 
    requires are_hints<Hints...>
class Info
{
public:
    Info(Hints... hints) 
    {
        fill_array(0, hints...);
    }

    MPI_Info get_info() const
    {
        MPI_Info mpi_info = MPI_INFO_NULL;

        if (sizeof...(Hints) > 0)
        {
            MPI_Info_create(&mpi_info);

            for (auto const info : _infos)
                MPI_Info_set(mpi_info, info.get_key(), info.get_value());
        }

        return mpi_info;
    }

private:

   template<typename T>
    void fill_array(size_t index, T head)
    {
        _infos[index] = head;
    }

    template<typename T, typename... Ts>
    void fill_array(size_t index, T head, Ts... tail)
    {
        _infos[index++] = head;
        fill_array(index, tail...);
    }

    std::array<Hint, sizeof...(Hints)> _infos;
};

#endif
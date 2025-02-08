#ifndef MPPI_AMODES_H
#define MPPI_AMODES_H

#include <type_traits>
#include <concepts>
#include "mpi.h"


enum class AccessMode
{
    MODE_APPEND = 128,
    MODE_CREATE = 1,
    MODE_DELETE_ON_CLOSE = 16,
    MODE_EXCL = 64,
    MODE_RDONLY = 2,
    MODE_RDWR = 8,
    MODE_SEQUENTIAL = 256,
    MODE_WRONLY = 4,
    MODE_UNIQUE_OPEN = 32
};

struct MODE_APPEND { static inline int value = 128; };
struct MODE_CREATE { static inline int value = 1; };
struct MODE_DELETE_ON_CLOSE { static inline int value = 16; };
struct MODE_EXCL { static inline int value = 64; };
struct MODE_RDONLY { static inline int value = 2; };
struct MODE_RDWR { static inline int value = 8; };
struct MODE_SEQUENTIAL { static inline int value = 256; };
struct MODE_WRONLY { static inline int value = 4; };
struct MODE_UNIQUE_OPEN { static inline int value = 32; };

using ModeAppend = MODE_APPEND;

template< typename T >
concept is_mode_create =
   ( std::is_same_v<T, MODE_CREATE> );

template< typename T >
concept is_mode_append =
   ( std::is_same_v<T, MODE_APPEND> );

template< typename T >
concept is_mode_rdonly =
   ( std::is_same_v<T, MODE_RDONLY> );

// template <typename...> 
// struct all_amodes;

// template <> 
// struct all_amodes<> : std::true_type 
// {};

// template <typename T, typename... Ts> 
// struct all_amodes<T, Ts...> : std::integral_constant<bool, (is_mode_create_v<T> || is_mode_append_v<T>) && all_amodes<Ts...>::value>
// {};

template< typename... Ts >
concept are_all_amodes =
   ( (is_mode_append<Ts> || is_mode_create<Ts> || is_mode_rdonly<Ts>) && ... );


// concept to detect if MPI_MODE_CREATE and MPI_MODE_RDONLY are both set
template< typename... Ts >
concept are_not_create_and_rdonly =
    ( !( (is_mode_create<Ts> || ...) && (is_mode_rdonly<Ts> || ...) ) );



template<typename... AccessModes> 
    requires are_all_amodes<AccessModes...> && are_not_create_and_rdonly<AccessModes...>
class AMode
{
public:
    AMode() = default;

    AMode(AccessModes... modes) 
    {
        calc_mode(modes...);
    }

    int get_amode() const { return _amode; }

private:

    template<typename T>
    void calc_mode(T head)
    {
        _amode |= T::value;
    }

    template<typename T, typename... Ts>
    void calc_mode(T head, Ts... tail)
    {
        _amode |= T::value;
        calc_mode(tail...); 
    }

    int _amode {0};
};


#endif
#ifndef MPPI_AMODES_H
#define MPPI_AMODES_H

#include <type_traits>
#include <concepts>
#include "mpi.h"


struct MODE_APPEND { static inline int value = 128; };
struct MODE_CREATE { static inline int value = 1; };
struct MODE_DELETE_ON_CLOSE { static inline int value = 16; };
struct MODE_EXCL { static inline int value = 64; };
struct MODE_RDONLY { static inline int value = 2; };
struct MODE_RDWR { static inline int value = 8; };
struct MODE_SEQUENTIAL { static inline int value = 256; };
struct MODE_WRONLY { static inline int value = 4; };
struct MODE_UNIQUE_OPEN { static inline int value = 32; };


template< typename T >
concept is_mode_append =
   ( std::is_same_v<T, MODE_APPEND> );

template< typename T >
concept is_mode_create =
   ( std::is_same_v<T, MODE_CREATE> );

template< typename T >
concept is_mode_delete_on_close =
   ( std::is_same_v<T, MODE_DELETE_ON_CLOSE> );

template< typename T >
concept is_mode_excl =
   ( std::is_same_v<T, MODE_EXCL> );

template< typename T >
concept is_mode_rdonly =
    ( std::is_same_v<T, MODE_RDONLY> );

template< typename T >
concept is_mode_rdwr =
    ( std::is_same_v<T, MODE_RDWR> );

template< typename T >
concept is_mode_sequential =
    ( std::is_same_v<T, MODE_SEQUENTIAL> );

template< typename T >
concept is_mode_wronly =
    ( std::is_same_v<T, MODE_WRONLY> );

template< typename T >
concept is_mode_unique_open =
    ( std::is_same_v<T, MODE_UNIQUE_OPEN> );


template< typename... Ts >
concept are_all_amodes =
   ( (is_mode_append<Ts> || 
        is_mode_create<Ts> || 
        is_mode_delete_on_close<Ts> ||
        is_mode_excl<Ts> ||
        is_mode_rdonly<Ts> ||
        is_mode_rdwr<Ts> ||
        is_mode_sequential<Ts> ||
        is_mode_wronly<Ts> ||
        is_mode_unique_open<Ts>) && ... );


// concept to detect if MPI_MODE_CREATE and MPI_MODE_RDONLY are both set
template< typename... Ts >
concept are_not_create_and_rdonly =
    ( !( (is_mode_create<Ts> || ...) && (is_mode_rdonly<Ts> || ...) ) );

// concept to detect of MPI_MODE_RDONLY and MPI_MODE_WRONLY are both set
template< typename... Ts >
concept are_wronly_and_rdonly =
    ( !( (is_mode_wronly<Ts> || ...) && (is_mode_rdonly<Ts> || ...) ) );



// Base condition: all template parameter need to be Modes
template<are_all_amodes... Modes>
    // Additional Condition: certain combination of Modes is not allowed
    requires are_not_create_and_rdonly<Modes...> && are_wronly_and_rdonly<Modes...>
class AccessMode
{
public:
    AccessMode() 
    {
        calc_mode(Modes{}...);
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
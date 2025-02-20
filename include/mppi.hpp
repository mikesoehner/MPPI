#ifndef MPPI_MPPI_H
#define MPPI_MPPI_H

#include "mpi.h"
#include "hint.hpp"
#include "amodes.hpp"
#include <array>
#include "datatype.hpp"
#include "communicator.hpp"


// template <typename...> 
// struct is_modes;

// template <> 
// struct is_modes<> : std::true_type 
// {};

// template <template <typename...> typename T, typename... Ts> 
// struct is_modes<T<Ts...>, Ts...> : std::is_same< T<Ts...>, AccessMode<Ts...> >
// {};

// template< typename Modes >
// concept is_mode =
//    ( is_modes<Modes>::value );

// template <typename...> 
// struct is_info;

// template <> 
// struct is_info<> : std::true_type 
// {};

// template <template <typename...> typename T, typename... Ts> 
// struct is_info<T<Ts...>, Ts...> : std::is_same< T<Ts...>, Info<Ts...> >
// {};


// // template to hold parameter pack
// template <typename...>
// struct TypeList {};

// base class, which is never used
template<typename Modes, typename Infos>
class File
{
public:
    File(const char *filename, Modes, Infos) = delete;
};

template<typename... Modes, typename... Infos>
class File< AccessMode< Modes... >, Info< Infos... > >
{
public:
    File(const char *filename, AccessMode<Modes...> modes, Info<Infos...> infos)
        : _modes(modes), _infos(infos)
    {
        auto amode = _modes.get_amode();
        auto info = _infos.get_info();

        MPI_File_open(MPI_COMM_WORLD, filename, amode, info, &_fh);
    }

    void seek() {}
    
    // template<typename... Ts>
    // void write(Data<Ts...> data) 
    // {
    //     MPI_File_write(_fh, data.get_data(), data.get_count(), data.get_type(), MPI_STATUS_IGNORE);
    // }

    // template<typename... Ts>
    // void write(Data<Ts...> data, MPI_Offset offset) 
    // {
    //     MPI_File_write_at(_fh, offset, data.get_data(), data.get_count(), data.get_type(), MPI_STATUS_IGNORE);
    // }

private:
    MPI_File _fh;
    AccessMode<Modes...> _modes;
    Info<Infos...> _infos;
};


#endif
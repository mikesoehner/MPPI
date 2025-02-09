#include <catch2/catch_test_macros.hpp>
#include "amodes.hpp"


TEST_CASE( "Access Modes functionality", "[amodes]" )
{

    SECTION("AccessMode creation with one Mode")
    {
        AccessMode<MODE_APPEND> amode_append;
        AccessMode<MODE_CREATE> amode_create;
        AccessMode<MODE_DELETE_ON_CLOSE> amode_delete_on_close;
        AccessMode<MODE_EXCL> amode_excl;
        AccessMode<MODE_RDONLY> amode_rdonly;
        AccessMode<MODE_RDWR> amode_rdwr;
        AccessMode<MODE_SEQUENTIAL> amode_seq;
        AccessMode<MODE_WRONLY> amode_wronly;
        AccessMode<MODE_UNIQUE_OPEN> amode_uniq_open;

        REQUIRE (amode_append.get_amode() == MPI_MODE_APPEND);
        REQUIRE (amode_create.get_amode() == MPI_MODE_CREATE);
        REQUIRE (amode_delete_on_close.get_amode() == MPI_MODE_DELETE_ON_CLOSE);
        REQUIRE (amode_excl.get_amode() ==  MPI_MODE_EXCL);
        REQUIRE (amode_rdonly.get_amode() == MPI_MODE_RDONLY);
        REQUIRE (amode_rdwr.get_amode() == MPI_MODE_RDWR);
        REQUIRE (amode_seq.get_amode() == MPI_MODE_SEQUENTIAL);
        REQUIRE (amode_wronly.get_amode() == MPI_MODE_WRONLY);
        REQUIRE (amode_uniq_open.get_amode() == MPI_MODE_UNIQUE_OPEN);
    }

    SECTION("AccessMode creation with two Modes")
    {
        AccessMode<MODE_APPEND, MODE_CREATE> amode_append_create;
        AccessMode<MODE_APPEND, MODE_DELETE_ON_CLOSE> amode_append_delete;
        AccessMode<MODE_SEQUENTIAL, MODE_UNIQUE_OPEN> amode_seq_uniq_open;

        REQUIRE (amode_append_create.get_amode() == MPI_MODE_APPEND + MPI_MODE_CREATE);
        REQUIRE (amode_append_delete.get_amode() == MPI_MODE_APPEND + MPI_MODE_DELETE_ON_CLOSE);
        REQUIRE (amode_seq_uniq_open.get_amode() == MPI_MODE_SEQUENTIAL + MPI_MODE_UNIQUE_OPEN);
    }
}
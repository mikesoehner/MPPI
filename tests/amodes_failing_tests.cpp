#include "amodes.hpp"


// These examples should not compile.
int main()
{
    AccessMode<MODE_CREATE, MODE_RDONLY> amode_create_rdonly;
    AccessMode<MODE_RDONLY, MODE_WRONLY> amode_rdonly_wronly;

    return 0;
}
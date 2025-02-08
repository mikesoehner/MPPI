#include <iostream>
#include "mppi.hpp"
#include <unistd.h>

/* =========================================================================
*
* Function : double_buffer
*
* Synopsis :
* void double_buffer (
* MPI_File fh , ** IN
* MPI_Datatype buftype , ** IN
* int bufcount ** IN
* )
*
* Description :
* Performs the steps to overlap computation with a collective write
* by using a double - buffering technique .
*
* Parameters :
* fh previously opened MPI file handle
* buftype MPI datatype for memory layout
* ( Assumes a compatible view has been set on fh )
* bufcount # buftype elements to transfer
*------------------------------------------------------------------------ */

/* this macro switches which buffer "x" is pointing to */
// #define TOGGLE_PTR (float* x) ((( x )==( buffer1 )) ? (x= buffer2 ) : (x= buffer1 ))
void toggle_ptr(float* x, float* buffer1, float* buffer2)
{
    ((( x )==( buffer1 )) ? (x= buffer2 ) : (x= buffer1 ));
}


void compute_buffer(float* compute_buf_ptr, int bufcount, int* done)
{

}

void double_buffer ( MPI_File fh , MPI_Datatype buftype , int bufcount )
{
    MPI_Status status ; /* status for MPI calls */
    float * buffer1 , * buffer2 ; /* buffers to hold results */
    float * compute_buf_ptr ; /* destination buffer */
    /* for computing */
    float * write_buf_ptr ; /* source for writing */
    int done ; /* determines when to quit */
    /* buffer initialization */
    buffer1 = ( float *) malloc ( bufcount * sizeof ( float ));
    buffer2 = ( float *) malloc ( bufcount * sizeof ( float ));
    compute_buf_ptr = buffer1 ; /* initially point to buffer1 */
    write_buf_ptr = buffer1 ; /* initially point to buffer1 */
    /* DOUBLE - BUFFER prolog :
    * compute buffer1 ; then initiate writing buffer1 to disk
    */
    compute_buffer ( compute_buf_ptr , bufcount , &done );
    MPI_File_write_all_begin (fh , write_buf_ptr , bufcount , buftype );
    /* DOUBLE - BUFFER steady state :
    * Overlap writing old results from buffer pointed to by write_buf_ptr
    * with computing new results into buffer pointed to by compute_buf_ptr .
    *
    * There is always one write - buffer and one compute - buffer in use
    * during steady state .
    */
    while (! done ) {
        toggle_ptr ( compute_buf_ptr, buffer1 , buffer2 );
        compute_buffer ( compute_buf_ptr , bufcount , & done );
        MPI_File_write_all_end (fh , write_buf_ptr , & status );
        toggle_ptr ( write_buf_ptr, buffer1 , buffer2 );
        MPI_File_write_all_begin (fh , write_buf_ptr , bufcount , buftype );
    }
    /* DOUBLE - BUFFER epilog :
    * wait for final write to complete .
    */
    MPI_File_write_all_end (fh , write_buf_ptr , & status );
    /* buffer cleanup */
    free ( buffer1 );
    free ( buffer2 );
}

// TODO: Make interface typesafe and switcheroo of parameters-safe.
// Prototype with some internal representation and make a normal MPI_Send with MPI_BYTE
// to circumvent datatype engine -> typesafe comm without datatype engine overhead

template<typename T>
void print_type(T t) { std::cout << "Type: " << typeid(std::ranges::range_value_t<T>).name() << std::endl; }

int main(int argc, char** argv)
{
    MPI_Init(&argc, &argv);

	// MPI_File fh;
    // MPI_File_open(MPI_COMM_WORLD, "output.txt", MPI_MODE_RDWR | MPI_MODE_APPEND | MPI_MODE_CREATE, MPI_INFO_NULL, &fh);

	if (false)
	{
		int rank;
		MPI_Comm_rank(MPI_COMM_WORLD, &rank);
		// if (rank == 0)
		{
			volatile int i = 0;
			char hostname[256];
			gethostname(hostname, sizeof(hostname));
			{
				printf("PID %d on %s with rank %d ready for attach\n", getpid(), hostname, rank);
				fflush(stdout);
				while (0 == i)
					sleep(5);
			}
		}
	}
    Hint hint0(Hint_Options::coll_read_bufsize, "6514161");
    Hint hint1(Hint_Options::coll_read_bufsize, "16777216");
    Info info(hint0, hint1, Hint(Hint_Options::mpiio_concurrency, "enable"));
    
    AMode<MODE_APPEND, MODE_CREATE> amode;

    File file("output.txt", amode, info);

    Communicator comm;

    std::vector<int> data(4);

    print_type(data | std::views::take(2));
    print_type(data | std::views::drop(3));

    std::cout << "types same?: " << free_are_same_types(data | std::views::take(2), data | std::views::drop(3)) << std::endl;
    std::cout << "concept?: " << are_same_types<decltype(data | std::views::take(2)), decltype(data | std::views::drop(3))> << std::endl;

    if (comm.get_rank() == 0)
        data = {1,2,3,4};

    if (comm.get_rank() == 0)
    {
        Data pattern(data | std::views::take(2), data | std::views::drop(3));
        comm.send(pattern, 1, Tag(0));
    }
    else
    {
        comm.recv(0, Tag(0), data | std::views::take(2), data | std::views::drop(3));
    }

    MPI_File fh;
    int buf[1000] = {0};
    int rank;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_File_open(MPI_COMM_WORLD, "btest.out", MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &fh);
    if (rank == 0)
        MPI_File_write(fh, buf, 4, MPI_BYTE, MPI_STATUS_IGNORE);
    
    MPI_File_close(&fh);

    MPI_Finalize();
    return 0;
}
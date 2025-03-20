# TODOs

1. Make sure the compiler optimizes the serialization of datatype members to the vector of bytes.
    1. Make sure everything is as much compile-time optimized as possible (maybe rewrite functions f.e. make mod calculations constexpr).
    2. Is the array of displacements in the DataPattern class resolved at compile-time? Maybe another construct would be more fitting. In theory all the information is there at compile-time.
2. Consider the overlapping the sending and splitting of large messages.
    1. Find out OpenMPIs thresholds for that
    2. Change of interface is probably necessary (currently data is packed completed until the communicator does something with it).
3. Look at other serialization libraries
    1. boost
    2. madness
    3. cereal
4. Make custom benchmarks "nicer".
5. Think about more restricting concepts and specializations (f.e. consecutive and non-consecutive memory containers). 
6. Give option to give the communicator a buffer that it can use for the memory pool.
7. Look at Rust type interface stuff?
8. Link-time optimization?
9. Look at other C++ MPI implementations: EnhancedMPI, ChronosMPI.
10. Read Josephs paper from last year about derived datatypes.
11. Change the "include" directory name to mppi, to be more expressive.
12. MPI_CHAR vs MPI_BYTE
13. Check for max bandwidth of 1 core (with stream?). How much of that do we get?
14. Compile-time send size feature?
15. (CUDA/HIP/)OpenMP device support?
16. Implement reflection datapattern with experimental clang compiler.
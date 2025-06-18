# MPPI

We address the limitations and complexities of MPI's legacy C API with a modern C++ interface that leverages contemporary language features. One of our goals is to demonstrate how recent advancements in C++ can be effectively applied to improve MPI’s datatype and buffer handling, resulting in a more expressive, type-safe, and less error-prone programming model. We have implemented our ideas in this prototype implementation - MPPI.


## Getting Started

### Prerequisites

The main version of MPPI requires:
* a C++23 compatible compiler.
* an MPI-3.1 compliant MPI library.

The experimental reflection version of MPPI requires:
* a compiler capable of experimental reflection according to this [draft](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p2996r12.html) (we use this [compiler](https://github.com/bloomberg/clang-p2996)).
* an MPI-3.1 compliant MPI library.

Additionally, for the tests:
* a Catch2 installation.


### Installing

To build, test and install the MPPI library execute
```shell
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/my/install/prefix ..
make [-j N] 
make install
```

## Notes

The main branch of MPPI uses ```std::move_only_function``` which is not available yet in clang's libc++. The compiler we used for the experimental reflection version requires libc++ and utilizes a workaround that uses ```std::function``` instead of ```std::move_only_function```. 

The ```Communicator``` class combines ```std::pmr::monotonic_buffer_resource``` and ```std::pmr::unsynchronized_pool_resource``` to create a memory pool. The maximum size of an allocation that will be part of this pool is compiler-dependent, but current gcc and clang compilers set it to 4 MB. Larger allocations will still allocate memory but not reuse or free it until the end of the program, leading to memory waste. Running the benchmarks with a buffer size larger than 4 MB can quickly lead to an out-of-memory situation. Clang's libc++ sets the upper bound for an allocation size to 1 GB, which makes working with the memory pool more convenient.

## Contact

Mike Söhner <mike.soehner@hlrs.de>

Christoph Niethammer <niethammer@hlrs.de>
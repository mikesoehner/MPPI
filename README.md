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

## Contact

Mike Söhner <mike.soehner@hlrs.de>

Christoph Niethammer <niethammer@hlrs.de>
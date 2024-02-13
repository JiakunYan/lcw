# Lightweight Communication Wrapper (LCW)
A flexible communication wrapper layer.

## Authors

- \@JiakunYan (jiakunyan1998@gmail.com)

## Overview

The Lightweight Communication Wrapper (LCW) is designed to be a common denominator of multiple communication 
libraries. It is originated as a minimum communication interface to efficiently support 
Asynchronous Many-Task Systems (AMTs) while with the expectation to also be applicable to a broad array of
applications. It can also be viewed as a testbed to evaluate different communication libraries in the contexts
of multithreaded, irregular, fine-grained communications. The major features of its API includes
- Communication backend selection during runtime.
- The communication device abstraction.
- One-sided dynamic put primitives.
- Two-sided send/recv primitives.
- Queue-based completion notification mechanisms.
- Explicit progress functions.

The actual API and (some) documentation are located in [lcw.hpp](src/api/lcw.hpp).

Currently, it has the following communication backend implementation
- [MPI](https://www.mpi-forum.org/)
- [LCI](https://github.com/uiuc-hpc/LC) 

Currently, it supports the following clients
- [HPX](https://github.com/STEllAR-GROUP/hpx) (The HPX/LCW backend sits in a 
  [special fork branch](https://github.com/uiuc-hpc/hpx/tree/lcw-pp). It has 
  not been merged to the HPX master branch.)

## Installation
LCW depends on LCT (The Lightweight Communication Tools), which is co-located with 
[LCI](https://github.com/uiuc-hpc/LC) (Check LCI's `LCI_WITH_LCT_ONLY` cmake variable).
LCT does not have any special dependents and should be easy to install.

After that,
```
cmake .
make
make install
```

### Important CMake variables
- `CMAKE_INSTALL_PREFIX=/path/to/install`: Where to install LCW
  - This is the same across all the cmake projects.
- `LCW_DEBUG=ON/OFF`: Enable/disable the debug mode (more assertions and logs).
  The default value is `OFF`.
- `LCW_TRY_ENABLE_BACKEND_LCI`: whether to try to find LCI and, if found, enable the LCI backend.
  The default value is `ON`.
- `LCW_TRY_ENABLE_BACKEND_MPI`: whether to try to find MPI and, if found, enable the MPI backend.
  The default value is `ON`.

## Run LCI applications

We use the same mechanisms as MPI to launch LCW processes, so you can use the same way
you run MPI applications to run LCW applications. Typically, it would be `mpirun` or
`srun`. For example,
```
mpirun -n 2 ./hello_world
```
or
```
srun -n 2 ./hello_world
```

## Write an LCI program

See [examples](examples) for some example code.

See [lcw.hpp](src/api/lcw.hpp) for public APIs.
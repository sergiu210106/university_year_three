# MPI lab cheat sheet

MPI (Message Passing Interface) lets separate processes communicate by sending messages. Every process runs the same program, has its own rank, and can follow a different path based on that rank. This lab uses C and Open MPI on macOS.

## Install and check Open MPI on macOS

```sh
brew install open-mpi
brew info open-mpi
brew --prefix open-mpi
command -v mpicc
command -v mpirun
mpicc --showme:version
mpicc --showme:incdirs
```

Homebrew installs the current formula version; there is usually no need to download and build an old Open MPI release manually. On Apple Silicon, Homebrew's prefix is usually `/opt/homebrew`; on Intel Macs it is often `/usr/local`.

## Compile and run

Use the MPI compiler wrapper so the compiler receives the correct MPI header and library flags:

```sh
# C
mpicc -std=c17 -Wall -Wextra -O2 main.c -o main
```

Run with the requested number of processes (`-np` and `-n` are common spellings):

```sh
mpirun -np 4 ./main
mpiexec -n 4 ./main
```

Useful Open MPI launcher options:

| Option | Use |
|---|---|
| `--oversubscribe` | Allow more local processes than available CPU slots. |
| `--host host1,host2` | Select hosts for a multi-machine run. |
| `--tag-output` | Prefix output lines with the process rank. |
| `--output-filename run-output` | Write process output to files under the given prefix. |
| `--map-by core` | Map processes to cores. |
| `--bind-to core` | Bind each process to a core. |
| `--help` | Show launcher options. |

Useful installation and runtime diagnostics:

```sh
ompi_info --version
ompi_info --all                 # Detailed Open MPI build/configuration info
mpirun --display-map -np 4 ./main
mpirun --tag-output -np 4 ./main
```

This lab's script builds and runs `main.c` with two processes by default:

```sh
./6/run.sh       # default: 2 processes
./6/run.sh 4     # 4 processes
```

If a command is run from inside `6/`, use `./run.sh` instead. To run more processes than available CPU slots with Open MPI, add `--oversubscribe`:

```sh
mpirun --oversubscribe -np 16 ./main
```

For a multi-machine run, Open MPI also accepts a host list, for example `mpirun --host node1,node2 -np 4 ./main`; the machines need compatible MPI installations and working SSH/network configuration.

## Minimal C program

```c
#include <mpi.h>
#include <stdio.h>

int main(int argc, char **argv) {
    int rank, process_count;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);      // This process's ID: 0..size-1
    MPI_Comm_size(MPI_COMM_WORLD, &process_count);

    printf("Hello from %d of %d\n", rank, process_count);

    MPI_Finalize();                            // Finish MPI before returning
    return 0;
}
```

`MPI_COMM_WORLD` is the communicator containing all processes started for the job. Rank zero is often used for input and final output, but it is an ordinary process and can also compute.

## Lifecycle and process information

| Routine | Purpose |
|---|---|
| `MPI_Init(&argc, &argv)` | Initialize MPI; call before other MPI routines. |
| `MPI_Comm_rank(comm, &rank)` | Get this process's rank in a communicator. |
| `MPI_Comm_size(comm, &size)` | Get the number of processes in a communicator. |
| `MPI_Finalize()` | Shut down MPI; call once after MPI work is complete. |
| `MPI_Abort(comm, error_code)` | Terminate a communicator's job after an unrecoverable error. |

For a program that uses threads as well as MPI, use `MPI_Init_thread` and check the provided thread-support level. Do not assume that every MPI build permits arbitrary MPI calls from every thread.

## Point-to-point communication

`MPI_Send` and `MPI_Recv` communicate between two ranks. The sender and receiver must agree on communicator, source/destination, tag, datatype, and compatible element counts.

```c
const int tag = 7;
if (rank == 0) {
    int value = 42;
    MPI_Send(&value, 1, MPI_INT, 1, tag, MPI_COMM_WORLD);
} else if (rank == 1) {
    int value = 0;
    MPI_Recv(&value, 1, MPI_INT, 0, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    printf("received %d\n", value);
}
```

General forms:

```c
MPI_Send(buffer, count, datatype, destination, tag, communicator);
MPI_Recv(buffer, count, datatype, source, tag, communicator, &status);
```

Common routines:

| Routine | Purpose |
|---|---|
| `MPI_Send` / `MPI_Recv` | Blocking send / receive. |
| `MPI_Ssend` | Synchronous blocking send; completes when the matching receive starts. |
| `MPI_Isend` / `MPI_Irecv` | Start a nonblocking send / receive; complete it with a wait/test call. |
| `MPI_Sendrecv` | Exchange data in one call; useful for neighbor exchanges. |
| `MPI_Probe` / `MPI_Get_count` | Inspect an incoming message and determine its element count. |
| `MPI_Barrier(comm)` | Wait until every process in the communicator reaches the barrier. |

`MPI_ANY_SOURCE` and `MPI_ANY_TAG` can be used as receive wildcards. Use `MPI_STATUS_IGNORE` if the receive status is not needed. Blocking sends can deadlock if every process waits in a send before any matching receive is posted; use a safe communication order, `MPI_Sendrecv`, or nonblocking communication.

For nonblocking calls, keep each buffer valid and unchanged (for a send) or unused (for a receive) until the request completes:

```c
MPI_Request request;
MPI_Isend(data, count, MPI_DOUBLE, destination, tag, MPI_COMM_WORLD, &request);
// Do independent work here.
MPI_Wait(&request, MPI_STATUS_IGNORE);
```

Other completion routines include `MPI_Waitall`, `MPI_Test`, and `MPI_Waitany`.

## Collective communication

All processes in a communicator must call a collective routine in a compatible order. Collective calls are not interchangeable with a barrier: many also move or combine data.

| Routine | Operation |
|---|---|
| `MPI_Bcast` | One root sends the same data to every process. |
| `MPI_Scatter` | Root distributes equal-sized chunks to processes. |
| `MPI_Gather` | Root collects equal-sized chunks from processes. |
| `MPI_Allgather` | Every process collects each process's chunk. |
| `MPI_Alltoall` | Every process sends a chunk to every other process. |
| `MPI_Reduce` | Combine values and return the result to a root. |
| `MPI_Allreduce` | Combine values and return the result to every process. |
| `MPI_Scan` / `MPI_Exscan` | Inclusive / exclusive prefix reduction by rank. |

Examples:

```c
// Broadcast one integer from rank 0 to all ranks.
MPI_Bcast(&value, 1, MPI_INT, 0, MPI_COMM_WORLD);

// Sum one local value and leave the total on rank 0.
MPI_Reduce(&local_sum, &total, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

// Sum one local value and leave the total on every rank.
MPI_Allreduce(&local_sum, &total, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
```

Common predefined reduction operators include `MPI_SUM`, `MPI_PROD`, `MPI_MAX`, `MPI_MIN`, `MPI_LAND`, and `MPI_LOR`. For variable chunk sizes or displacements, use `MPI_Scatterv` and `MPI_Gatherv`.

## Common MPI datatypes

| C value | MPI datatype |
|---|---|
| `char` | `MPI_CHAR` |
| `int` | `MPI_INT` |
| `long` | `MPI_LONG` |
| `long long` | `MPI_LONG_LONG_INT` |
| `float` | `MPI_FLOAT` |
| `double` | `MPI_DOUBLE` |
| `unsigned char` | `MPI_UNSIGNED_CHAR` |

For fixed-width integer types such as `int32_t`, use the matching MPI fixed-width type where available (for example `MPI_INT32_T`). MPI datatypes describe the transmitted representation; do not assume `long` has the same width on every platform.

## Timing and checks

```c
MPI_Barrier(MPI_COMM_WORLD);       // Optional: synchronize before timing
double start = MPI_Wtime();
// Work to measure
double elapsed = MPI_Wtime() - start;
double slowest = 0.0;
MPI_Reduce(&elapsed, &slowest, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
if (rank == 0) {
    printf("maximum elapsed seconds: %f\n", slowest);
}
```

Use the maximum elapsed time across ranks as a simple measure of parallel completion time. `MPI_Wtime` returns a local wall-clock timer; do not subtract timestamps measured on different machines.

## Troubleshooting

- **`mpi.h` not found by the compiler:** compile with `mpicc`, not plain `clang` or `gcc`.
- **`mpi.h` not found by VS Code:** set the C/C++ IntelliSense compiler to `mpicc` and add the Open MPI include directory. On this Apple Silicon setup it is `/opt/homebrew/opt/open-mpi/include`; check the active path with `mpicc --showme:incdirs`.
- **`mpirun` cannot start processes:** check `mpirun --version`, use `mpirun -np 2 ./main`, and check whether a system security or network policy blocks Open MPI's local communication sockets.
- **Too many processes for available slots:** reduce `-np` or, for a local Open MPI run, add `--oversubscribe`.
- **Program hangs:** check that every receive has a matching send (same communicator, tag, and compatible datatype/count), that all ranks enter collectives in the same order, and that all nonblocking requests are completed.
- **Output appears in a different order:** processes execute concurrently; rank order is not guaranteed. Gather output to one rank or sort it afterward if ordered output is needed.
- **Different ranks print the same result:** verify that each process uses its rank to select its input and work partition.

## Quick reference

```sh
brew install open-mpi
mpicc -std=c17 -Wall -Wextra -O2 main.c -o main
mpirun -np 4 ./main
mpirun --oversubscribe -np 16 ./main
mpirun --version
mpicc --showme:version
mpicc --showme:incdirs
```

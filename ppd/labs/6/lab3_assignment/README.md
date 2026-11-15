# Lab 3: MPI large-number addition

C++11 implementations of Variants 0, 1, 1.1, 2, and 3 from [the assignment](task.md). All digit buffers and MPI request arrays have fixed capacity and automatic storage duration.

For learning and presentation preparation, start with the [complete study guide](study_guide.md). It explains every function, MPI call, and script, with worked examples and practice questions.

## Build and run

From the labs workspace:

```bash
./6/lab3_assignment/scripts/build.sh
./6/lab3_assignment/scripts/run_tests.sh
```

Dependencies: Open MPI (`mpic++`, `mpirun`), Bash, Python 3, `timeout`, `awk`, and `cmp`. The build uses `mpic++ -std=c++11 -O2 -Wall -Wextra -pedantic`. Generated inputs, outputs, executables, and compilation commands stay in ignored `build/`.

After the test script creates the supplied inputs:

```bash
cd 6/lab3_assignment
./scripts/run.sh seq build/inputs/supplied_1.txt build/inputs/supplied_2.txt build/reference.txt
./scripts/run.sh mpi1 5 build/inputs/supplied_1.txt build/inputs/supplied_2.txt build/result.txt
./scripts/run.sh mpi11 5 build/inputs/supplied_1.txt build/inputs/supplied_2.txt build/result.txt
./scripts/run.sh mpi2 4 build/inputs/supplied_1.txt build/inputs/supplied_2.txt build/result.txt
./scripts/run.sh mpi3 5 build/inputs/supplied_1.txt build/inputs/supplied_2.txt build/result.txt
```

Each executable accepts `[input1 [input2 [output]]]`, defaulting to `Numar1.txt`, `Numar2.txt`, and `Numar3.txt` relative to the current directory. `run.sh` defaults to five processes for MPI variants and passes `--oversubscribe`. Set `MPICXX`, `MPIEXEC`, `TIMEOUT`, or `TIMEOUT_SECONDS` to override the corresponding tools or the test timeout of 30 seconds. MPI needs permission to create local sockets; the recorded suite ran outside the socket-restricted sandbox.

For direct execution:

```bash
./build/seq
mpirun --oversubscribe -np 5 ./build/mpi1
```

Every program prints one total elapsed time in milliseconds. MPI timing starts after initialization and an initial barrier, includes input, communication, computation, output close, and a completion barrier, then reports the maximum elapsed time across ranks. The final timing reduction, MPI initialization/finalization, launcher startup, input generation, and external comparisons are excluded. Sequential timing covers reading through output close using `high_resolution_clock`. Closing the output flushes the C++ stream; it does not force an OS disk synchronization.

## Files and limits

Input files contain a count followed by whitespace-separated decimal digits, least significant first. For example, `123` is stored as:

```text
3
3 2 1
```

The shorter operand is padded with zeroes. Output preserves the larger input's declared width, including leading zeroes, and adds one digit only when the final carry is nonzero. All outputs use a six-character, space-padded count header, one space after each digit, and a final newline. This lets the streaming implementations rewrite the count without moving digits and makes every variant byte-identical.

Operands may contain 1 through 100,000 digits. Variants 1, 1.1, and 3 support 2 through 17 processes; Variant 2 supports 1 through 17. Lengths and process counts are checked before buffer use. `CAPACITY = 100017` reserves space for at most 16 collective padding digits and one final carry. There are no variable-length, heap-allocated, global, or static digit or request buffers. The largest digit-buffer footprint is Variant 2's six local arrays, totaling 600,102 bytes per process, plus small stream and MPI bookkeeping objects.

VS Code uses `.vscode/c_cpp_properties.json` with the installed MPI wrapper and include directory. The build generates six C++11 compilation entries in `build/compile_commands.json`. Files outside this assignment retain the C++20 fallback.

## Implementations

| Executable | Algorithm |
| --- | --- |
| `seq` | Read both operands, add from the least significant digit with a carry, write the result. |
| `mpi1` | Rank 0 reads and sends one slice at a time. Workers receive both operands before receiving their predecessor's carry, add, forward the carry, and return their slice. Rank 0 writes each returned slice in rank order. |
| `mpi11` | Same distribution and collection, but workers add with carry zero before waiting for the real carry, then increment their local result if necessary. |
| `mpi2` | Rank 0 reads and zero-pads to a multiple of the process count. All ranks participate using two scatters, ordered carry propagation, and a gather. Output discards distribution padding while retaining any true carry. |
| `mpi3` | Rank 0 incrementally reads disjoint slices and posts nonblocking sends and result receives. Workers post nonblocking operand and carry receives, wait, add, and send their carry and result asynchronously. All requests complete before buffers leave scope. |

Variants 1, 1.1, and 3 use quotient/remainder partitioning over `p-1` workers. The first `N % (p-1)` workers receive one extra digit, so every digit is covered exactly once. Variant 3's assignment says `N/p` while reserving rank 0 for I/O. We interpret this as distributing all `N` digits across the `p-1` workers, rather than leaving a slice unprocessed. Rank 0 performs no digit addition in these variants.

## Verification and measurements

The code was reviewed against the assignment before the final suite. The review checked capacities, slice coverage, carry boundaries, incremental I/O, synchronous-send safety, nonblocking lifetimes, and readability. No blocking findings remained. The key review conclusions and deadlock argument are in [the study notes](information.md).

Only the required cases were run, once per configuration:

| Variants | Operand lengths | Processes |
| --- | --- | --- |
| 1, 1.1, 3 | Supplied 16 and 16 | 5 |
| 1, 1.1, 3 | 10,000 and 10,000 | 5, 9, 17 |
| 1, 1.1, 3 | 100 and 100,000 | 5, 9, 17 |
| 2 | Supplied 16 and 16 | 4 |
| 2 | 1,000 and 1,000 | 4, 8, 16 |
| 2 | 100 and 100,000 | 4, 8, 16 |

Variants 1 and 1.1 repeat their seven configurations using `MPI_Ssend` for every blocking send. All 28 standard runs and 14 synchronous runs passed `cmp` against four sequential reference outputs. No run reached the 30-second timeout. No additional edge-case, invalid-input, independent-oracle, or sanitizer suites were run.

Random inputs are generated once per operand per suite and reused for every applicable configuration. The generator uses `std::mt19937`, modulo-10 digits, and a nonzero most significant digit. Fixed seeds are:

| Case | Operand 1 seed | Operand 2 seed |
| --- | --- | --- |
| 1,000 and 1,000 | 3101 | 3102 |
| 10,000 and 10,000 | 3201 | 3202 |
| 100 and 100,000 | 3301 | 3302 |

Recorded on 2026-11-09 with macOS arm64, 14 logical CPUs, Apple Clang 21.0.0, and Open MPI 5.0.11. Counts of 16 and 17 processes exceed the logical CPU count. [times.csv](times.csv) contains four sequential baselines and all 28 standard configurations; [synchronous_times.csv](synchronous_times.csv) contains the 14 synchronous configurations. `speedup = sequential_total_ms / parallel_total_ms` for the same operands. Rerunning the suite replaces both CSV files.

| Operand lengths | Sequential ms | Best observed standard variant | Processes | Total ms | Speedup |
| --- | ---: | --- | ---: | ---: | ---: |
| 16 and 16 | 0.888792 | 2 | 4 | 0.854 | 1.041 |
| 1,000 and 1,000 | 0.902125 | 2 | 8 | 0.546 | 1.652 |
| 10,000 and 10,000 | 1.831542 | 1.1 | 9 | 1.575 | 1.163 |
| 100 and 100,000 | 7.870750 | 1.1 | 9 | 6.862 | 1.147 |

These are single-run observations and are sensitive to noise, filesystem caches, scheduling, and run order. They do not establish statistically reliable speedups. Variant 1.1 was faster than Variant 1 in six of seven standard configurations, but lost at 17 processes for 10,000 digits. See the notes for the full comparison and its limitations.

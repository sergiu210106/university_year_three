# Lab 3: understand the assignment and explain the code

This guide explains the implementation in this directory, including the C++ syntax, MPI calls, algorithms, scripts, correctness arguments, and measurements. Read it beside the source files. The examples below are paper exercises for learning, not additional test runs.

The central problem is simple: add two long decimal numbers digit by digit. The interesting part is distributing the work while preserving the carry dependency. A process responsible for more significant digits needs information from the process responsible for less significant digits.

## Start here: flow of each MPI executable

If you already understand the underlying concepts, start here and open the corresponding `.cpp` file beside this section. The aim is to follow the actual function calls and see what each process does.

### The shared path through all four files

```text
main(argc, argv)
  -> run_mpi(argc, argv, addition, minimum_processes)
       -> initialize MPI, find rank and process count, parse paths
       -> start timing
       -> addition(paths, rank, processes)
       -> finish timing, report the maximum elapsed time on rank 0
       -> finalize MPI
```

Every process follows this path separately. Each file supplies its own `addition` function, and `run_mpi` calls that function. The rank-dependent work is inside `addition` or the helpers it calls.

The shared functions live in headers:

| Header | Functions you will follow while reading the entry points |
| --- | --- |
| `mpi_common.hpp` | `run_mpi` handles the common program lifecycle. `send_blocking` selects `MPI_Send` or the synchronous-test `MPI_Ssend`. |
| `common.hpp` | `open_input` reads an input header; `read_digits` reads and zero-pads a slice; `add_digits` adds it; `chunk_count` and `chunk_offset` describe its position; output helpers write the answer. |
| `blocking.hpp` | `add_blocking`, `distribute_blocking`, `add_worker`, and `collect_blocking` implement the shared flow for Variants 1 and 1.1. |

Here, `p` means all MPI processes, including rank 0. It is the variable named `processes` in the source.

### mpi1.cpp: distribute, wait for carry, add, collect

**Purpose:** rank 0 handles files and communication; ranks 1 through `p-1` do the addition.

The file itself is small because its implementation is shared through `blocking.hpp`:

```text
main
  -> run_mpi(..., addition, 2)
       -> addition
            -> add_blocking(..., false)
                 rank 0: distribute_blocking -> collect_blocking
                 others: add_worker(..., false)
```

`2` is the minimum process count. `false` disables speculative addition.

| Function in this flow | What it does here |
| --- | --- |
| `addition` | Selects ordinary addition by passing `false` to `add_blocking`. |
| `add_blocking` | Root opens inputs and reads lengths, broadcasts the maximum length, then selects root or worker work. |
| `distribute_blocking` | Uses `chunk_count`, `chunk_offset`, and `read_digits` to read one worker's slices, then sends both with `send_blocking`. Repeats for all workers. |
| `add_worker` | Receives both slices, then the incoming carry if needed, calls `add_digits`, forwards its outgoing carry, and sends its result to root. |
| `collect_blocking` | Calls `open_output`, receives results in rank order, calls `write_segment` for each, receives the last carry, and calls `finish_output`. |

**Root's order:** read headers, broadcast length, distribute all slices incrementally, then receive and write result slices in order. Root never calls `add_digits` in this variant.

**A worker's order:** receive first operand, receive second operand, receive predecessor's carry, add, send carry onward, send result to root. Worker 1 starts with carry zero. The last worker sends its final carry to root after sending its result.

The main thing to notice in the code is where `add_digits` sits: in this variant, it runs after the incoming carry is known.

### mpi11.cpp: the same flow, with early addition and correction

**Purpose:** let a worker begin adding before its predecessor's carry arrives.

```text
main
  -> run_mpi(..., addition, 2)
       -> addition
            -> add_blocking(..., true)
                 rank 0: distribute_blocking -> collect_blocking
                 others: add_worker(..., true)
```

The entry point differs from `mpi1.cpp` only in passing `true`. Root's distribution and collection are identical. Follow the `speculative` branches inside `add_worker` to find the changed behavior.

| Function or expression | What changes |
| --- | --- |
| `add_digits(..., 0)` in the initializer of `outgoing` | Computes the local result assuming carry zero, before receiving the real carry. |
| The predecessor's `MPI_Recv` | Obtains the actual incoming carry after that initial computation. |
| `increment_digits(result, count)` | Adds one to the computed result if incoming carry is one. |
| `outgoing |= ...` | Preserves the original outgoing carry and any overflow produced by correction. |

**A worker's order:** receive both operands, add assuming zero carry, receive the actual carry if needed, correct the digits if it is one, forward the carry, return the result. Worker 1's incoming carry is already zero, so its initial calculation needs no correction.

The simplest explanation of the difference is: Variant 1 waits and then adds; Variant 1.1 adds first and corrects afterward. Both produce the same result and use the same communication routes.

### mpi2.cpp: scatter to all ranks, add, gather

**Purpose:** make rank 0 compute too, using collectives for operand distribution and result collection.

```text
main
  -> run_mpi(..., addition, 1)
       -> addition
            rank 0: read complete operands and pad them
            all: broadcast length -> scatter first -> scatter second
            all: receive carry if needed -> add -> forward carry if needed
            all: gather results -> broadcast last rank's carry
            rank 0: choose true output width and write result
```

`1` is the minimum process count because rank 0 can perform the whole calculation itself. Unlike `mpi1.cpp`, this file contains the variant's full `addition` implementation directly.

| Function used | Its role in this file |
| --- | --- |
| `open_input`, `read_digits` | Root reads both complete operands, padding the shorter operand and rounding the distribution width up to a multiple of `p`. |
| `MPI_Bcast` on `length` | Gives every rank enough information to calculate the same chunk size. |
| Two `MPI_Scatter` calls | Give each rank its first-operand and second-operand slices, including a slice for rank 0. |
| `MPI_Recv`, `add_digits`, `send_blocking` | Form the ordered carry/addition chain from rank 0 through rank `p-1`. |
| `MPI_Gather` | Places all local result slices into root's result array in rank order. |
| `MPI_Bcast` on `carry` | Gives every rank the carry leaving the last rank's padded slice. |
| `write_output` | Root writes only the true result digits, after deciding whether one extra digit is needed. |

**Root's order:** read everything, participate in both scatters, add its lowest slice with carry zero, forward carry, participate in gather and final broadcast, write the answer.

**Another rank's order:** participate in the length broadcast and both scatters, receive predecessor's carry, add its slice, forward carry unless last, participate in gather and final broadcast.

Look closely at `final_carry = padded > length ? result[length] : carry`. If there is distribution padding, the true extra digit may already be stored in the first padded position. That expression prevents it from being discarded with the padding.

### mpi3.cpp: post operations, keep buffers valid, wait for completion

**Purpose:** keep the I/O-only root and worker arrangement, but use nonblocking operand, carry, and result communication.

```text
main
  -> run_mpi(..., addition, 2)
       -> addition
            rank 0: open inputs and read headers
            all: broadcast maximum length
            rank 0: master(...)
            others: worker(...)
```

This file defines four functions: `main`, `addition`, `master`, and `worker`. The last two separate root's job from the workers' job.

| Function | What to follow inside it |
| --- | --- |
| `main` | Calls the shared MPI runner, requiring at least two processes. |
| `addition` | Root uses `open_input` to read headers, everyone participates in `MPI_Bcast`, and each rank enters `master` or `worker`. Worker counts come from `chunk_count`. |
| `master` | Posts the final-carry receive; uses `chunk_count`, `chunk_offset`, and `read_digits` for each slice; posts its result receive and two operand sends; waits for all requests; calls `write_output`. |
| `worker` | Posts operand and incoming-carry receives, waits for them, calls `add_digits`, posts carry and result sends, then waits for those sends before returning. |

**Root's order inside `master`:**

1. Post `MPI_Irecv` for the last worker's final carry.
2. For each worker, read its two slices into distinct array regions, post `MPI_Irecv` for its result, and post two `MPI_Isend` operations for its inputs.
3. Call `MPI_Waitall` for the operand sends and then for the result/carry receives.
4. Append a final carry if needed and write the complete result.

**A worker's order inside `worker`:**

1. Post two operand receives and, except on worker 1, an incoming-carry receive.
2. Wait until all required receives finish.
3. Call `add_digits` with the actual incoming carry.
4. Post a carry send to the next worker, or to root if last, and a result send to root.
5. Wait for both sends before its local buffers leave scope.

The key MPI functions are `MPI_Irecv`, `MPI_Isend`, and `MPI_Waitall`. Follow each request from its creation to its wait. Root uses different array regions for different workers so later file reads do not overwrite pending sends. This worker waits for the carry before adding; it does not use Variant 1.1's speculative correction.

### One comparison to keep in mind

| File | Where addition happens | Main flow to remember |
| --- | --- | --- |
| `mpi1.cpp` | Workers only | Distribute, wait for carry, add, stream results back. |
| `mpi11.cpp` | Workers only | Same distribution, add early, correct with carry, stream results back. |
| `mpi2.cpp` | All ranks | Read/pad, scatter, carry/add, gather, remove padding, write. |
| `mpi3.cpp` | Workers only | Post operations on stable buffers, wait for inputs/carry, add, return asynchronously, wait before writing. |

For deeper explanations of these exact paths, continue with Sections 7, 8, and 9 below.

## Reading map

Start with the [MPI file-flow overview above](#start-here-flow-of-each-mpi-executable) when you want to connect the concepts to the code directly.

1. [Problem, representation, and carry](#1-problem-representation-and-carry)
2. [Files and execution flow](#2-files-and-execution-flow)
3. [C++ syntax used in this project](#3-c-syntax-used-in-this-project)
4. [Shared arithmetic and I/O: common.hpp](#4-shared-arithmetic-and-io-commonhpp)
5. [Sequential implementation: seq.cpp](#5-sequential-implementation-seqcpp)
6. [MPI concepts and shared setup: mpi_common.hpp](#6-mpi-concepts-and-shared-setup-mpi_commonhpp)
7. [Variants 1 and 1.1: blocking.hpp and entry points](#7-variants-1-and-11-blockinghpp-and-entry-points)
8. [Variant 2: mpi2.cpp](#8-variant-2-mpi2cpp)
9. [Variant 3: mpi3.cpp](#9-variant-3-mpi3cpp)
10. [Build, run, generation, and verification scripts](#10-build-run-generation-and-verification-scripts)
11. [Correctness, performance, and limitations](#11-correctness-performance-and-limitations)
12. [Presenting the assignment](#12-presenting-the-assignment)
13. [Practice questions and worked answers](#13-practice-questions-and-worked-answers)
14. [Final revision sheet](#14-final-revision-sheet)

After the opening overview, use Sections 7, 8, and 9 for the detailed implementation walkthroughs. Sections 1 and 6 revisit the underlying concepts, and Section 3 is a syntax reference when an expression is unfamiliar.

## 1. Problem, representation, and carry

### Why an ordinary integer is insufficient

A fixed-width integer has a limited range. Even a 64-bit integer holds only about 19 or 20 decimal digits, depending on signedness. The assignment includes 100,000-digit numbers, so we store their individual decimal digits instead of converting the entire number to an integer.

For a number represented by `digits`, its mathematical value is:

```text
digits[0] * 10^0 + digits[1] * 10^1 + ... + digits[N-1] * 10^(N-1)
```

The code never computes these powers or this complete value. That formula explains what the array represents.

### Least significant digit first

The number `1234` is stored as `[4, 3, 2, 1]`. Index zero is the units digit. This is called least significant digit first, or LSD first.

The file is text:

```text
4
4 3 2 1
```

The first `4` is the digit count. The following four integers are the digits. The array stores numerical byte values such as `4`, not the character code for `'4'`.

Why this order? Ordinary addition begins at the units digit and moves toward more significant digits. A forward array loop now follows that exact order. A chunk at a lower array offset must pass its outgoing carry to the chunk at the next higher offset.

### One digit of addition

```cpp
int sum = first[i] + second[i] + carry;
result[i] = static_cast<unsigned char>(sum % 10);
carry = sum / 10;
```

`% 10` gives the units digit. Integer division by 10 gives the carry. With inputs from 0 through 9 and an incoming carry from 0 through 1, `sum` is at most 19, so the outgoing carry is always zero or one.

Example: `58 + 67`, represented as `[8, 5] + [7, 6]`:

| Index | First digit | Second digit | Incoming carry | Sum | Stored digit | Outgoing carry |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 8 | 7 | 0 | 15 | 5 | 1 |
| 1 | 5 | 6 | 1 | 12 | 2 | 1 |

Append the final carry: `[5, 2, 1]`, representing `125`.

### Different operand lengths

For `1234 + 56`, the logical arrays are:

```text
first:  [4, 3, 2, 1]
second: [6, 5, 0, 0]
```

The missing more significant digits are zeroes. Let `N = max(N1, N2)`. The output contains exactly `N` digits unless addition produces an extra carry, in which case it contains `N+1`. The implementation preserves the declared width even if the most significant input digits happen to be zero.

### Why dividing the arrays does not remove the dependency

Split `9999 + 0001` into two chunks of two digits:

```text
low chunk:  99 + 01 + incoming 0 = 100 -> result 00, outgoing 1
high chunk: 99 + 00 + incoming 1 = 100 -> result 00, outgoing 1
```

The answer is `10000`. If the high chunk ignored the incoming carry, it would produce `99` instead of `00`. MPI must transmit this one-bit mathematical dependency, represented here by an `int` zero or one.

## 2. Files and execution flow

| File | Responsibility |
| --- | --- |
| [task.md](task.md) | Professor's requirements and required configurations. |
| [cpp/common.hpp](cpp/common.hpp) | Paths, digit reading, addition, increment correction, partitioning, output, and time formatting. |
| [cpp/seq.cpp](cpp/seq.cpp) | Variant 0 and the reference result. |
| [cpp/mpi_common.hpp](cpp/mpi_common.hpp) | MPI setup, message tags, selectable blocking-send mode, errors, and timing. |
| [cpp/blocking.hpp](cpp/blocking.hpp) | Shared implementation of Variants 1 and 1.1. |
| [cpp/mpi1.cpp](cpp/mpi1.cpp) | Select ordinary blocking addition. |
| [cpp/mpi11.cpp](cpp/mpi11.cpp) | Select speculative addition and correction. |
| [cpp/mpi2.cpp](cpp/mpi2.cpp) | Scatter, ordered carry, and gather with rank 0 computing. |
| [cpp/mpi3.cpp](cpp/mpi3.cpp) | Nonblocking master and worker communication. |
| [scripts/generate_input.cpp](scripts/generate_input.cpp) | Reproducible random operands. |
| [scripts/build.sh](scripts/build.sh) | Build all executables and generate IntelliSense compilation commands. |
| [scripts/run.sh](scripts/run.sh) | Convenient launcher for one variant. |
| [scripts/run_tests.sh](scripts/run_tests.sh) | Required matrix, byte comparisons, timeouts, and CSV results. |
| [README.md](README.md) | Usage, limits, and result summary. |
| [information.md](information.md) | Original review notes, deadlock proof, and performance comparison. |

The MPI executable is started multiple times by the launcher. Every process enters the same `main`, calls `run_mpi`, and eventually executes the selected `addition` function with its own rank.

```text
mpi1 main  -> run_mpi -> addition -> add_blocking(speculative=false)
mpi11 main -> run_mpi -> addition -> add_blocking(speculative=true)
mpi2 main  -> run_mpi -> addition -> scatter / carry chain / gather
mpi3 main  -> run_mpi -> addition -> rank 0: master; other ranks: worker
seq main   -> read -> add_digits -> write_output
```

Headers provide shared definitions to each executable at compilation time. They do not create an extra process. The five `.cpp` entry points are compiled into separate executables, so their repeated names `main` and `addition` do not conflict.

## 3. C++ syntax used in this project

| Syntax from the code | Meaning and reason |
| --- | --- |
| `#include "common.hpp"` | Include a project header. Angle brackets, as in `<fstream>` or `<mpi.h>`, identify library headers. |
| `#ifndef`, `#define`, `#endif` | Header guards prevent duplicate definitions when a header is included through multiple paths in one compilation unit. |
| `inline` on a header function | Permits matching definitions in multiple compilation units. It does not force the compiler to substitute the body at every call. |
| `const int CAPACITY = ...` | Compile-time constant suitable for an ordinary fixed-size array bound. |
| `unsigned char first[CAPACITY]` | Local array of byte-sized nonnegative values. Capacity is fixed; the active digit count can be smaller. |
| `const unsigned char first[]` | Function receives access to existing array data and promises not to modify it through this parameter. The whole array is not copied. |
| `std::ifstream &input` | Reference to an existing input stream, so reading advances the caller's stream. |
| `const Paths &paths` | Read-only reference to the caller's small path structure. |
| `const char *path` | Pointer to a character string whose characters this code does not modify. |
| `char **argv` | Array of pointers to command-line argument strings. |
| `&carry` | Address of a scalar. MPI needs a memory address where it can read or write the value. |
| `first + offset` | Pointer to the array element at `offset`, equivalent to `&first[offset]`. No data is copied by this expression. |
| `condition ? a : b` | Conditional expression: evaluate `a` if true, otherwise `b`. |
| `if (carry)` | Execute the body when `carry != 0`. Here it means a final carry exists. |
| `length++` | Use the old value, then increment. `result[length++] = ...` stores at the old end and increases active length. |
| `++digits[i]` | Increment that digit by one. |
| `outgoing |= value` | Replace `outgoing` with the bitwise OR of its old value and `value`. Both operands are evaluated. |
| `&&` and `||` | Short-circuit logical operations. Later operands may not be evaluated. This matters in digit reading. |
| `static_cast<unsigned char>(digit)` | Explicit conversion from the validated integer to the array element type. |
| `static_cast<int>(digits[i])` | Print a byte as a decimal integer rather than as a character. |
| `auto start = ...` | Infer the clock time-point type from the expression. |
| `try`, `throw`, `catch` | Report failures without passing error codes through every helper. The top-level handler prints the explanation. |
| `std::cerr` versus `std::cout` | Errors go to standard error; the numerical timing goes to standard output for script capture. |

The local arrays are uninitialized when declared. This is intentional: the program fills every active element before reading it. Unused capacity is not automatically zero and must never be treated as valid data.

Each MPI process has its own arrays and variables in its own address space. Rank 0's `first` array and rank 1's `first` array are separate objects even when their source declarations have the same name.

The includes supply the declarations used by the code:

| Header | What it supplies here |
| --- | --- |
| `<algorithm>` | `std::min` and `std::max`. |
| `<fstream>` | Input and output file streams. |
| `<iomanip>` | `std::setw` and `std::setprecision` formatting. |
| `<iostream>` | Console streams and common stream operations. |
| `<stdexcept>` | `std::runtime_error` for failures with explanatory messages. |
| `<chrono>` | Sequential elapsed-time measurement. |
| `<mpi.h>` | MPI's C interface, which can be called from C++11. |
| `<cstdint>` | The fixed-width `std::uint32_t` seed type. |
| `<random>` | The `std::mt19937` generator. |
| `<string>` | String-to-integer conversions `std::stoi` and `std::stoul`. |

## 4. Shared arithmetic and I/O: common.hpp

### Constants and Paths

`MAX_DIGITS = 100000` bounds each operand. `MAX_PROCESSES = 17` bounds rank count and request arrays. `CAPACITY = MAX_DIGITS + MAX_PROCESSES = 100017` reserves a conservative maximum of 100,000 operand positions, 16 padding positions, and one extra carry position.

If a collective pads to the next multiple of `p`, it adds fewer than `p` digits, hence at most 16 for `p <= 17`. An index is valid only if it is less than the capacity. Capacity and active length are different: declaring 100017 elements does not mean the number has that many digits.

`HEADER_WIDTH = 6` accommodates the maximum result count, 100001. It is the width of the text count in the output, not the number of digits processed by a worker.

`struct Paths` groups three borrowed strings: first input, second input, and output. It owns no digit arrays and performs no file operations.

### parse_paths(argc, argv)

`argc` includes the executable name at `argv[0]`. Up to three optional paths are allowed, so `argc > 4` is an error. Each conditional expression checks that its argument exists before accessing it. Missing arguments use `Numar1.txt`, `Numar2.txt`, and `Numar3.txt`.

The `return { ... }` initializes a `Paths` value in field order. Relative paths are interpreted against the program's current working directory, not against the executable's directory.

### open_input(input, path)

`input.open(path)` connects the existing stream to a file. `input >> length` reads the first whitespace-separated integer and advances the stream past it. Its failure state is checked along with the bounds `1 <= length <= 100000`.

The function returns the declared digit count, leaving the stream positioned before the digits. Merely opening inputs and reading these headers is allowed before distribution in the streaming variants. It does not read the complete operands.

### read_digits(input, length, offset, digits, count)

Read this signature as: fill `count` entries in a destination slice corresponding to logical positions starting at `offset` in an operand of declared length `length`.

```cpp
int digit = 0;
if (offset + i < length && (!(input >> digit) || digit < 0 || digit > 9))
    throw std::runtime_error("expected a decimal digit");
digits[i] = static_cast<unsigned char>(digit);
```

For a logical position inside the operand, read and validate an integer digit. For a position beyond its length, the first condition is false, so `&&` skips the read. The initialized zero becomes padding. This avoids trying to extract nonexistent digits from the shorter file.

`offset` does not seek within the input file. Callers read slices in increasing, adjacent order, so the stream is already at the next unread digit. The offset tells the helper whether a position is inside the original operand.

The destination parameter is already a pointer to the beginning of the desired slice. In Variant 1 it points to a reusable chunk buffer, so writes use `digits[i]`. In Variant 3 the caller passes `first + offset`, so the same write reaches the full array's `first[offset + i]`. Adding `offset` again inside the helper would be wrong.

Reading into `int` first also avoids treating input as character extraction and permits checking the digit range before converting to a byte.

### add_digits(first, second, result, count, carry)

The loop performs the recurrence from Section 1 for exactly `count` positions. Input arrays are read-only. The result is written into the caller's array. The incoming carry is passed by value, so the function updates its local copy and returns the final carry.

The caller must store that return value. The function cannot append a final digit itself because, in MPI variants, its outgoing carry may belong to the next worker rather than the final output.

In the sum expression, C++ promotes the byte operands to an integer type before arithmetic. The intermediate is stored as `int`; the resulting digit is converted back to a byte only after taking the remainder.

An invariant for explaining correctness: before processing index `i`, all earlier result digits are correct and `carry` is exactly the carry entering index `i`. The loop preserves that fact for the next index. With `count == 0`, the loop does nothing and returns the incoming carry unchanged.

### increment_digits(digits, count)

This is addition of one to an existing local result:

1. Starting at the lowest local index, a digit below nine can be incremented and finishes the correction. Return zero for no overflow.
2. A nine becomes zero, and the increment continues to the next position.
3. If every digit was nine, all become zero and the helper returns one for overflow beyond the slice.

For LSD-first `[9, 9, 4]`, the result is `[0, 0, 5]` with return value zero. For `[9, 9]`, the result is `[0, 0]` with return value one. For an empty slice, it returns one, allowing an incoming carry to pass through that slice.

### chunk_count and chunk_offset

For a logical length `N`, `parts` recipients, and a zero-based recipient index `j`, define:

```text
q = N / parts        // integer quotient
r = N % parts        // remainder
count(j)  = q + (j < r ? 1 : 0)
offset(j) = j*q + min(j, r)
```

Every recipient gets `q` digits; the first `r` get one additional digit. `min(j,r)` counts how many earlier recipients got an extra digit. Consequently, slices are adjacent, do not overlap, and cover exactly `N` digits.

Example: `N=10`, three workers:

| Worker rank | Recipient index | Count | Offset | Global positions |
| --- | ---: | ---: | ---: | --- |
| 1 | 0 | 4 | 0 | 0, 1, 2, 3 |
| 2 | 1 | 3 | 4 | 4, 5, 6 |
| 3 | 2 | 3 | 7 | 7, 8, 9 |

Worker rank is converted to recipient index using `worker - 1`, because rank 0 is not a worker in these variants. Variant 2 uses equal padded chunks instead of these helpers.

### open_output, write_segment, finish_output, write_output

`open_output` truncates any existing output and writes a six-position count followed by a newline. `std::setw(6)` sets the minimum width of the next formatted field. With the default right alignment, count 16 occupies four spaces followed by `16`.

`write_segment` converts each byte to `int`, writes its decimal value followed by a space, and checks the stream. A segment is written as soon as the helper is called; the C++ stream can still buffer bytes before sending them to the operating system.

`finish_output` adds the digit line's final newline, moves the output position to the beginning with `seekp(0)`, replaces the count in the reserved header, closes the file, and checks for errors. Replacing a six-character count field with another six-character field keeps the digit data at the same location. The output is an ordinary seekable file.

For a two-digit sum with an extra carry, the file begins conceptually as:

```text
"     2\n"
```

After streaming digits and finalizing it contains:

```text
"     3\n5 2 1 \n"
```

In these blocks, `\n` denotes a newline character and the quotes are explanatory; the quotes are not written to the file. The trailing space after the final digit is deliberate. Every variant uses the same format, enabling byte-for-byte comparison.

`write_output` combines these three operations when the complete result is already available. Variants 1 and 1.1 use the operations separately because they write one received slice at a time.

Closing completes stream output, but does not request a physical-disk `fsync`. The validation is about the completed output file, not a durability benchmark.

### print_time(milliseconds)

Print a fixed-format decimal with six digits after the decimal point and a newline. This is the only normal output from an executable. Six printed decimal places do not guarantee the clock actually has that measurement precision.

## 5. Sequential implementation: seq.cpp

Read `main` from top to bottom:

1. `parse_paths` chooses the input and output names.
2. `high_resolution_clock::now()` begins timing. This clock offers the implementation's shortest clock tick period; whether it is monotonic depends on the implementation.
3. Three fixed arrays hold the two operands and the result. Two input stream objects are created.
4. `open_input` reads the two lengths; `std::max` selects the common processing width.
5. Two `read_digits` calls fill that width, padding the shorter input.
6. `add_digits(..., 0)` starts with carry zero, because there are no digits below index zero.
7. If the returned carry is one, `result[length++]` stores it after the existing result and updates the output width.
8. `write_output` writes and closes the result before timing stops.
9. `std::chrono::duration<double, std::milli>(end - start).count()` expresses the elapsed interval as a floating-point millisecond count.
10. The exception handler prints failures and returns one. Reaching the end of `main` on success returns zero in C++.

The baseline is deliberately simple. Parallel results are compared with it. Since parallel variants share its arithmetic helper, this is a reference comparison rather than an independent mathematical oracle.

## 6. MPI concepts and shared setup: mpi_common.hpp

### Processes, ranks, and communicator

For `mpirun -np 5 ./build/mpi1`, the MPI job contains five processes with ranks 0 through 4. A rank is an identifier inside a communicator. `MPI_COMM_WORLD` is the communicator containing the processes in this job.

MPI is a communication interface; Open MPI is the implementation used here. `mpic++` is a compiler wrapper that supplies the required MPI include and link options. It does not automatically turn a sequential loop into parallel work.

All ranks execute the same program but choose different branches based on `rank`. They share source code, not ordinary array memory. MPI transfers data between those separate address spaces.

### Reading a point-to-point call

From the blocking helper:

```cpp
MPI_Send(buffer, count, type, destination, tag, MPI_COMM_WORLD);
```

| Argument | Meaning in this assignment |
| --- | --- |
| `buffer` | Address of the first value to send. |
| `count` | Number of elements, not generally number of bytes. |
| `type` | Element representation, such as `MPI_UNSIGNED_CHAR` for digits or `MPI_INT` for a carry. |
| `destination` | Rank receiving the message. |
| `tag` | Integer identifying the message's purpose. |
| `MPI_COMM_WORLD` | Communication context and process group. |

The corresponding receive replaces destination with source and adds a status argument:

```cpp
MPI_Recv(result, count, MPI_UNSIGNED_CHAR, worker, RESULT,
         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
```

This receives digit data specifically from `worker` with tag `RESULT` in this communicator. The sender's destination is implicit on the receive side: the process executing the receive is that destination. Matching uses communicator, source/destination, and tag; the datatype describes compatible transferred data and the receive count bounds the buffer. These calls agree on the exact count.

`MPI_STATUS_IGNORE` discards message metadata because the source, type, and expected length are already known. It does not skip receiving or checking completion. See the official [send](https://docs.open-mpi.org/en/main/man-openmpi/man3/MPI_Send.3.html) and [receive](https://docs.open-mpi.org/en/main/man-openmpi/man3/MPI_Recv.3.html) descriptions.

### MessageTag

```cpp
enum MessageTag { FIRST = 1, SECOND, CARRY, RESULT, FINAL_CARRY };
```

The names stand for consecutive integers 1 through 5:

| Tag | Contents | Sender to receiver |
| --- | --- | --- |
| `FIRST` | First operand slice | Root to worker |
| `SECOND` | Second operand slice | Root to worker |
| `CARRY` | Carry between adjacent computing ranks | Earlier slice to later slice |
| `RESULT` | Computed result slice | Worker to root |
| `FINAL_CARRY` | Carry beyond the final worker slice | Last worker to root |

Tags separate message purposes. They do not establish global arrival order. In Variant 2, operand/result movement uses collectives instead of these input/result tags.

### Blocking, synchronous, and nonblocking mean different things

| Operation | What returning from this call tells the caller |
| --- | --- |
| `MPI_Send` | Standard blocking send has completed locally; the caller can reuse its send buffer. MPI may have used internal buffering. |
| `MPI_Ssend` | Synchronous blocking send has completed; a matching receive has been started and matched, and the caller can reuse its send buffer. |
| `MPI_Recv` | This receive has completed and the received values are available. |
| `MPI_Isend` | Send has been initiated; completion still needs to be established. |
| `MPI_Irecv` | Receive has been initiated; received values cannot yet be assumed available. |
| `MPI_Waitall` | All listed active requests have completed locally. |

Blocking means the call waits for its own completion condition. It does not mean every process waits. Synchronous describes the send's relationship to the matching receive. Nonblocking initiation and synchronous send mode are separate dimensions of MPI communication.

Never assume `MPI_Send` can finish without a matching receive, even if it happens to do so for small messages on your machine. Correctness must survive an implementation with no helpful buffering. Also, completion of a send does not mean the receiver has finished computing with the data. These distinctions follow the MPI standard's [communication modes](https://www.mpi-forum.org/docs/mpi-4.1/mpi41-report/node67.htm).

### send_blocking

The `const void *` parameter accepts the address of either a digit array or an integer carry. `count` and `MPI_Datatype` describe how MPI interprets that address.

`#ifdef SYNCHRONOUS_SEND` is a compile-time choice. Building with `-DSYNCHRONOUS_SEND` selects `MPI_Ssend`; otherwise it selects `MPI_Send`. It is not a runtime `if`, and every use of this helper, including carry and result sends, follows the chosen mode.

### MpiAddition function pointer

```cpp
typedef void (*MpiAddition)(const Paths &, int, int);
```

Read this as: `MpiAddition` names a pointer to a function returning `void` and taking a path reference plus two integers. Every variant's `addition` function has this signature. `run_mpi` can therefore call whichever addition function its executable supplies, while sharing initialization, timing, and error handling.

### run_mpi, block by block

1. `MPI_Init(&argc, &argv)` initializes MPI. Passing argument addresses allows MPI to handle its startup arguments. Ordinary MPI communication follows initialization.
2. `MPI_Comm_rank(..., &rank)` writes this process's rank. `MPI_Comm_size(..., &processes)` writes the total rank count.
3. Parse paths and validate the process count. Worker-only variants require at least two processes; Variant 2 permits one. All cap the count at 17.
4. The initial barrier ensures every rank reaches the timing boundary before any rank leaves it. Their subsequent timestamps need not be identical.
5. `MPI_Wtime()` provides a local elapsed-time clock in seconds. Record a start value on each rank.
6. `addition(paths, rank, processes)` invokes the selected implementation. Rank 0 opens, writes, and closes output during this call.
7. A final barrier ensures every rank has finished the addition call before a rank can leave the barrier. This includes waiting for root's output. A barrier completes only after all ranks in this communicator have entered it. See [MPI_Barrier](https://docs.open-mpi.org/en/v5.0.11/man-openmpi/man3/MPI_Barrier.3.html).
8. Subtract this rank's own start value and multiply by 1000 to convert seconds to milliseconds. Do not subtract timestamps from different ranks.
9. `MPI_Reduce(&elapsed, &total, 1, MPI_DOUBLE, MPI_MAX, 0, ...)` takes the largest elapsed value and delivers it to rank 0. The operation is a maximum, not a sum or average. All ranks participate; only root uses the reduced output. See [MPI_Reduce](https://docs.open-mpi.org/en/main/man-openmpi/man3/MPI_Reduce.3.html).
10. Only rank 0 prints the time, avoiding multiple numerical lines in the script's capture.
11. On a caught failure, print its rank and call `MPI_Abort` to terminate the job, so peers are not left waiting for communication from a failed rank. MPI's own default error handling can also abort before a C++ catch is reached; the catch handles exceptions thrown by our C++ helpers.
12. On normal completion, `MPI_Finalize()` finishes the MPI runtime and the function returns zero.

All ranks must call a collective such as broadcast, scatter, gather, reduce, or barrier in a compatible order. Putting a required collective only inside `if (rank == 0)` would leave its other participants absent. A collective is not automatically a barrier; rely only on its specified data/completion guarantees.

## 7. Variants 1 and 1.1: blocking.hpp and entry points

### The two small entry points

`mpi1.cpp` calls `add_blocking(paths, rank, processes, false)`. `mpi11.cpp` changes only the last argument to `true`. Their `main` functions pass the corresponding `addition` function to `run_mpi` with minimum process count two.

The Boolean `speculative` selects the worker arithmetic behavior. Both variants share distribution, receive order, result collection, file formatting, and error handling. This keeps the comparison focused on the speculative optimization.

### add_blocking: choose root or worker work

Every rank creates local stream objects and initializes the length variables. Only rank 0 opens input files and reads their headers. It computes `length = max(length1, length2)`.

```cpp
MPI_Bcast(&length, 1, MPI_INT, 0, MPI_COMM_WORLD);
```

Rank 0 supplies the length and all other ranks receive it into their own local `length`. Everyone can now independently calculate their slice size. The individual operand lengths and stream positions are needed only on root, which handles padding. See [MPI_Bcast](https://docs.open-mpi.org/en/main/man-openmpi/man3/MPI_Bcast.3.html).

`workers = processes - 1`, because root is reserved for I/O. Root calls `distribute_blocking` and then `collect_blocking`. Other ranks call `add_worker` using the count for recipient index `rank - 1`.

### distribute_blocking: streaming input

Root declares two chunk buffers and loops from worker rank 1 through `workers`, inclusive. For each worker it:

1. Calculates that worker's count and global offset.
2. Reads the next slice of each file into the reusable buffers, padding missing operand digits.
3. Sends the first buffer with tag `FIRST`.
4. Sends the second buffer with tag `SECOND`.

After each blocking send returns, its buffer can be reused. The next iteration can overwrite both buffers because both sends from this iteration have completed. The function never fills whole-operand arrays before beginning distribution and never performs digit addition.

The arrays have large fixed capacity to support all valid configurations, but only `count` entries are active in each iteration. Declaring a full-capacity array is not the same as reading all digits upfront.

### add_worker: the exact communication order

The worker receives `first` from root, then `second` from root. These blocking receives complete before it attempts to receive a carry. This follows the assignment's receive-order constraint.

For Variant 1, `outgoing` initially becomes zero and no addition happens yet. `incoming` starts at zero. Worker 1 uses that initial zero; every other worker receives a carry from `rank - 1`.

```cpp
outgoing = add_digits(first, second, result, count, incoming);
```

This computes the correct local result and outgoing carry. If the worker is not last, it sends that carry to `rank + 1` before sending its result to root. The last worker sends no neighbor carry; it sends its result and then sends the final carry to root under `FINAL_CARRY`.

The last worker sends the final carry even when it is zero. Root always posts that receive, so omitting the zero message would leave root waiting forever.

```text
Root  -- first and second slice --> Worker 1
Root  -- first and second slice --> Worker 2
Root  -- first and second slice --> Worker 3

Worker 1 -- carry --> Worker 2 -- carry --> Worker 3

Worker 1 -- result ------------------------------> Root
Worker 2 -- result ------------------------------> Root
Worker 3 -- result, then final carry ------------> Root
```

These arrows describe dependencies and message routes. They do not require every independent action to happen at the same time or in a single global sequence.

### collect_blocking: write without gathering a full result array

Root opens the output with a provisional count `length`. It loops over workers in increasing rank order, receives exactly that worker's result count into one reusable result buffer, and calls `write_segment` immediately.

Increasing rank order is also increasing digit significance, so the file stays LSD first even if workers become ready in a different order. A ready later worker may wait while root receives an earlier worker. The root does not first concatenate all results into a full-result array.

After all slices, root receives the last worker's final carry. If it is one, root writes one additional byte-valued digit, then finalizes the header with `length + carry`. Since carry is zero or one, that expression is exactly the final width.

### Walk through the supplied 16-digit operands

There are five processes but only four computing workers. Each worker receives four digits. The numbers are written below in conventional most-significant-first notation inside each group, while global offsets still refer to the LSD-first arrays.

```text
Operand 1: 9999 4444 4444 9999
Operand 2: 9999 5555 5555 9999
```

| Worker | Global offset | Local operands | Incoming carry | Local total | Four result digits, conventional order | Outgoing carry |
| --- | ---: | --- | ---: | ---: | --- | ---: |
| 1 | 0 | 9999 + 9999 | 0 | 19998 | 9998 | 1 |
| 2 | 4 | 4444 + 5555 | 1 | 10000 | 0000 | 1 |
| 3 | 8 | 4444 + 5555 | 1 | 10000 | 0000 | 1 |
| 4 | 12 | 9999 + 9999 | 1 | 19999 | 9999 | 1 |

Root writes the low group first, so the digit line is:

```text
8 9 9 9 0 0 0 0 0 0 0 0 9 9 9 9 1
```

There are 17 digits. Reading from the end for conventional notation gives `1 9999 0000 0000 9998`. Worker boundaries explain why this example is useful: an incoming carry crosses entire groups of nines.

### Why synchronous sends do not deadlock here

A deadlock requires a cycle in which processes cannot reach the operations needed to release one another. A worker merely waiting for another worker is not by itself a deadlock.

Assume every send requires the matching receiver to participate:

1. Worker 1 is receiving operands, so root can deliver its two chunks.
2. Worker 1 may block trying to send carry to worker 2, which is still waiting for its operands.
3. Root does not wait for worker 1's result at this point. Root proceeds to give worker 2 its operands.
4. Worker 2 then reaches its carry receive, releasing worker 1. The same argument advances through all workers.
5. After distribution, root receives results in rank order. Every worker forwards its carry before it can get stuck waiting to return its result.
6. Root's final receive order matches the last worker's result-then-final-carry send order.

An unsafe change would be for root to receive worker 1's result before distributing worker 2's operands. Worker 1 might be waiting to send its carry to worker 2; worker 2 would be waiting for operands from root; root would be waiting for worker 1's result. That is a cycle.

The required `MPI_Ssend` runs demonstrate that the tested cases do not depend on standard-send buffering. The ordering argument explains why the design is safe more generally; passing a few executions alone is not a universal proof.

### Variant 1.1: computation before the incoming carry

The conditional initializer is the key change:

```cpp
int outgoing = speculative ? add_digits(first, second, result, count, 0) : 0;
```

When `speculative` is true, the worker computes assuming incoming carry zero before receiving the real carry. The guessed carry is not a prediction that must usually be correct: both possibilities are handled exactly. Since the real carry can only be zero or one, a zero-carry result can be corrected by adding one.

After receiving `incoming`:

```cpp
if (incoming) outgoing |= increment_digits(result, count);
```

If incoming is zero, the speculative digits are already correct. If incoming is one, increment the local result, and preserve either source of outgoing carry: the original speculative addition or the correction overflow.

For worker 2 in the supplied example, `4444 + 5555 = 9999` gives speculative carry zero. Incoming one changes it to `0000` and produces correction overflow one. For worker 4, `9999 + 9999 = 19998` gives result `9998` and speculative carry one. Incoming one changes its result to `9999`, with correction overflow zero. Its outgoing carry must remain one.

Replacing `outgoing |= ...` with `outgoing = ...` would lose the original carry in that last case. Replacing it with `outgoing = outgoing || increment_digits(...)` would also be wrong: logical OR could short-circuit when outgoing is already one and skip the required digit correction. Bitwise OR evaluates the helper and combines its zero-or-one return value.

Why can OR combine these values? For a `k`-digit slice let `B=10^k`. The sum of the two slice values is at most `2B-2`. A speculative carry of one together with a local result of all nines would require sum `2B-1`, which is impossible. The two overflow sources cannot both be one for a nonempty slice, and either source means the outgoing carry should be one. For an empty slice, the speculative carry is zero and increment passes incoming one through.

The optimization allows different workers' zero-carry additions to overlap once their inputs arrive. A correction may touch only one digit or may scan an entire slice of nines. It therefore reduces a dependency in favorable cases without guaranteeing a speedup on every input or process count.

## 8. Variant 2: mpi2.cpp

### What changes from Variant 1

Rank 0 participates in arithmetic. There are `p` computing ranks instead of `p-1`. Root may read the complete operands upfront. All ranks receive equal-size slices through collectives, and root gathers the complete result before writing it.

Six arrays appear in `addition`: three full arrays named `first`, `second`, `result`, and three arrays for the local slice. The same declarations exist in every process, even though the full operand/result arrays are only used meaningfully on root. This simple fixed-array implementation favors readability over minimum per-rank memory usage.

### Read and pad

Root opens the files and computes the true length `N`. It calculates:

```cpp
int padded = ((length + processes - 1) / processes) * processes;
```

For positive integers, `(N+p-1)/p` is `ceil(N/p)` using integer division. Multiplying by `p` produces the smallest multiple of `p` at least `N`.

For `N=10`, `p=4`, each slice has three digits and padded length is 12. For the required `N=1000`, `p=16`, each slice has 63 digits and padded length is 1008. Root checks that padded digits plus one carry position fit the capacity.

`read_digits` fills to padded length for each operand. This handles two distinct kinds of zeroes: those needed because an operand is shorter than `N`, and those needed because the distribution width exceeds `N`.

After broadcasting the true length, every rank recomputes `count` and `padded`. The root-local variable named `padded` inside the earlier `if` has a narrower scope; the later variable is a separate declaration with the same calculated value.

### Two scatters

```cpp
MPI_Scatter(first, count, MPI_UNSIGNED_CHAR, local_first, count,
            MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
```

Read the arguments in two groups:

| Arguments | Meaning |
| --- | --- |
| `first, count, MPI_UNSIGNED_CHAR` | Root sends `count` digits per destination from consecutive blocks of its full array. |
| `local_first, count, MPI_UNSIGNED_CHAR` | Each rank receives its block into its local array. |
| `0, MPI_COMM_WORLD` | Root rank is zero and all ranks in this communicator participate. |

For rank `r`, the block begins at root offset `r * count`. `sendcount` is the per-rank count, not the full padded length. Root receives a local block too. A second scatter distributes `second` in the same way. See [MPI_Scatter](https://docs.open-mpi.org/en/main/man-openmpi/man3/MPI_Scatter.3.html).

On nonroot ranks, scatter's send buffer is ignored. It is therefore okay that their full `first` and `second` arrays were not initialized. The receive buffers must be valid on every rank and are separate from root's full arrays.

### Ordered carry

Rank 0 starts with carry zero. Every other rank receives from `rank - 1`. Every rank calls `add_digits` and, unless last, sends to `rank + 1`.

```text
rank 0 -> rank 1 -> ... -> rank p-1
```

The chain begins at rank 0 here, whereas Variant 1 begins at worker rank 1. Each rank completes its carry forwarding before entering the gather. All ranks execute the same collective sequence; earlier ranks can wait inside gather while later ranks finish the carry chain.

Scatter and gather do not calculate decimal carries. They only move the digit data. The explicit neighbor communication is still necessary.

### Gather and final carry broadcast

```cpp
MPI_Gather(local_result, count, MPI_UNSIGNED_CHAR, result, count,
           MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
```

Each rank sends `count` digits from `local_result`. On root they are placed in rank order, with rank `r` starting at `r * count`. Root's receive count is per rank, so the total received length is `p * count`. On nonroot ranks, the full receive array `result` is ignored. See [MPI_Gather](https://docs.open-mpi.org/en/v5.0.11/man-openmpi/man3/MPI_Gather.3.html).

Then `MPI_Bcast(&carry, ..., processes - 1, ...)` distributes the last rank's carry. A broadcast root need not be rank 0. Before this broadcast, each rank's `carry` describes its own slice's outgoing carry; afterward every rank has the last slice's carry. Only rank 0 needs it for output, but using a broadcast keeps this final transfer collective.

### The true final carry versus the padded final carry

This expression is essential:

```cpp
int final_carry = padded > length ? result[length] : carry;
```

If there was no distribution padding, the last rank's carry is the carry beyond the true number width. If there was padding, that true overflow has already been added to the first padded zero position, at index `length`. The carry leaving the entire padded range can now be zero even when the true result needs an extra digit.

Paper example: `99 + 01`, true width two, padded width three:

```text
inputs:          [9, 9, 0] + [1, 0, 0]
gathered result: [0, 0, 1]
carry after index 2: 0
```

The answer is `100`, so inspect `result[2]`, not just the final padded carry. The implementation then writes only `N` or `N+1` digits, discarding the other distribution padding.

The assignment tests this variant with 4, 8, and 16 processes. The code also handles one process: both scatter and gather are local collective participation, rank 0 is also last rank, and no neighbor carry message is needed.

## 9. Variant 3: mpi3.cpp

### The change is communication initiation and completion

Variant 3 keeps rank 0 for I/O and uses `p-1` workers. It replaces blocking point-to-point calls with `MPI_Isend` and `MPI_Irecv`, then explicitly waits for their requests.

The assignment says root sends `N/p` digits per iteration despite reserving root from arithmetic. Using that literally for exactly `p-1` slices would leave digits unprocessed. This implementation uses quotient/remainder partitioning across all `p-1` workers, covering every digit. Explain this interpretation openly if asked.

A nonblocking request is a handle for an outstanding operation, not a copy of the data. MPI may continue accessing the original buffer. An active send buffer must not be modified; an active receive buffer must not be accessed until completion. See [MPI_Isend](https://docs.open-mpi.org/en/main/man-openmpi/man3/MPI_Isend.3.html) and [MPI_Irecv](https://docs.open-mpi.org/en/main/man-openmpi/man3/MPI_Irecv.3.html).

### addition: input headers and branching

As in the blocking variants, only root opens files and reads lengths. Broadcasting the maximum width lets every worker calculate its own count. Root calls `master` with both open streams and the actual operand lengths. Every other rank calls `worker` with its rank, the number of workers, and its assigned count.

`main` again delegates to `run_mpi` with minimum process count two.

### master: stack arrays and request indexing

Root allocates two full-capacity input arrays and one result array. Unlike the blocking reusable chunk buffer, each in-flight slice occupies a distinct region. These arrays are populated incrementally, not fully read before distribution starts.

```cpp
MPI_Request sends[2 * (MAX_PROCESSES - 1)];
MPI_Request receives[MAX_PROCESSES];
```

At most 16 workers require 32 operand sends, 16 result receives, and one final-carry receive. For worker rank `w`:

| Request entry | Operation |
| --- | --- |
| `sends[2*(w-1)]` | First operand send to worker `w`. |
| `sends[2*(w-1)+1]` | Second operand send to worker `w`. |
| `receives[w-1]` | Result receive from worker `w`. |
| `receives[workers]` | Final carry receive from the last worker. |

The carry request uses the slot after the result-receive slots, so it does not overwrite one of them. At 17 processes, active send indices are 0 through 31 and receive indices are 0 through 16.

Root initializes `carry` and immediately posts its final-carry receive. Posting early is valid even though the matching message will arrive later.

### master: one iteration

For each worker, root calculates its count and offset, reads both slices into `first + offset` and `second + offset`, posts a result receive into `result + offset`, then posts the two input sends.

For example, after sending positions 0 through 3 to worker 1, root can fill positions 4 through 7 for worker 2. These are disjoint memory regions. It must not reuse positions 0 through 3 until their send requests complete.

This is why the blocking version's single reusable chunk buffer cannot simply be combined with `MPI_Isend` by renaming the call. The next read might overwrite digits MPI has not yet copied. Keeping stack storage alive is necessary, but the data must also remain unchanged throughout the active send.

Posting result receives early lets workers return results without waiting for root to finish its distribution loop. Separate receive regions prevent workers' results from overwriting one another.

### master: completion and output

```cpp
MPI_Waitall(2 * workers, sends, MPI_STATUSES_IGNORE);
MPI_Waitall(workers + 1, receives, MPI_STATUSES_IGNORE);
```

The first wait ensures all input-send buffers are released by MPI. The second ensures every result slice and the final carry are available. Waiting on sends first is safe because all result receives have already been posted; root is not withholding an operation a worker needs in order to finish.

`MPI_STATUSES_IGNORE` discards the array of completion statuses. Only the initialized active prefix of each request array is passed to `MPI_Waitall`; unused capacity contains no valid requests and is not included. On successful completion, these ordinary nonblocking requests become `MPI_REQUEST_NULL`. See [MPI_Waitall](https://docs.open-mpi.org/en/v5.0.11/man-openmpi/man3/MPI_Waitall.3.html).

After both waits, root can inspect `carry`, append a digit if needed, and write the complete result. The arrays remain in `master`'s scope until all associated requests complete.

### worker: receives may complete in any order

The worker declares three digit arrays, three receive-request slots, and two send-request slots. It posts both operand receives first. If its rank exceeds one, it also posts the predecessor's carry receive.

`receive_count` is two for worker 1 and three otherwise. Worker 1 has no predecessor carry message and uses its initialized incoming zero. Calling `MPI_Waitall(3, ...)` there would incorrectly include an uninitialized request entry.

All required receives are posted before waiting. A carry that arrives before one operand can match its own receive request; the program does not need to guess message arrival order. Worker arithmetic starts only after all its required receive requests have completed.

Unlike Variant 1.1, this worker does not speculatively compute before the carry. Variant 3 changes communication handling; it retains the addition's mathematical carry dependency.

### worker: route the outgoing carry and return the result

After addition:

```cpp
int destination = rank < workers ? rank + 1 : 0;
int tag = rank < workers ? CARRY : FINAL_CARRY;
```

An intermediate worker sends to its successor under `CARRY`; the last sends to root under `FINAL_CARRY`. Both use one `MPI_Isend` request. A second request returns the result slice to root. Both zero and one carries are sent.

`MPI_Waitall(2, sends, ...)` completes these sends before the worker returns. Its local `outgoing` scalar and `result` array must remain alive and unmodified until then. `Isend(&outgoing, ...)` does not copy the integer just because the call itself returned.

### Why this pattern is safe, and what it does not promise

Root posts receives without waiting for workers to finish. Workers post their operand and carry receives before waiting. Worker 1 can begin the finite carry chain when its inputs arrive. Each subsequent worker can then complete its inputs and carry, compute, and send onward. There is no cycle in which someone must first post a receive that they can reach only after the other operation completes.

Nonblocking communication allows potential overlap between independent operations. It does not imply that transfers finish immediately, that MPI always progresses in the background during file reading, or that Variant 3 will be faster. Effective overlap depends on the implementation and system, as described by the MPI standard's [nonblocking communication section](https://www.mpi-forum.org/docs/mpi-4.1/mpi41-report/node71.htm).

## 10. Build, run, generation, and verification scripts

### Bash syntax shared by the scripts

| Expression | Explanation |
| --- | --- |
| `#!/bin/bash` | Select Bash when the executable script is launched directly. |
| `set -euo pipefail` | Stop on unhandled command failures in the script's normal flow, reject unset variable expansions, and propagate pipeline failure. Bash has exceptions to `-e`, such as commands used as conditions. |
| `ROOT="$(cd "$(dirname "$0")/.." && pwd)"` | Find the script's parent directory, move one level up, and capture that absolute assignment directory. `$0` is the script path. |
| `"$ROOT/build/seq"` | Quote expansions to preserve a path containing spaces as one argument. |
| `${MPICXX:-mpic++}` | Use `MPICXX` when set and nonempty, otherwise use `mpic++`. |
| `$(command)` | Run a command and capture its standard output, removing trailing newlines. |
| `$((comparisons + 1))` | Evaluate shell integer arithmetic. |
| `[[ condition ]]` | Bash conditional syntax. |
| `"$@"` | Forward remaining arguments separately, preserving spaces within each. |
| `shift` | Remove the current first positional argument. |
| `>` and `>>` | Create/replace a file, or append to it. |
| `<<'EOF'` | Supply a literal multiline block to a command; the quoted delimiter prevents shell expansion inside it. |
| `read -r ... <<< "$spec"` | Split one specification string into variables without interpreting backslashes. |
| `local` | Declare function-local variables. Other variables used by the function can come from its surrounding shell context. |

### build.sh

The script creates `build/`, chooses the MPI compiler wrapper, and compiles each entry-point source into a separate executable.

| Compiler argument | Purpose |
| --- | --- |
| `-std=c++11` | Select the language version required by the assignment. |
| `-O2` | Enable compiler optimizations for the measured build. |
| `-Wall -Wextra` | Enable useful compiler diagnostics. |
| `-pedantic` | Request diagnostics for nonstandard language extensions. |
| `-o path` | Choose the executable output path. |
| `-DSYNCHRONOUS_SEND` | Define the preprocessor macro that selects synchronous sends in the shared helper. |

The first loop builds `seq`, `mpi1`, `mpi11`, `mpi2`, and `mpi3`. The second builds `mpi1_ssend` and `mpi11_ssend` from the same sources with the macro enabled. The generator is compiled as an eighth executable. All use the wrapper, although sequential addition and generation make no MPI runtime calls and are launched directly.

The Python block generates `build/compile_commands.json` for editor tooling:

1. `python3 - "$ROOT" "$MPICXX"` reads the Python program from standard input and passes root and compiler as script arguments.
2. `Path` handles filesystem paths; `sys.argv` accesses those two arguments.
3. `shutil.which` resolves the selected compiler executable.
4. `subprocess.check_output([compiler, '--showme:incdirs'], text=True)` asks the Open MPI wrapper for its header directories. Splitting the returned text provides the include list. This query is specific to the installed Open MPI workflow.
5. `glob('*.cpp')` selects the five C++ entry points; the generator source is appended separately. Headers do not need standalone compilation entries because they are included from sources.
6. For each source, construct an argument list with C++11 flags, explicit `-I` include directories, `-c`, source path, and object-output path.
7. Store its working directory, source filename, and argument list as a dictionary, then write the six dictionaries as formatted JSON.

These `-c` commands describe compilation to the editor. The Python block does not run them or create the listed `.o` files. The earlier shell commands already built the actual executables.

### VS Code configuration and ignore rules

The workspace file `.vscode/c_cpp_properties.json` points IntelliSense to `/opt/homebrew/bin/mpic++`, supplies MPI's include directory, selects macOS ARM64 parsing, and names the generated compilation database. That database gives these six sources their C++11 configuration. The C++20 fallback remains available for earlier labs; the C language fallback remains C17.

Editor configuration affects parsing, navigation, and diagnostics. The build script's flags determine the actual executables. Hardcoded compiler/include paths reflect this machine; another installation may need its corresponding paths.

This assignment's `.gitignore` excludes `build/`, since inputs, binaries, outputs, and compilation commands can be regenerated. The workspace `.gitignore` specifically allows the requested C/C++ configuration through the repository's broader `.vscode/` ignore rule while leaving other editor settings ignored. CSV measurements and study notes remain outside `build/` so they can be reviewed.

### generate_input.cpp

The generator's command line is:

```text
generate_input <digits> <output> <seed>
```

It checks `argc == 4` because the executable name counts as one argument. `std::stoi(argv[1])` parses the requested length and the usual bounds check follows. `std::stoul(argv[3])` parses an unsigned seed; converting to `std::uint32_t` selects a 32-bit seed value for `std::mt19937`.

`std::mt19937 random(seed)` creates a deterministic pseudo-random generator. Reusing the seed and generation procedure reproduces the same operand. It is intended for reproducible inputs, not cryptography.

The output begins with the count. For each digit:

```cpp
unsigned int digit = i + 1 == length ? 1 + random() % 9 : random() % 10;
```

Ordinary positions get values from zero through nine. The final array position is the most significant digit, so it gets a value from one through nine, ensuring the number really has the declared width. Modulo reduction introduces a very small distribution bias when the generator's output range is not divisible by the modulus; exact statistical uniformity is not required here.

The second conditional expression writes spaces between digits and a newline after the last. Closing and checking the output detects write failures. The exception handler also catches parsing errors and reports them.

### run.sh

The script requires a variant name, saves `$1` in `variant`, then shifts it off. `case` selects one branch:

| Branch | Action |
| --- | --- |
| `seq` | Replace the shell with the sequential executable and forward the remaining paths. |
| Any MPI variant | Take the next argument as process count, defaulting to five; remove that argument if present; launch the selected executable through MPI and forward the remaining paths. |
| Anything else | Print a usage error and exit unsuccessfully. |

`exec` replaces the script process with the selected command instead of retaining an extra shell. `${MPIEXEC:-mpirun}` permits a launcher override. `--oversubscribe` permits more processes than available local slots. `-np` sets the number of MPI processes; it does not set a thread count.

For an MPI run with custom paths, provide the process count before those paths. Paths are forwarded unchanged, so their relative location still depends on where you invoked the script.

### run_tests.sh: prepare fixed input pairs

The script builds first and creates input/output directories. Two literal blocks create the supplied 16-digit operands in LSD-first format. These particular input strings are symmetric under digit reversal, but that is incidental: arbitrary operands must still use the required LSD-first representation.

Three specifications define generated pairs:

| Case name | Lengths | Seeds |
| --- | --- | --- |
| `equal1000` | 1000, 1000 | 3101, 3102 |
| `equal10000` | 10000, 10000 | 3201, 3202 |
| `unequal` | 100, 100000 | 3301, 3302 |

Each operand is generated once per suite invocation and reused across variants and process counts. Repeating the suite regenerates those deterministic files and replaces the CSVs; it does not add more samples to the old measurement set.

### record_parallel: launch, compare, record

The function receives variant, process count, send mode, and destination CSV. It selects the `_ssend` executable when requested and constructs a distinct output path.

```bash
elapsed="$("$TIMEOUT" "$LIMIT" "$MPIEXEC" --oversubscribe -np "$processes" \
    "$ROOT/build/$program" "$input1" "$input2" "$output")"
```

The timeout surrounds the launcher and program, catching hangs as well as unexpectedly slow launches. It defaults to 30 seconds. The captured stdout is the executable's internally measured millisecond value, so the external launcher interval is not used as the reported runtime. Normal errors go to stderr and are not part of that value.

`cmp "$reference" "$output"` checks byte identity, including header spacing and the entire digit sequence. A mismatch or failed/expired execution stops the suite rather than recording success. The `awk` command computes `baseline / elapsed`, and `printf` appends the CSV record. The comparison count increases only after a successful comparison.

Variables such as `name`, `n1`, `n2`, `input1`, `input2`, `reference`, and `baseline` are set by the surrounding case loop. The shell function reads those values; it does not recompute inputs or baselines.

### The outer loops and the exact required matrix

For each of four input pairs, run sequential once and save its timing and reference output. Then:

| Group | Supplied 16 digits | Equal-size random case | Unequal random case | Standard runs |
| --- | --- | --- | --- | ---: |
| Each of 1, 1.1, 3 | p=5 | 10,000 digits, p=5/9/17 | 100 and 100,000 digits, p=5/9/17 | 7 each |
| Variant 2 | p=4 | 1,000 digits, p=4/8/16 | 100 and 100,000 digits, p=4/8/16 | 7 |

The condition excluding `equal1000` from the worker-only variants and the condition excluding `equal10000` from Variant 2 implement these different assignment requirements. The supplied case narrows each process-count list to its single required value.

Variants 1 and 1.1 also repeat their seven configurations with synchronous sends: 14 extra comparisons. Totals are four sequential executions, 28 standard MPI executions, and 14 synchronous MPI executions. That is 46 output files and 42 parallel comparisons. `times.csv` has 32 data rows including baselines; `synchronous_times.csv` has 14 data rows.

No independent-oracle, extra edge-case, invalid-input, or sanitizer suite is part of this script. Timeouts plus tested synchronous sends provide evidence about the required runs, while source reasoning covers the general algorithm.

## 11. Correctness, performance, and limitations

### Four correctness questions to answer at any code location

1. Which global digit positions does this local array region represent?
2. Has every active element been filled or received before it is read?
3. Is the carry entering this slice correct, or will a speculative result be corrected before sending it?
4. For any MPI buffer, has the operation completed before that buffer is reused or destroyed?

For file output, add a fifth question: are slices written in increasing global offset, and is only the true final carry appended?

### A proof outline for the complete sum

Digit addition's loop invariant proves a slice is correct given its incoming carry. The lowest slice starts with carry zero. Assume a slice receives the correct carry: it computes the correct result and forwards the correct outgoing carry, so the next slice receives the correct carry as well. By induction through the rank chain, all slices are correct.

Variant 1.1 uses an equivalent local calculation because adding incoming one equals incrementing the zero-carry sum. Partitioning or scatter assigns every digit to exactly one slice. Ordered output or gather restores global LSD-first order. Correct handling of the true final carry then establishes the complete output width and value.

Communication correctness is a separate obligation: matching tags, counts, and datatypes; participation in collectives; no circular waits; and valid nonblocking buffer lifetimes. Correct arithmetic cannot compensate for an unmatched message.

### Why multiple processes can still have largely serial arithmetic

Let `N` be the maximum operand width and `m` the number of computing ranks. For roughly equal chunks, local addition takes `O(N/m)` work per rank, but Variant 1 waits for the predecessor before doing that work. The arithmetic dependency path contains all chunks in sequence, adding up to `O(N)` digit work, plus neighbor communication latency.

Variant 2 and this implementation of Variant 3 also wait for the incoming carry before doing local addition. Merely using scatter/gather or nonblocking communication does not shorten that mathematical arithmetic chain. They change distribution, collection, and possible communication overlap.

Variant 1.1 can overlap zero-carry additions. In an idealized situation with inputs already available, these additions take about one chunk's time on the critical path, followed by ordered corrections and carry messages. Short corrections help; worst-case all-nine corrections can still scan a total of `N` digits along the chain. Root's serial reading and output remain present in all variants.

Total digit work remains linear for fixed representation, including correction work in Variant 1.1. Root communicates `O(N)` digit data overall; carry messages add `O(p)` small messages. These descriptions separate computational work from latency and filesystem costs; they are not an exact runtime prediction.

An advanced answer if asked how to remove more carry dependency: a parallel-prefix method could combine per-block carry behavior before finalizing results. That is a different algorithm and is not implemented in this assignment.

### Memory use

The arrays are fixed at capacity `C=100017`, regardless of the active slice size. Therefore a worker's storage in this implementation is `O(C)`, not a dynamically shrinking `O(N/p)` allocation. The mathematical active data per slice is smaller, but the actual reserved array does not shrink.

Variant 2 declares six digit arrays per rank, totaling 600102 bytes of digit storage, approximately 586 KiB, plus other objects and runtime overhead. Variant 3's master and each arithmetic worker declare three digit arrays. The blocking distributor uses two reusable operand arrays, and the collector uses a reusable result array in a separate function phase.

Only digit and request buffers are constrained to these local arrays. File streams, standard-library internals, and the MPI implementation can manage their own memory. Stack-buffer requirements do not imply that the entire operating-system process performs no internal allocation.

### Understand the timing boundary

| Included in the reported parallel time | Excluded |
| --- | --- |
| Opening and reading inputs after the start timestamp | Input generation and compilation |
| Metadata broadcast, distribution, carries, and result collection | MPI launcher startup and `MPI_Init` |
| Local arithmetic and any correction | Initial timing barrier before the timestamp |
| Writing, updating the header, and closing root's output | External `cmp` and CSV calculation |
| Final completion barrier and its waiting overhead | Timing reduction, printed timing, and `MPI_Finalize` |

The final barrier makes every measured interval extend through output completion, subject to each rank's local start/exit time. Maximum reduction provides one job-level observation. Do not sum rank times: processes execute and wait concurrently, so summing would count overlapping intervals multiple times.

### Speedup and actual observations

```text
speedup = sequential_total_ms / parallel_total_ms
```

Above one means the parallel execution was faster in that measurement. Below one means slower. `7.870750 / 6.862000 = approximately 1.147`, the recorded unequal-length case for Variant 1.1 at nine processes. This is about a 14.7% increase in work per unit time for the same task, or about a 12.8% reduction in elapsed time; those two percentages have different denominators.

If discussing efficiency, `speedup / p` describes speedup per allocated process. For worker-only variants, `p-1` is the arithmetic-worker count, but rank 0 still consumes resources and is included in total allocated process count. State your denominator rather than mixing definitions.

The recorded standard-send data show Variant 1.1 faster than Variant 1 in six of seven configurations, with a loss for equal 10,000-digit operands at 17 processes. The detailed numbers are in [information.md](information.md) and the CSVs. The largest required job width uses 17 processes on a host reporting 14 logical CPUs, so scheduling contention is possible.

These are single measurements, not averages. Sequential runs precede their parallel comparisons; warmed filesystem caches, run order, OS activity, communication overhead, and oversubscription can affect results. Do not present the observed best case as a guaranteed speedup or use these samples to claim a statistically established ranking.

### Supported behavior versus verified behavior

The source supports operands through 100,000 digits and process counts through 17 within the stated variant minimums. The required suite covers only the specified combinations. General partition, padding, empty-slice, and capacity arguments also rely on code reasoning. The guide's extra small examples are explanations, not claims that extra automated tests were run.

## 12. Presenting the assignment

### Suggested five-minute explanation

| Approximate time | What to explain | Source to point at |
| --- | --- | --- |
| First minute | Arbitrarily long decimal operands, LSD-first representation, and carry recurrence. | `common.hpp`: `read_digits`, `add_digits` |
| Second minute | Variant 1: I/O root, worker slices, ordered carries, streaming collection, and the no-buffering requirement. | `blocking.hpp`: `distribute_blocking`, `add_worker`, `collect_blocking` |
| Third minute | Variant 1.1: zero-carry computation followed by increment, preserving the original carry. | `add_worker`'s speculative branches; `increment_digits` |
| Fourth minute | Variant 2 uses all ranks and equal padded slices; Variant 3 separates initiation from completion with stable buffers. | `mpi2.cpp` final carry expression; `mpi3.cpp` request arrays and waits |
| Fifth minute | Required verification, synchronous-send results, total-time measurement, and noisy performance conclusions. | `run_tests.sh`, `run_mpi`, CSVs |

You can open with: "I store decimal digits from least significant to most significant. Splitting the digits creates a carry dependency between adjacent slices. The variants differ in who computes, how digits are distributed and returned, and whether work can begin before the incoming carry is known."

When showing a particular function, explain its input, output, precondition, and reason before reading individual expressions. For example: "`add_digits` receives a slice and a valid incoming carry, writes that slice's result, and returns the outgoing carry for the next slice."

### Comparisons to be able to make without looking at the code

| Property | 0 | 1 | 1.1 | 2 | 3 |
| --- | --- | --- | --- | --- | --- |
| Computing ranks | One sequential process | Ranks 1 through p-1 | Ranks 1 through p-1 | Ranks 0 through p-1 | Ranks 1 through p-1 |
| Root reads whole operands first | Yes | No | No | Yes | No |
| Distribution | None | Blocking sends | Blocking sends | Scatter twice | Nonblocking sends |
| Local addition begins before carry receive | Initial carry is zero | No | Yes, assumes zero | No | No |
| Return of digit results | Already local | Blocking sends | Blocking sends | Gather | Nonblocking sends |
| Root writes result slices as received | Complete result written | Yes, in rank order | Yes, in rank order | Complete gathered result written | Complete received result written |
| Main subtlety | Correct digit order and final carry | Deadlock-safe order | Correction and preserved carry | Padding versus true overflow | Requests and buffer lifetimes |

## 13. Practice questions and worked answers

Try answering aloud before reading the answer. You should be able to justify an answer using a function or expression, not only repeat its name.

### Concept questions

**1. Why store digits as bytes instead of an integer?** The inputs exceed fixed-width integer capacity. A byte easily stores a numerical decimal digit from zero through nine.

**2. Why is the least significant digit at index zero?** Carry moves from lower to higher significance, so forward iteration and increasing chunk offsets follow the dependency naturally.

**3. Is the value `9` the same as the character `'9'`?** No. The byte arrays store numerical nine. Text I/O converts between digit text and numerical values; casting to `int` when printing avoids character output.

**4. What is the difference between capacity and length?** Capacity is allocated storage, here 100017 elements. Length is the number of active input, padded, slice, or output digits relevant to that operation.

**5. How many computing ranks are there with `p=5`?** Four in Variants 1, 1.1, and 3; five in Variant 2. Root is included in `p` in every MPI job.

**6. Does rank 0 send a carry to worker 1 in Variant 1?** No. Worker 1 starts with zero. There are no lower digits that could generate an incoming carry.

**7. Why broadcast the length?** Workers need to know their counts before posting receives. They calculate them independently from the common maximum width and process count.

**8. Why send both operands before waiting for a worker's result?** Root must keep distribution advancing, including providing inputs to workers needed to receive earlier workers' carries. Receiving results too early can create a circular wait.

**9. Does blocking send mean the receiver has processed the data?** No. It means the send has completed sufficiently for the sender to reuse its buffer. Standard send may complete using internal buffering.

**10. What does synchronous-send verification check?** It removes reliance on the standard mode's possible buffering. The tested execution must still progress when a send needs a matching receive to participate.

**11. Can a nonblocking program still deadlock?** Yes. Nonblocking initiation does not fix missing matching operations or an incorrect wait dependency. The complete posting and waiting pattern must be safe.

**12. Why not read an Irecv buffer immediately?** Its receive may still be pending. Wait for the associated request before accessing it.

**13. Does Isend let the buffer go out of scope?** No. It initiates an operation on the original storage. The storage must remain alive and unmodified until completion.

**14. Does Variant 3 perform Variant 1.1's speculative addition?** No. Its worker waits for both operands and the incoming carry, then adds. The variants optimize different parts of the flow.

**15. Does scatter compute the sum, or gather combine the carries?** Neither. They distribute and collect data. Arithmetic and carry propagation are explicit code.

**16. Why does gather's receive count equal `count`, not `count*p`?** It is a per-sender count. MPI gathers one block of that size from each rank into the root's sufficiently large receive array.

**17. Why broadcast carry from `processes-1` in Variant 2?** That rank owns the most significant padded slice and knows the outgoing carry after the entire padded width.

**18. Why not always use that carry as the true final carry?** Distribution padding can absorb the true overflow as a normal result digit at index `N`. Inspect that digit when padding exists.

**19. Why not append a carry after every worker's result?** Intermediate carries belong in the next slice's arithmetic. Appending them as digits would insert incorrect positions and change the represented number.

**20. Why reserve header space?** Streaming output starts before the final carry is known. A fixed-width count field can be updated later without moving digit data.

**21. Why print only on root?** The script expects one timing value, and root is responsible for the one result file. Multiple normal timing lines would make CSV calculation ambiguous.

**22. Why is the maximum rank time used?** Parallel completion depends on the slowest measured participant. Summing counts overlapping intervals; averaging can hide a slow participant.

**23. Why might adding more ranks make this program slower?** Carry dependencies remain; root I/O is serial; communication and synchronization cost time; excess ranks can contend for CPUs. Local addition is very cheap relative to those costs.

**24. Does byte-for-byte agreement prove the arithmetic helper independently?** No. Sequential and parallel code share that helper. It is the assignment's requested validation, supported by arithmetic reasoning rather than an independent oracle suite.

### Calculation exercises

**A. Encode and add `58 + 67` by hand.**

Answer: `[8,5] + [7,6]`. Index zero gives 15, store five and carry one. Index one gives 12, store two and carry one. Append one: `[5,2,1]`, width three.

**B. Partition `N=11` over four workers. What are their counts and offsets?**

Answer: quotient two, remainder three. Counts are `[3,3,3,2]`, offsets `[0,3,6,9]`. Ranks are 1 through 4; recipient indices are 0 through 3. The counts sum to 11 and adjacent ranges meet without overlap.

**C. A speculative result is `[9,9,4]`, outgoing zero, incoming one. What is sent onward?**

Answer: increment gives `[0,0,5]` with overflow zero; outgoing remains zero. For `[9,9]` with outgoing zero, correction gives `[0,0]` and outgoing one. For a speculative sum `99+99=198`, result `[8,9]` and outgoing one become `[9,9]` and outgoing one after incoming one.

**D. Variant 2 has `N=1000`, `p=16`. What is each count and the padded width?**

Answer: `(1000+15)/16 = 63`, padded width `63*16 = 1008`. Rank `r` starts at `63*r`. Padding occupies global indices 1000 through 1007. If index 1000 contains one after addition, the true output needs 1001 digits.

**E. Variant 3 has nine processes. How many root requests are active? Which slots belong to worker 4?**

Answer: eight workers, 16 operand sends, nine receives including final carry. Worker 4 uses send slots six and seven and result-receive slot three. Final carry uses receive slot eight. The fixed arrays have more capacity than these active prefixes.

**F. Sequential time is 8 ms and parallel time is 10 ms. What is the speedup?**

Answer: `8/10 = 0.8`, meaning this parallel execution is slower. Its elapsed time increased by 25% relative to the sequential eight milliseconds. More processes alone do not guarantee an improvement.

### Find the bug

| Incorrect change | Explanation |
| --- | --- |
| Use `read_digits` on one reusable buffer immediately after `MPI_Isend` from that buffer. | It may overwrite an active send region. Wait first or use stable, disjoint regions as the implementation does. |
| Set Variant 1.1's outgoing carry only to the increment helper's return value. | Loses a carry already produced by the speculative addition. |
| Use logical OR and skip calling increment when outgoing is already one. | The result digits still need correction even when outgoing carry is already known to be one. |
| Send the final-carry message only when it is one. | Root always receives it, so a zero carry can cause a hang. |
| Pass full request-array capacity to Waitall instead of active count. | Unused entries were not initialized as valid requests or null handles. |
| Put scatter only in root's branch. | Other ranks never participate in the required collective. |
| Use the padded final carry without examining `result[N]`. | May drop a true final digit that was absorbed into padding. |
| Reverse worker output order. | The file would no longer represent the assembled result in LSD-first global position order. |
| Append a carry before completing the root's Irecv into `carry`. | Reads an in-progress receive buffer and may use stale or invalid data. |

## 14. Final revision sheet

Be able to state these facts without looking them up:

- `N = max(N1,N2)`. Missing operand digits are zeroes. Output has `N` or `N+1` digits.
- Digits are numerical bytes in LSD-first order. Carries flow toward increasing offsets and ranks.
- Local recurrence: `s=a+b+c`, digit `s%10`, carry `s/10`. Carry is zero or one.
- Variants 1, 1.1, and 3 use `p-1` computing workers. Variant 2 uses all `p` ranks.
- Balanced chunks: quotient plus one for the first remainder recipients; offset equals earlier base digits plus earlier extras.
- Variant 1: receive operands, receive carry, add, forward carry, return result; root distributes first and then streams result slices in order.
- Variant 1.1: add with zero carry before receiving real carry, increment if needed, preserve either source of outgoing carry.
- Variant 2: read/pad, broadcast length, scatter twice, ordered carry/add, gather, broadcast last carry, remove distribution padding correctly.
- Variant 3: post receives and sends on stable regions, wait for active requests, then use results or release storage. Nonblocking does not remove the carry dependency.
- Tags identify purpose, datatypes describe elements, and counts describe how many elements a message uses.
- Standard sends must not be assumed buffered. Synchronous-send verification and an ordering argument support deadlock safety.
- Clock intervals include input, communication, arithmetic, and completed output. The timing reduction and launcher startup are outside the reported interval.
- Required validation: four sequential baselines, 28 standard parallel runs, 14 synchronous parallel runs, 42 byte comparisons. Recorded timings are single-run observations.

You are ready to present when you can trace one complete example across ranks, explain every argument of an MPI call, justify both outgoing-carry cases in Variant 1.1, explain Variant 2's padding expression, and identify when each Variant 3 buffer becomes safe to reuse.

# Lab 2 remake — How to explain Variant 1b confidently

## A short opening explanation

“Lab 1 applied a convolution filter and stored the output in a second matrix. Lab 2 must overwrite the input matrix, so the challenge is preserving the original neighbours that later calculations still need. I use two rolling row vectors sequentially and three vectors per worker in parallel. Workers own contiguous row ranges. Each worker snapshots its own original neighbours, all workers wait at a barrier, and only then do they overwrite the image. This is Variant 1b.”

## What convolution does

An image is a matrix of numbers. A 3×3 kernel supplies nine weights. For each output pixel, multiply the nine neighbouring original pixels by the corresponding weights and add the products. A different kernel gives a different operation, such as selecting a neighbour or detecting changes.

The implementation follows Lab 1's formula directly: weights are not flipped. Strict mathematical convolution flips the kernel; this course's supplied formula corresponds to cross-correlation. Explain the course formula rather than silently changing it.

At an edge, missing neighbours repeat the closest valid pixel. For this image and kernel:

```text
image          kernel
1 2            1 2 3
3 4            4 5 6
               7 8 9
```

The top-left neighbourhood is:

```text
1 1 2
1 1 2
3 3 4
```

So the top-left output is `1*1 + 1*2 + 2*3 + 1*4 + 1*5 + 2*6 + 3*7 + 3*8 + 4*9 = 111`. The full result is:

```text
111 126
141 156
```

This asymmetric kernel is useful for testing because reversing weights or mixing up directions changes the answer. An identity kernel alone would not catch those errors.

## Why directly overwriting the image is wrong

Suppose the top-left result, 111, replaces the original 1. Computing the next pixel must still use the original 1. Reading the live matrix would accidentally use 111 instead. Moving to the next row has the same problem: the preceding row now contains results.

Saving only the previous row is therefore insufficient; the current row must also remain available in its original form throughout the row computation.

## Walk through the sequential buffers

Name the original image rows `A`, `B`, `C`, and `D`. A prime denotes a filtered row. Buffer contents are copies, not references to rows that will be overwritten.

| Row being computed | `previous` | `current` | next source | Image after this step |
|---|---|---|---|---|
| 0 | A | A | live B | A′, B, C, D |
| 1 | A | B | live C | A′, B′, C, D |
| 2 | B | C | live D | A′, B′, C′, D |
| 3 | C | D | saved D (`current`) | A′, B′, C′, D′ |

After row 0, swap buffer references/storage: the old `current` A becomes `previous`. The old `previous` vector becomes reusable storage; copy live B into it to become `current`. Repeat this pattern. Swapping does not copy the whole row; the subsequent refill does.

The invariant to state is: **before row i is computed, the saved previous/current vectors and selected next source all contain original pixels.** It is true initially, each iteration preserves it, and each output cell is consequently calculated from the correct original neighbourhood.

Only two vectors are needed sequentially because future rows have not yet been modified. No third output-row vector is needed: the destination can be the image itself because source values come from saved or untouched rows.

## Split work between threads

Let `q=min(p,N)`. For worker `t`, use integer division:

```text
start = t*N/q
end   = (t+1)*N/q
```

`start` is included and `end` is excluded. For seven rows and three workers, the ranges are `[0,2)`, `[2,4)`, and `[4,7)`: two, two, and three rows. Every row has one owner and the workload differs by at most one row.

The code widens multiplication before dividing so the partition calculation does not overflow a 32-bit intermediate. When p>N, only N workers are created; there are no empty worker ranges.

## The thread-boundary race

Consider workers owning `[0,2)` and `[2,4)`. Worker 0 needs original row 2 while finishing row 1. Worker 1 might already have overwritten row 2. Worker 1 also needs original row 1 while computing row 2, which worker 0 might have overwritten.

Disjoint writes alone do not make this correct. A read can still race with another worker's write.

Each worker begins by preparing its own vectors, while every other worker is also restricted to snapshotting:

1. `previous`: original row `max(start-1,0)`.
2. `current`: original row `start`.
3. `following`: original row `min(end,N-1)`.

After copying, each worker waits at the shared barrier. Only after **all** workers have arrived may any worker start filtering. An early worker can reach the barrier first, but cannot overwrite a row before a later worker has copied it.

Inside a range, the next row is safe to read from the image because only the current worker owns it and processes rows forward. At the final row of a range, use `following`. At the global bottom edge, use saved `current` for replicated padding.

This also works when a worker owns just one row: all required source rows were copied before computation began.

## Why Variant 1b needs a barrier

The workers now copy their own boundaries after starting. A fast worker must wait for slower workers to finish copying before it modifies the image. C++ uses a shared `std::barrier<>`, constructed with the active worker count; each worker calls `arrive_and_wait()`. Java uses one `CyclicBarrier`, and each worker calls `await()`. Main is not a barrier participant.

The barrier is used once per execution, not once per image row. It separates the read-only snapshot phase from the write phase. One worker also works correctly: the barrier has one participant and releases immediately.

A join is not a substitute: joining happens after computation and cannot stop an early worker from modifying data before a late worker has copied it. Conversely, the barrier does not replace joins: main still needs to wait until the full filtering computation is finished before writing output.

Workers write disjoint rows and never read another worker's live pixels. Kernel data and image structure remain unchanged during computation. The image container is not resized while workers run. The main thread joins all workers before writing the output.

In Java, `start()` publishes the original image and configuration to each worker. The barrier ensures the snapshot phase finishes before filtering can begin; `join()` makes completed worker writes visible to main. C++ thread startup, barrier completion, and joining provide the corresponding synchronization. Separate row buffers are local to separate workers; they are not shared scratch space.

No lock is required around each output pixel. Adding one would serialize work and would not, by itself, restore pixels that were already overwritten.

## Explain the memory claim precisely

Let N be rows and M columns. The image contains NM integers. The fixed kernel contains nine integers.

- Sequential scratch: two vectors of length M, or O(M).
- Parallel scratch: three vectors per active worker, or O(qM), where q=min(p,N).
- Thread metadata, runtime stacks, row metadata, and fixed I/O buffers also exist.
- Total image-plus-buffer storage remains O(NM), since q<=N.

For the required square cases and a fixed number of threads, auxiliary row storage is linear in image side length. If asked whether O(pM) is always O(M), answer **no**: that simplification only holds when p is treated as a constant.

At 10000×10000, image integers occupy 400 MB in decimal units. Sequential vectors occupy 80 KB. Sixteen workers' vectors occupy 1.92 MB. Lab 1 additionally needed a second 400 MB image. Real process memory exceeds these payload numbers, especially for the JVM.

Time remains O(NM) sequentially because there are nine multiplications per cell. Each parallel worker now copies its own three rows in O(M); total snapshot work is O(qM), but it is no longer a serial loop in main. Ideal parallel time is roughly O(M + NM/q), ignoring rounding, thread creation, allocation contention, barrier waiting, scheduling and joins. The barrier changes coordination, not the O(qM) row-storage bound.

## Navigate the code during an explanation

| Code | What to point out |
|---|---|
| `read_input` / `readInput` | Loads the only image matrix and the 3×3 kernel |
| `RowBuffers` | Copies original rows; parallel version adds the following boundary |
| `compute_row` / `computeRow` | Applies the nine weights with clamped column indices |
| `compute_range` / `computeRange` | Selects safe source rows, writes results, rotates and refills buffers |
| C++ `par.cpp`, Java `Par.main` / `Worker.run` | Partitions rows, starts workers, copies snapshots inside workers, waits at the barrier, filters, joins |
| `write_output` / `writeOutput` | Writes the now-filtered original matrix |
| `verify.py` | Small independent reference and edge/race checks |
| `run_tests.sh` | Validates options, builds, runs independent checks, executes benchmarks, compares outputs and calculates means |
| `same_content` / `run_program` in `run_tests.sh` | `cmp -s` comparison, per-run `OK`/`WRONG`, failure counts and CSV timing rows |

Java `clone()` copies a row; assigning `current=image[start]` would only alias the original row and break the algorithm. In C++, copying a `vector<int>` copies its elements. The thread lambda captures the range bounds by value and the shared barrier by reference, then creates its own local `RowBuffers`; there is no main-owned collection of prepared buffers and no `std::ref` argument.

Buffered integer I/O is retained from Lab 1 because the largest text input has 100 million values. It uses fixed-size byte buffers rather than splitting the whole file into strings. This matters for memory, but it is separate from the convolution algorithm.

## Explain the experiments

The required cases are 10×10 with two threads, and 1000×1000 / 10000×10000 with 2, 4, 8, and 16 threads, plus sequential execution in both languages. Each configuration runs ten times on the same generated input. Because convolution overwrites its input, every run reloads the file rather than filtering an already-filtered image.

Computation timing includes row-buffer allocation/copying inside workers, thread creation, the barrier wait, filtering, and joining. It excludes reading, writing, and correctness comparison. Total time also includes in-program I/O, but excludes process/JVM startup before main.

C++ uses `std::chrono::high_resolution_clock` through the shared `Clock` alias; Java uses `System.nanoTime()`. `high_resolution_clock` means the clock with the shortest tick period, not a guarantee that the algorithm runs faster. It is not guaranteed to be monotonic on every platform; its `is_steady` property depends on the standard library. On this machine, a compiled check confirmed it is the same type as `steady_clock` and `is_steady` is true. The name and portability behavior are specified in the [C++ standard draft, section time.clock.hires](https://isocpp.org/files/papers/n4296.pdf).

### Walk through the test script

Run `bash scripts/run_tests.sh` from the assignment directory for the full default suite. For a quick example, use `CASES="10 10 2; 7 13 2 3 16" bash scripts/run_tests.sh 2`. Quick runs also replace result files, so save the existing CSVs first if you need the full assignment measurements.

The shell script validates the settings, calls `build.sh`, and runs the independent Python `verify.py` checks. It then generates one input per case and saves an unmeasured C++ sequential output as `build/output_seq.txt`. Each measured execution reloads the input and writes `build/output.txt`.

The `run_program` function captures `<compute_ms> <total_ms>` from the program, then uses `same_content` to compare these two output files with `cmp -s`. Matching files produce `OK`; different or unreadable files produce `WRONG` and increment `failures`. Comparison takes place after the program's timers have finished and uses bounded memory even for large outputs.

Output mismatches do not stop the next test: they are counted and reported with a final nonzero exit. A program/build/check failure stops the script immediately. If all comparisons pass, `awk` calculates means and speedups from `times.csv` and writes `averages.csv`. On failure, previous averages may still exist, so their presence alone does not prove that the latest run passed. Correctness statuses are printed to the console and are not CSV columns.

Bash handles execution and comparison; Python is the small independent oracle. This remake completed a fresh full 240-execution suite for Variant 1b on 30 October 2026 and retains its own CSV files; all outputs matched the sequential reference. Results from the original main-thread-snapshot implementation must not be presented as measurements of this barrier implementation.

The runner compares **every** measured output to a fixed sequential reference using bounded-memory file reads. An independent small reference is also needed: agreement among implementations alone cannot rule out an identical bug in all of them. The test oracle may use a result matrix because it is separate test code operating on small examples, not the submitted production convolution.

The reported mean is the sum of ten times divided by ten. Speedup is `mean Tseq / mean Tpar` within the same language. Above 1 means faster, below 1 means slower. For example, 100 ms sequential and 40 ms parallel gives 2.5× speedup; with four workers, efficiency would be 2.5/4 = 62.5%.

For small matrices, expect thread overhead to dominate. For larger matrices, parallelism can help, but memory traffic, barrier waiting, core count, and scheduling limit scaling. More threads do not guarantee better performance. This machine has heterogeneous cores, so equal row counts do not guarantee equal completion times.

Java starts a fresh JVM per run with no warm-up; computation may include JIT compilation. C++ is compiled ahead of time with optimization. This benchmark compares these actual execution conditions; it does not isolate warmed-up steady-state kernel performance. Do not attribute a difference solely to a language or cache behavior without a separate experiment supporting that explanation.

Read the measured tables and analysis in [README.md](README.md#measured-results). Use `times.csv` to inspect variation and `averages.csv` for the reported means. Explain observed numbers first, then distinguish possible causes from directly established facts.

## Likely oral questions

| Question | Short answer |
|---|---|
| What changed from Lab 1? | The output overwrites the original image, so dependencies need saved original rows. |
| Why two sequential vectors? | Previous rows are overwritten, and the current row changes while computing its columns. |
| Why a third vector per worker? | The next worker may overwrite the following boundary row at any time. |
| Why copy, then wait at a barrier? | A fast worker must not overwrite a row before a slower worker has copied it. |
| Why does a join not replace this barrier? | Join waits for completion; it does not separate snapshotting from writes. |
| How many times is the barrier used? | Once per worker per execution, before any filtering. |
| Could workers share one scratch row? | No; they need independently preserved data and update buffers concurrently. |
| Why row distribution? | It is required, gives contiguous work, and makes a 3×3 stencil manageable with row vectors. |
| Why no per-pixel mutex? | After the barrier, row ownership and private snapshots eliminate overlapping writes and live cross-worker reads. |
| Why join? | Output must wait for all rows, and main must observe completed writes. |
| What happens at an image corner? | Both indices clamp to the nearest valid row and column. |
| What if p>N? | Use N active threads, one per row. |
| Does in-place mean zero extra memory? | No; it means the image is reused as the destination, with bounded row scratch. |
| Is auxiliary memory independent of p? | No; parallel vectors use O(pM), with p capped by N. |
| Why not repeatedly filter the same matrix for timing? | That changes the input after the first run, so measurements would no longer use the same data. |
| Why can small parallel runs be slower? | Starting and joining workers costs more than the tiny computation. |
| Which C++ clock is used? | `std::chrono::high_resolution_clock`, through the shared `Clock` alias. |
| Is that clock always monotonic? | No; check `is_steady`. It is steady on the tested machine. |
| Where are output files compared? | `run_program` calls `same_content`, which uses `cmp -s` in `run_tests.sh`. |
| What happens if an output differs? | Print `WRONG`, count the mismatch, and ultimately exit unsuccessfully without writing new averages. |
| Is this distributed computing? | This assignment uses shared-memory threads; there are no networked processes. |
| Are arbitrary integers safe? | Only if all products and partial sums fit int; generated bounds guarantee this. |

## A simple presentation order

1. State the in-place requirement and the incorrect direct-overwrite example.
2. Show the sequential buffer table and its invariant.
3. Show row ownership and the two boundary hazards.
4. Explain worker snapshots, the shared barrier, and why no per-pixel mutex is needed.
5. Give O(M) / O(pM) auxiliary memory and the concrete memory savings.
6. Explain correctness checks, what is timed, and the observed performance tables.


## Failure handling and limits

C++ main handles a thread-start failure by dropping the unstarted participants from the barrier, joining already-started workers, and returning failure. Otherwise those workers could wait forever for threads that do not exist. An exception escaping a C++ worker, such as failure to allocate its row buffers, terminates the process rather than being caught in main.

Java treats worker and startup failures as fatal for this command-line run, including interrupted or broken barrier waits: it reports the error and exits unsuccessfully. Catching `Throwable` at this boundary also handles allocation/startup errors so peers are not left waiting. There is no recovery or partial output on that failure path. The independent verification commands have a 30-second timeout so a synchronization regression fails a check rather than hanging the suite.

Only Variant 1b is implemented. [utils.md](utils.md) explains the other supplied variants and their qualitative space/performance tradeoffs; it does not claim measured results for unimplemented variants.


## Results to explain for this remake

All 212 independent checks and all 240 required benchmark outputs passed. The additional sanitizer, Java interruption, and C++ thread-start failure checks also passed; details are in the README. The largest image achieved a best observed computation speedup of about 7.47× for C++ with sixteen workers and 4.76× for Java with eight. Smaller tasks show why coordination overhead matters, and file I/O limits the total program speedup to roughly 1.10× for the largest case. These measurements include the barrier and belong to this remake, not the original implementation.

## C++ presentation walkthrough, following the source line by line

Use this section while pointing at the code in the editor. The right column is phrased as something I can say to the professor. Line numbers refer to the current source files; update them if the code changes. Blank lines separate ideas, and closing braces end the surrounding loop, conditional, function, or class, so they do not need a separate spoken explanation.

I present the shared data and algorithm first, then the sequential and parallel entry points, and finally the input generator. I skip `Reader` and `Writer` internals in `cpp/conv.hpp` (lines 24–103); their public use in the input/output helpers is covered below. These utilities handle buffered files rather than the parallelism itself.

### Shared declarations and error handling

Source: [cpp/conv.hpp](cpp/conv.hpp).

| Line(s) | Code | What I say to the professor |
|---|---|---|
| 1–2 | `// Shared image, buffered I/O and in-place convolution for Seq and Par.`<br>`#pragma once` | I put the shared image, I/O helpers, and convolution functions in one header. `#pragma once` prevents repeated inclusion within a single translation unit. |
| 4 | `#include <algorithm>` | I use `<algorithm>` for `min`, `max`, and `copy`. |
| 5 | `#include <chrono>` | I use `<chrono>` for the clock, time points, and duration conversion. |
| 6 | `#include <cstdio>` | I use `<cstdio>` for C-style file operations and printing, including the buffered I/O helpers. |
| 7 | `#include <cstdlib>` | This includes the general C library utilities. It is not directly needed by the current code; it does not perform any work by itself. |
| 8 | `#include <climits>` | I use `<climits>` for `INT_MAX` when the reader validates integers; I will skip the reader internals. |
| 9 | `#include <stdexcept>` | I use `<stdexcept>` for `runtime_error` and standard exception handling. |
| 10 | `#include <string>` | I use `<string>` for error messages and parsing the thread-count argument. |
| 11 | `#include <vector>` | I use `<vector>` for dynamically allocated image rows and temporary row vectors. |
| 13 | `using namespace std;` | This lets me write standard-library names without repeating `std::`. It is a convenience in this small lab, rather than a requirement of the algorithm. |
| 14 | `using Clock = chrono::high_resolution_clock;` | I define `Clock` as `std::chrono::high_resolution_clock`, so both programs use the same clock. The correct standard name is `high_resolution_clock`. On this machine it aliases `steady_clock`; that is not guaranteed on every platform. |
| 16 | `int rows, cols;` | These hold the image dimensions read from the input file. |
| 17 | `vector<vector<int>> image; // The only image-sized matrix; vector storage is dynamic.` | This is the only image-sized matrix. Both the outer vector and its row vectors allocate their storage dynamically. I overwrite its values with the filtered results. |
| 18 | `int kernel[3][3];` | The kernel has exactly nine weights. Its size is fixed because the assignment only requires 3×3 filtering. The dynamic-allocation requirement is met by the image; the tiny fixed kernel is separate. |
| 20–22 | `[[noreturn]] void fail(const string &message) {`<br>`throw runtime_error(message);`<br>`}` | `fail` throws a standard exception with the supplied message. `[[noreturn]]` says the function never returns normally. The main program catches the error, prints it, and returns a failure status. |

### Loading the one image and the fixed kernel

Source: [cpp/conv.hpp](cpp/conv.hpp).

| Line(s) | Code | What I say to the professor |
|---|---|---|
| 105–107 | `// date.txt: rows cols, image values, kernel size (3), kernel values.`<br>`void read_input(const char *path) {`<br>`Reader input(path);` | This helper loads one input file. `path` is the filename, and the local `Reader` supplies integers. I treat the reader as a buffered input utility and skip its implementation. |
| 108 | `rows = input.next_int();` | The first integer is the number of rows. |
| 109 | `cols = input.next_int();` | The second integer is the number of columns. |
| 110 | `if (rows < 1 \|\| cols < 1) fail("rows and columns must be positive");` | I reject nonpositive dimensions before allocating the image. |
| 111 | `image.resize(rows);` | I create one row-vector object per image row. These row vectors are initially empty. |
| 112 | `for (auto &row : image) {` | I visit each actual row by reference; I am not copying the row. |
| 113 | `row.resize(cols);` | I allocate that row with exactly `cols` integers. |
| 114 | `for (int &value : row) value = input.next_int();` | I read every pixel directly into its position in the row. `int &value` refers to the stored pixel, so assignment changes the image. |
| 116 | `if (input.next_int() != 3) fail("kernel size must be 3");` | After the pixels, the file gives the kernel size. I accept only 3. This algorithm is deliberately specialized to the required kernel and does not claim to support 5×5 filters. |
| 117 | `for (auto &row : kernel)` | I visit the three kernel rows by reference. |
| 118 | `for (int &value : row) value = input.next_int();` | I fill their nine weights from the same input file. When this function returns, the local reader closes its file automatically. |

### Saving the result

Source: [cpp/conv.hpp](cpp/conv.hpp).

| Line(s) | Code | What I say to the professor |
|---|---|---|
| 121–122 | `void write_output(const char *path) {`<br>`Writer output(path);` | This helper opens the chosen output file using the buffered writer. I skip the writer internals, but show which values the program sends to it. |
| 123–126 | `output.put_int(rows);`<br>`output.put_char(' ');`<br>`output.put_int(cols);`<br>`output.put_char('\n');` | I write the row count, one space, the column count, and a newline. That is the output header. |
| 127 | `for (const auto &row : image) {` | I visit each image row through a const reference, so writing the output neither copies nor changes the row. |
| 128 | `for (int j = 0; j < cols; ++j) {` | I visit every column in the row. |
| 129 | `if (j) output.put_char(' ');` | I insert a space before every value except the first. This avoids a trailing space. |
| 130 | `output.put_int(row[j]);` | I write the value already stored in the original image matrix, which now contains the filtered result. |
| 132 | `output.put_char('\n');` | I end each matrix row with a newline. |
| 134 | `output.flush();` | I flush the remaining buffered output before returning. Output writing is outside the computation timer. |

### Preparing the original-row snapshots

Source: [cpp/conv.hpp](cpp/conv.hpp).

| Line(s) | Code | What I say to the professor |
|---|---|---|
| 137–138 | `struct RowBuffers {`<br>`vector<int> previous, current, following;` | I group a worker's scratch vectors into `RowBuffers`: original previous row, original current row, and an optional following boundary row. This struct contains vectors, not another image matrix. |
| 140 | `RowBuffers(int start, int end, bool parallel)` | The constructor receives the worker's half-open row range `[start,end)` and whether it is preparing parallel work. |
| 141 | `: previous(image[max(0, start - 1)]), current(image[start]) {` | The initializer list copies the preceding original row into `previous` and the first owned row into `current`. `max(0,start-1)` repeats the top edge for the first range. Copying a vector copies its elements, so these buffers do not alias writable image rows. |
| 142–143 | `// Preserve the neighbouring row; every worker copies before the barrier opens.`<br>`if (parallel) following = image[min(end, rows - 1)];` | In parallel mode I also copy row `end`, clamped at the global bottom edge. This row belongs to the next worker, so I preserve it before reaching the barrier; no worker may overwrite anything until all snapshots are ready. Sequential mode leaves `following` empty and uses only two vectors. |

### Computing one row

Source: [cpp/conv.hpp](cpp/conv.hpp).

| Line(s) | Code | What I say to the professor |
|---|---|---|
| 147–149 | `// Read only original values from the supplied rows; write directly into the image.`<br>`void compute_row(int i, const vector<int> &previous, const vector<int> &current,`<br>`const vector<int> &next) {` | This function filters image row `i`. The three source rows are passed by const reference: no row copies are made here, and the function cannot change those sources through these parameters. |
| 150 | `for (int j = 0; j < cols; ++j) {` | I compute every output column in the row. |
| 151 | `int sum = 0;` | Each output pixel starts with a zero accumulator. |
| 152 | `for (int offset = -1; offset <= 1; ++offset) {` | The horizontal offset visits left, centre, and right: −1, 0, and 1. |
| 153 | `int column = max(0, min(cols - 1, j + offset));` | I clamp the source column into `[0,cols-1]`. At the left or right edge, this repeats the nearest valid pixel instead of reading outside the row. |
| 154 | `sum += previous[column] * kernel[0][offset + 1];` | I add the contribution from the original previous row. `offset+1` maps −1, 0, 1 to kernel columns 0, 1, 2. |
| 155 | `sum += current[column]  * kernel[1][offset + 1];` | I add the contribution from the original current row using the middle kernel row. |
| 156 | `sum += next[column]     * kernel[2][offset + 1];` | I add the contribution from the original next row using the bottom kernel row. Across the three offset iterations, these three statements produce all nine products. |
| 158 | `image[i][j] = sum;` | I write the completed sum directly into the image. It is safe because none of the three source rows is the destination row being overwritten: the original current row was saved separately. Generated pixels and weights guarantee the integer sum fits in range. |

### Processing a range and rotating buffers

Source: [cpp/conv.hpp](cpp/conv.hpp).

| Line(s) | Code | What I say to the professor |
|---|---|---|
| 162–163 | `// One worker owns [start, end). There is no shared write inside this range.`<br>`void compute_range(int start, int end, RowBuffers &buffers) {` | This function filters one half-open range of rows. It takes the prepared buffers by non-const reference because it will rotate and refill them. Exactly one worker owns the destination rows in this range. |
| 164 | `for (int i = start; i < end; ++i) {` | I process the owned rows from top to bottom. That order keeps future interior rows untouched until they are needed. |
| 165 | `const vector<int> *next;` | `next` is only a pointer to an existing row vector. Declaring it allocates no new pixel buffer. |
| 166 | `if (i == rows - 1) next = &buffers.current;` | At the last row of the entire image, the bottom neighbour repeats the original current row. I use the saved vector, not the live image row. |
| 167 | `else if (i == end - 1 && !buffers.following.empty()) next = &buffers.following;` | At the last row of a parallel range, I use the saved following boundary. The next worker may already have overwritten the corresponding image row, so reading it live would be a race. |
| 168 | `else next = &image[i + 1];` | Otherwise, the next row is still untouched inside this worker's own range, so it can be read directly from the image. In sequential mode, `following` is empty and this also handles all non-bottom rows. |
| 169 | `compute_row(i, buffers.previous, buffers.current, *next);` | I dereference the selected pointer and compute the row using three original source rows. Dereferencing passes a reference; it does not copy the row. |
| 170 | `if (i + 1 < end) {` | I prepare buffers only if there is another row in this range. |
| 171 | `buffers.previous.swap(buffers.current);` | I swap the two vector objects' storage. The old original current row becomes `previous`; the other allocation becomes reusable space for the new current row. This swap does not copy M pixels. |
| 172–173 | `// This row is still original: only this worker can overwrite it.`<br>`copy(image[i + 1].begin(), image[i + 1].end(), buffers.current.begin());` | I copy the next untouched image row into the reusable current buffer. `begin()` and `end()` describe the source elements; the destination starts at `current.begin()`. This is an O(M) copy into existing storage. The invariant is now ready for the next iteration. |

### Thread-count parsing and timing output

Source: [cpp/conv.hpp](cpp/conv.hpp).

| Line(s) | Code | What I say to the professor |
|---|---|---|
| 178 | `int parse_threads(const string &text) {` | This helper converts the command-line thread count from text to an integer. |
| 179 | `size_t consumed;` | `consumed` will record how many characters were parsed. |
| 180 | `int count = stoi(text, &consumed);` | `stoi` performs the conversion and reports the consumed length. Invalid or out-of-range conversions throw exceptions caught by main. |
| 181 | `if (consumed != text.size() \|\| count < 1) fail("threads must be a positive integer");` | I require the whole argument to be consumed and the value to be positive. Thus `2x`, zero, and negative counts are rejected. |
| 182 | `return count;` | I return the validated count. |
| 185–186 | `void print_times(Clock::time_point program_start, Clock::time_point compute_start,`<br>`Clock::time_point compute_end) {` | The timing helper accepts three time points: program start, computation start, and computation end. |
| 187 | `double compute_ms = chrono::duration<double, milli>(compute_end - compute_start).count();` | I subtract computation start from computation end, convert the duration to floating-point milliseconds, and extract its numeric value with `count()`. |
| 188 | `double total_ms = chrono::duration<double, milli>(Clock::now() - program_start).count();` | I sample the clock again after output writing and subtract program start to obtain total in-program milliseconds. Startup before main and final process teardown are not included. |
| 189 | `printf("%.6f %.6f\n", compute_ms, total_ms);` | I print computation time followed by total time with six digits after the decimal point. `run_tests.sh` reads exactly these two values; output matrices are written to files instead. |

### Sequential entry point

Source: [cpp/seq.cpp](cpp/seq.cpp).

| Line(s) | Code | What I say to the professor |
|---|---|---|
| 1 | `#include "conv.hpp"` | I include the common implementation, so the sequential and parallel programs use the same input format, edge rules, and row-filtering functions. |
| 3 | `int main(int argc, char **argv) {` | The entry point receives the argument count and argument strings. `argv[0]` is the executable name. |
| 4 | `if (argc != 3) {` | I require three arguments in total: executable name, input path, and output path. |
| 5 | `fprintf(stderr, "usage: %s <input_file> <output_file>\n", argv[0]);` | If the arguments are wrong, I print the expected command format to standard error. |
| 6 | `return 1;` | I return a nonzero status so the test script knows execution failed. |
| 8 | `try {` | I put the operation in a try block so ordinary parsing, allocation, and I/O exceptions can be reported cleanly. |
| 9 | `auto program_start = Clock::now();` | I start the total in-program timer before reading input. |
| 10 | `read_input(argv[1]);` | I load the image and the kernel from the first user-supplied path. |
| 11 | `auto compute_start = Clock::now();` | I start computation timing only after input reading is complete. |
| 12 | `RowBuffers buffers(0, rows, false);` | I prepare buffers for the entire image range `[0,rows)`. The flag is false, so only previous and current row vectors contain pixel storage. |
| 13 | `compute_range(0, rows, buffers);` | I filter every row in place with the shared range function. There are no worker threads in this program. |
| 14 | `auto compute_end = Clock::now();` | I stop computation timing before writing the output. |
| 15 | `write_output(argv[2]);` | I save the filtered image to the second user-supplied path. |
| 16 | `print_times(program_start, compute_start, compute_end);` | I print computation and total time in the script's expected format. |
| 17 | `} catch (const exception &error) {` | I catch a standard exception by const reference, without copying it. |
| 18 | `fprintf(stderr, "error: %s\n", error.what());` | I print its explanation to standard error, keeping normal timing output separate. |
| 19 | `return 1;` | I return failure. If the normal path reaches the end of main, C++ implicitly returns zero. |

### Parallel entry point

Source: [cpp/par.cpp](cpp/par.cpp).

| Line(s) | Code | What I say to the professor |
|---|---|---|
| 1 | `#include <barrier>` | I include `<barrier>` for the standard C++20 synchronization primitive; this remake is built with `-std=c++20`. |
| 2 | `#include <thread>` | I include `<thread>` for explicitly created worker threads. |
| 3 | `#include "conv.hpp"` | I reuse the shared image, convolution, and I/O helpers. |
| 5 | `int main(int argc, char **argv) {` | This is the parallel command-line entry point. |
| 6 | `if (argc != 4) {` | I require the executable name, input path, output path, and requested thread count. |
| 7 | `fprintf(stderr, "usage: %s <input_file> <output_file> <threads>\n", argv[0]);` | I print the expected command format when the argument count is wrong. |
| 8 | `return 1;` | I return failure for an invalid command. |
| 10 | `try {` | The outer try block reports ordinary setup and I/O exceptions. |
| 11 | `auto program_start = Clock::now();` | I start total in-program timing. |
| 12 | `int requested_threads = parse_threads(argv[3]);` | I parse and validate the requested positive thread count. |
| 13 | `read_input(argv[1]);` | I load the original image and kernel before starting workers. |
| 14 | `auto compute_start = Clock::now();` | I start computation timing before synchronization setup, thread creation, and worker snapshots. |
| 15 | `int count = min(requested_threads, rows);` | I cap active workers at the row count so each gets at least one row. |
| 16 | `barrier<> snapshots_ready(count);` | I create one shared barrier with exactly this many participants. The empty template brackets select the default completion behavior. Main does not arrive at the barrier; only workers do. |
| 17 | `vector<thread> workers;` | I create the container of thread objects. There is no collection of main-thread snapshots in Variant 1b. |
| 18 | `workers.reserve(count);` | I reserve its capacity before starting threads, avoiding reallocation of the container during startup. |
| 19 | `try {` | The inner try block handles partial thread-start failure. |
| 20 | `for (int t = 0; t < count; ++t) {` | I create one thread for each balanced row range. |
| 21 | `int start = static_cast<long long>(t) * rows / count;` | I compute its inclusive start using wide multiplication before integer division. |
| 22 | `int end = static_cast<long long>(t + 1) * rows / count;` | I compute its exclusive end using the same partition formula. |
| 23 | `workers.emplace_back([start, end, &snapshots_ready] {` | I start a thread running this lambda. The range bounds are captured by value, so later loop iterations cannot change them. The barrier is captured by reference so every worker reaches the same object; it remains alive until after all joins. |
| 24–25 | `// Variant 1b: each worker saves its original neighbours first.`<br>`RowBuffers buffers(start, end, true);` | Inside the worker, I create private buffers and copy its original previous, current, and following rows. This is step 1 of Variant 1b, not work done in main. |
| 26–27 | `// Nobody may overwrite the image until ALL snapshots are ready.`<br>`snapshots_ready.arrive_and_wait();` | The worker arrives and waits. Only after all workers finish copying can the barrier release them. This is step 2 of Variant 1b and prevents a fast worker from overwriting another worker’s source before it is copied. |
| 28 | `compute_range(start, end, buffers);` | After the barrier, I run the existing in-place rolling-buffer algorithm on this worker’s range. This is step 3 of Variant 1b. |
| 29 | `});` | This closes the lambda and the thread construction. Its local buffers are destroyed automatically when that worker finishes. |
| 31 | `} catch (...) {` | If construction of a later thread throws, this handler runs in main. |
| 32–34 | `// A worker that could not start must not leave others waiting forever.`<br>`for (size_t missing = workers.size(); missing < static_cast<size_t>(count); ++missing)`<br>`snapshots_ready.arrive_and_drop();` | For each thread that could not start, I drop one expected barrier participant. Without this, already-started workers could wait forever. This changes only the failure path; successful runs always require all original workers to arrive. |
| 35 | `for (auto &worker : workers) worker.join();` | I join every thread that did start, allowing its stack and buffers to be cleaned up before leaving this scope. |
| 36 | `throw;` | I rethrow the startup error to the outer handler. Even if some workers computed rows after the drops, no output is written from this failed run. |
| 38 | `for (auto &worker : workers) worker.join();` | On the normal path, I join all workers. The barrier only gates the start of filtering; these joins still gate output until filtering has actually finished. |
| 39 | `auto compute_end = Clock::now();` | I stop computation timing after every join, including the snapshot and barrier costs. |
| 40 | `write_output(argv[2]);` | I save the now-complete filtered image. |
| 41 | `print_times(program_start, compute_start, compute_end);` | I print computation and total milliseconds. |
| 42 | `} catch (const exception &error) {` | I catch ordinary exceptions from the main thread. This does not catch exceptions escaping worker lambdas; such failures terminate the process. |
| 43 | `fprintf(stderr, "error: %s\n", error.what());` | I print the error message to standard error. |
| 44 | `return 1;` | I return a nonzero status so the script does not treat the failed run as a measurement. |

### Input generator

Source: [scripts/generate_input.cpp](scripts/generate_input.cpp).

| Line(s) | Code | What I say to the professor |
|---|---|---|
| 1 | `#include <fstream>` | I include `<fstream>` for writing the generated input file. |
| 2 | `#include <iostream>` | I include `<iostream>` for diagnostic output to `std::cerr`. |
| 3 | `#include <random>` | I include `<random>` for the random engine and integer distributions. |
| 4 | `#include <string>` | I include `<string>` for converting the argument strings. |
| 6–7 | `// generate_input <rows> <cols> <output_file> <seed>`<br>`int main(int argc, char **argv) {` | This separate utility creates input before the programs are timed. It accepts rows, columns, output filename, and seed. |
| 8 | `if (argc != 5) {` | I require five arguments including the executable name. |
| 9 | `std::cerr << "usage: generate_input <rows> <cols> <output_file> <seed>\n";` | I print the expected generator command if the count is incorrect. |
| 10 | `return 1;` | I return a failure status. |
| 12 | `try {` | I handle ordinary conversion and file errors with a try/catch block. |
| 13 | `int rows = std::stoi(argv[1]), cols = std::stoi(argv[2]);` | I convert the requested row and column counts to integers. The benchmark script has already validated that its settings are positive integer strings. |
| 14 | `if (rows < 1 \|\| cols < 1) throw std::runtime_error("dimensions must be positive");` | I reject zero or negative dimensions. |
| 15 | `std::mt19937 random(std::stoul(argv[4]));` | I seed an `mt19937` pseudorandom engine from the seed argument. Reusing the seed with the same toolchain reproduces the generated values. |
| 16 | `std::uniform_int_distribution<int> pixel(0, 255), weight(-5, 5);` | I define inclusive ranges of 0 through 255 for pixels and −5 through 5 for weights. Nine products then have an absolute-sum bound of 11475, so generated convolution arithmetic fits in int. |
| 17 | `std::ofstream output(argv[3]);` | I open the requested output file. |
| 18 | `if (!output) throw std::runtime_error("cannot open output file");` | I reject failure to open it. |
| 19 | `output << rows << ' ' << cols << '\n';` | I write the image dimensions and a newline, matching `read_input`. |
| 20 | `for (int i = 0; i < rows; ++i) {` | I generate one image row at a time. |
| 21 | `for (int j = 0; j < cols; ++j) output << (j ? " " : "") << pixel(random);` | I draw and write every pixel immediately, inserting a space before all but the first. I do not allocate a random image in memory inside the generator. |
| 22 | `output << '\n';` | I end the generated image row. |
| 24 | `output << "3\n";` | I write the fixed kernel size, 3. |
| 25 | `for (int i = 0; i < 3; ++i) {` | I generate the three kernel rows. |
| 26 | `for (int j = 0; j < 3; ++j) output << (j ? " " : "") << weight(random);` | I draw and write each of the nine weights using the weight distribution. |
| 27 | `output << '\n';` | I end the kernel row. |
| 29 | `output.close();` | I explicitly close the output stream so pending writes and closing errors can be checked. |
| 30 | `if (!output) throw std::runtime_error("cannot write output file");` | I check the final stream state and report a write failure rather than silently accepting an incomplete input file. |
| 31 | `} catch (const std::exception &error) {` | I catch ordinary errors by const reference. |
| 32 | `std::cerr << "error: " << error.what() << '\n';` | I write the diagnostic message to standard error. |
| 33 | `return 1;` | I return failure. On the successful path, reaching the end of main returns zero. |

### Finish the code explanation

“The central correctness argument is that `compute_row` always receives original previous, current, and next rows. The range loop preserves that invariant by swapping saved buffers and copying the next untouched row. Each worker saves its cross-worker boundary rows, then all workers wait at the barrier; ownership then prevents overlapping writes, and joins finish the computation before output. We obtain the same 3×3 result as the independent reference, using two temporary row vectors sequentially and three per active worker in parallel.”

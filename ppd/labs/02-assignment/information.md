# Lab 2 — How to explain the assignment confidently

## A short opening explanation

“Lab 1 applied a convolution filter and stored the output in a second matrix. Lab 2 must overwrite the input matrix, so the challenge is preserving the original neighbours that later calculations still need. I use two rolling row vectors sequentially and three vectors per worker in parallel. Workers own contiguous row ranges, and I snapshot their boundaries before starting any threads.”

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

The main thread prepares each worker's vectors before starting anyone:

1. `previous`: original row `max(start-1,0)`.
2. `current`: original row `start`.
3. `following`: original row `min(end,N-1)`.

Only after **all** snapshots are complete are workers started. If snapshotting and starting were mixed in the same loop, an early worker could overwrite a row before a later worker copied it.

Inside a range, the next row is safe to read from the image because only the current worker owns it and processes rows forward. At the final row of a range, use `following`. At the global bottom edge, use saved `current` for replicated padding.

This also works when a worker owns just one row: all required source rows were copied before computation began.

## Why there is no barrier or lock

A barrier would be needed if workers captured their own boundaries after starting. Here, main does all snapshots before any worker runs, so a barrier would add no protection.

Workers write disjoint rows and never read another worker's live pixels. Kernel data and image structure remain unchanged during computation. The image container is not resized while workers run. The main thread joins all workers before writing the output.

In Java, `start()` makes prior initialization visible to the new thread, and `join()` makes completed worker writes visible to main. C++ thread startup and joining provide the corresponding synchronization. Separate row buffers are owned by separate workers; they are not shared scratch space.

No lock is required around each output pixel. Adding one would serialize work and would not, by itself, restore pixels that were already overwritten.

## Explain the memory claim precisely

Let N be rows and M columns. The image contains NM integers. The fixed kernel contains nine integers.

- Sequential scratch: two vectors of length M, or O(M).
- Parallel scratch: three vectors per active worker, or O(qM), where q=min(p,N).
- Thread metadata, runtime stacks, row metadata, and fixed I/O buffers also exist.
- Total image-plus-buffer storage remains O(NM), since q<=N.

For the required square cases and a fixed number of threads, auxiliary row storage is linear in image side length. If asked whether O(pM) is always O(M), answer **no**: that simplification only holds when p is treated as a constant.

At 10000×10000, image integers occupy 400 MB in decimal units. Sequential vectors occupy 80 KB. Sixteen workers' vectors occupy 1.92 MB. Lab 1 additionally needed a second 400 MB image. Real process memory exceeds these payload numbers, especially for the JVM.

Time remains O(NM) sequentially because there are nine multiplications per cell. Parallel work is distributed, but snapshot preparation is serial O(qM), and thread creation/joining adds overhead. Ideal parallel time is roughly O(qM + NM/q), ignoring rounding and runtime effects.

## Navigate the code during an explanation

| Code | What to point out |
|---|---|
| `read_input` / `readInput` | Loads the only image matrix and the 3×3 kernel |
| `RowBuffers` | Copies original rows; parallel version adds the following boundary |
| `compute_row` / `computeRow` | Applies the nine weights with clamped column indices |
| `compute_range` / `computeRange` | Selects safe source rows, writes results, rotates and refills buffers |
| C++ `par.cpp`, Java `Par.main` | Partitions rows, prepares every snapshot, starts workers, joins them |
| `write_output` / `writeOutput` | Writes the now-filtered original matrix |
| `verify.py` | Small independent reference and edge/race checks |
| `run_tests.sh` | Validates options, builds, runs independent checks, executes benchmarks, compares outputs and calculates means |
| `same_content` / `run_program` in `run_tests.sh` | `cmp -s` comparison, per-run `OK`/`WRONG`, failure counts and CSV timing rows |

Java `clone()` copies a row; assigning `current=image[start]` would only alias the original row and break the algorithm. In C++, copying a `vector<int>` copies its elements; worker arguments use `ref` so the already-prepared buffer object is passed by reference.

Buffered integer I/O is retained from Lab 1 because the largest text input has 100 million values. It uses fixed-size byte buffers rather than splitting the whole file into strings. This matters for memory, but it is separate from the convolution algorithm.

## Explain the experiments

The required cases are 10×10 with two threads, and 1000×1000 / 10000×10000 with 2, 4, 8, and 16 threads, plus sequential execution in both languages. Each configuration runs ten times on the same generated input. Because convolution overwrites its input, every run reloads the file rather than filtering an already-filtered image.

Computation timing includes row-buffer allocation/copying, thread creation, filtering, and joining. It excludes reading, writing, and correctness comparison. Total time also includes in-program I/O, but excludes process/JVM startup before main.

C++ uses `std::chrono::high_resolution_clock` through the shared `Clock` alias; Java uses `System.nanoTime()`. `high_resolution_clock` means the clock with the shortest tick period, not a guarantee that the algorithm runs faster. It is not guaranteed to be monotonic on every platform; its `is_steady` property depends on the standard library. On this machine, a compiled check confirmed it is the same type as `steady_clock` and `is_steady` is true. The name and portability behavior are specified in the [C++ standard draft, section time.clock.hires](https://isocpp.org/files/papers/n4296.pdf).

### Walk through the test script

Run `bash scripts/run_tests.sh` from the assignment directory for the full default suite. For a quick example, use `CASES="10 10 2; 7 13 2 3 16" bash scripts/run_tests.sh 2`. Quick runs also replace result files, so save the existing CSVs first if you need the full assignment measurements.

The shell script validates the settings, calls `build.sh`, and runs the independent Python `verify.py` checks. It then generates one input per case and saves an unmeasured C++ sequential output as `build/output_seq.txt`. Each measured execution reloads the input and writes `build/output.txt`.

The `run_program` function captures `<compute_ms> <total_ms>` from the program, then uses `same_content` to compare these two output files with `cmp -s`. Matching files produce `OK`; different or unreadable files produce `WRONG` and increment `failures`. Comparison takes place after the program's timers have finished and uses bounded memory even for large outputs.

Output mismatches do not stop the next test: they are counted and reported with a final nonzero exit. A program/build/check failure stops the script immediately. If all comparisons pass, `awk` calculates means and speedups from `times.csv` and writes `averages.csv`. On failure, previous averages may still exist, so their presence alone does not prove that the latest run passed. Correctness statuses are printed to the console and are not CSV columns.

Bash now handles execution and comparison; Python is only the small independent oracle. The stored 240-run dataset was collected before the benchmark loop moved into the shell script. The current runner was checked separately with 24 smoke executions and a deliberate comparison mismatch. The C++ clock rename leaves the recorded timing source unchanged on this machine because the two clock names alias the same type.

The runner compares **every** measured output to a fixed sequential reference using bounded-memory file reads. An independent small reference is also needed: agreement among implementations alone cannot rule out an identical bug in all of them. The test oracle may use a result matrix because it is separate test code operating on small examples, not the submitted production convolution.

The reported mean is the sum of ten times divided by ten. Speedup is `mean Tseq / mean Tpar` within the same language. Above 1 means faster, below 1 means slower. For example, 100 ms sequential and 40 ms parallel gives 2.5× speedup; with four workers, efficiency would be 2.5/4 = 62.5%.

For small matrices, expect thread overhead to dominate. For larger matrices, parallelism can help, but memory traffic, serial buffer preparation, core count, and scheduling limit scaling. More threads do not guarantee better performance. This machine has heterogeneous cores, so equal row counts do not guarantee equal completion times.

Java starts a fresh JVM per run with no warm-up; computation may include JIT compilation. C++ is compiled ahead of time with optimization. This benchmark compares these actual execution conditions; it does not isolate warmed-up steady-state kernel performance. Do not attribute a difference solely to a language or cache behavior without a separate experiment supporting that explanation.

Read the measured tables and analysis in [README.md](README.md#measured-results). Use `times.csv` to inspect variation and `averages.csv` for the reported means. Explain observed numbers first, then distinguish possible causes from directly established facts.

## Likely oral questions

| Question | Short answer |
|---|---|
| What changed from Lab 1? | The output overwrites the original image, so dependencies need saved original rows. |
| Why two sequential vectors? | Previous rows are overwritten, and the current row changes while computing its columns. |
| Why a third vector per worker? | The next worker may overwrite the following boundary row at any time. |
| Why snapshot first and start later? | Otherwise a running worker can corrupt another worker's future snapshot. |
| Could workers share one scratch row? | No; they need independently preserved data and update buffers concurrently. |
| Why row distribution? | It is required, gives contiguous work, and makes a 3×3 stencil manageable with row vectors. |
| Why no mutex? | No overlapping writes or live cross-worker reads remain after snapshotting. |
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
4. Explain snapshots-before-start, then why no barrier or mutex is needed.
5. Give O(M) / O(pM) auxiliary memory and the concrete memory savings.
6. Explain correctness checks, what is timed, and the observed performance tables.

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
| 142–143 | `// Save the neighbouring worker's original row BEFORE any worker starts.`<br>`if (parallel) following = image[min(end, rows - 1)];` | In parallel mode I also copy row `end`, clamped at the global bottom edge. This row belongs to the next worker, so I must preserve it before any thread can overwrite it. Sequential mode leaves `following` empty and uses only two vectors. |

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
| 1 | `#include <functional>` | I include `<functional>` for `std::ref`, which lets a thread receive an existing buffer object by reference. |
| 2 | `#include <thread>` | I include `<thread>` for explicitly created `std::thread` workers. |
| 3 | `#include "conv.hpp"` | I include the same convolution and I/O helpers as the sequential program. |
| 5 | `int main(int argc, char **argv) {` | The parallel entry point receives command-line argument strings. |
| 6 | `if (argc != 4) {` | I require executable name, input path, output path, and requested thread count. |
| 7 | `fprintf(stderr, "usage: %s <input_file> <output_file> <threads>\n", argv[0]);` | I print the command format if an argument is missing or extra. |
| 8 | `return 1;` | I return a failure status for invalid argument count. |
| 10 | `try {` | The outer try block reports ordinary program errors. |
| 11 | `auto program_start = Clock::now();` | I start total in-program timing. |
| 12 | `int requested_threads = parse_threads(argv[3]);` | I parse and validate the requested positive thread count. |
| 13 | `read_input(argv[1]);` | I load the original image and fixed-size kernel before any worker exists. |
| 14 | `auto compute_start = Clock::now();` | I start computation timing before buffer preparation and thread creation, so their overhead is included. |
| 15 | `int count = min(requested_threads, rows);` | I cap the active thread count at the number of rows. Every worker then owns at least one row. |
| 16 | `vector<RowBuffers> buffers;` | I create a container for the workers' buffer objects. These objects contain separate row vectors; there is still only one image-sized matrix. |
| 17 | `buffers.reserve(count);` | I reserve space for exactly this many buffer objects. Reserve changes capacity, not size, and avoids reallocating the outer container while preparing it. |
| 18–19 | `// Complete ALL snapshots before starting ANY worker: no barrier is needed.`<br>`for (int t = 0; t < count; ++t) {` | This first loop only prepares data. I deliberately start no workers yet, so every snapshot is taken from the original image. |
| 20 | `int start = static_cast<long long>(t) * rows / count;` | I calculate the inclusive first row with integer division. Casting before multiplication makes the intermediate arithmetic use `long long`. |
| 21 | `int end = static_cast<long long>(t + 1) * rows / count;` | I calculate the exclusive end row in the same way. Together these formulas give complete, nonoverlapping ranges differing by at most one row. |
| 22 | `buffers.emplace_back(start, end, true);` | I construct this worker's buffers directly in the container. The true flag also saves its following boundary row. |
| 24 | `vector<thread> workers;` | Only after all snapshots are complete do I create the container of thread objects. |
| 25 | `workers.reserve(count);` | I reserve room for all thread objects before starting them. |
| 26 | `try {` | This inner try block specifically handles failure while starting threads. Already-started threads must be joined before their objects are destroyed. |
| 27 | `for (int t = 0; t < count; ++t) {` | I now start one worker for each prepared range. |
| 28 | `int start = static_cast<long long>(t) * rows / count;` | I recompute the worker's inclusive start row with the same formula used during preparation. |
| 29 | `int end = static_cast<long long>(t + 1) * rows / count;` | I recompute its exclusive end row. |
| 30 | `workers.emplace_back(compute_range, start, end, ref(buffers[t]));` | `emplace_back` constructs and starts a thread that calls `compute_range(start,end,buffers[t])`. `ref` passes that worker's existing buffer object by reference instead of copying it. Each worker gets its own object, and the containers are not resized while workers use them. |
| 32 | `} catch (...) {` | If starting a later worker throws, this catch-all handler runs. |
| 33 | `for (auto &worker : workers) worker.join();` | I join each worker that was successfully started. This avoids destroying a joinable `std::thread`, which would terminate the program. |
| 34 | `throw;` | I rethrow the original startup error so the outer handler can report it. A partially completed computation is not written as a successful output. |
| 36 | `for (auto &worker : workers) worker.join();` | On the normal path I wait for every worker to finish. Joining also makes completed writes visible before main reads the image for output. No per-pixel locks or barriers are required because snapshots and ownership removed cross-worker read/write hazards. |
| 37 | `auto compute_end = Clock::now();` | I stop computation timing only after every join has completed. |
| 38 | `write_output(argv[2]);` | I write the complete filtered image after all workers have finished. |
| 39 | `print_times(program_start, compute_start, compute_end);` | I print the two timings. |
| 40 | `} catch (const exception &error) {` | The outer handler catches ordinary exceptions from input, allocation, thread startup, or output. It is not a mechanism for catching an exception thrown inside a worker; workers only execute the preallocated arithmetic loop on supported inputs. |
| 41 | `fprintf(stderr, "error: %s\n", error.what());` | I report the failure message to standard error. |
| 42 | `return 1;` | I return a nonzero process status so the script stops rather than treating a failed execution as a measurement. |

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

“The central correctness argument is that `compute_row` always receives original previous, current, and next rows. The range loop preserves that invariant by swapping saved buffers and copying the next untouched row. Parallel preparation saves the cross-worker boundary rows before any worker starts; ownership then prevents overlapping writes, and joins finish the computation before output. We obtain the same 3×3 result as the independent reference, using two temporary row vectors sequentially and three per active worker in parallel.”

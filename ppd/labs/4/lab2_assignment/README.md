# Lab 2 - In-place convolution in Java and C++

## Requirements and approach

| Requirement | Implementation |
|---|---|
| One image matrix, dynamically allocated | C++ `vector<vector<int>>`; Java `int[][]` |
| Sequential in-place filtering | `cpp/seq.cpp`, `java/Seq.java` |
| Explicit threads, row distribution | `cpp/par.cpp`, `java/Par.java` |
| Shared I/O and row-buffer algorithm | `cpp/conv.hpp`, `java/Conv.java` |
| Random input generated before execution | `scripts/generate_input.cpp` |
| Independent small-case correctness checks | `scripts/verify.py` |
| Ten runs, output checks, averages and speedup | `scripts/run_tests.sh`, `times.csv`, `averages.csv` |
| Personal study and oral explanation guide | [information.md](information.md) |

## Build and run

Requirements: C++11 compiler with threads, a JDK (tested with Java 17), Bash, Python 3, and the standard `awk` and `cmp` utilities. No third-party libraries are needed. Run these commands from this directory:

```bash
bash scripts/build.sh
build/generate_input 10 10 build/date.txt 42
build/seq build/date.txt build/output_seq.txt
build/par build/date.txt build/output.txt 2
cmp build/output_seq.txt build/output.txt
java -cp build Seq build/date.txt build/output_java_seq.txt
java -cp build Par build/date.txt build/output_java_par.txt 2
cmp build/output_seq.txt build/output_java_seq.txt
cmp build/output_seq.txt build/output_java_par.txt
```

The compiler defaults to `c++`; set `CXX` to choose another compiler. C++ is built with `-std=c++11 -O2 -Wall -Wextra -pedantic`, adding `-pthread` for parallel execution.

Each program accepts an input and output path. Parallel programs also take a positive thread count. They print two numbers: `<compute_ms> <total_ms>`. Output matrices go to the specified file, not standard output. `total_ms` covers work inside the entry point through writing the output; it does not include process/JVM startup or final process teardown.

For the full verification and benchmark suite:

```bash
bash scripts/run_tests.sh
```

For a quick run instead:

```bash
CASES="10 10 2; 7 13 2 3 16" bash scripts/run_tests.sh 2
```

`CASES` contains semicolon-separated `rows cols threads...` entries. The positional argument is the repetition count, defaulting to 10. Dimensions, requested thread counts, and repetition count must be positive integers. Once building and independent verification succeed, the script replaces `times.csv`; it replaces `averages.csv` only after all benchmark outputs match. Copy existing results before starting a new experiment, including a quick run. The independent verification can also be run alone after building:

```bash
python3 scripts/verify.py
```

The largest input has 100 million integers. Allow approximately 2 GB of free disk space for the input, sequential reference, and current output, plus at least 1 GB of available memory. The benchmark runs one process at a time and gives Java a 1 GiB maximum heap (`-Xmx1g`). Build products and generated matrices stay in ignored `build/`; the original Lab 1 is unchanged.

## File format and mathematical behavior

`date.txt` is whitespace-separated:

```text
N M
N rows of M image integers
3
3 rows of 3 kernel integers
```

For example:

```text
2 2
1 2
3 4
3
1 2 3
4 5 6
7 8 9
```

The corresponding output is:

```text
2 2
111 126
141 156
```

Output starts with `N M`, followed by the filtered rows with single spaces between values. All four implementations produce the same byte format.

For row `i`, column `j`, the result is:

```text
sum(kernel[a][b] * original[clamp(i+a-1, 0, N-1)][clamp(j+b-1, 0, M-1)])
a, b = 0, 1, 2
```

Indices outside the image repeat the nearest edge value, including at corners. The kernel is used as supplied, without flipping, matching Lab 1's formula (often called cross-correlation in mathematical terminology).

The generator takes `rows cols output_file seed`, uses `mt19937`, and generates pixels in `[0,255]` and weights in `[-5,5]`. It is repeatable with the same seed and toolchain; all programs consume the exact same generated file. Since `|result| <= 9 × 255 × 5 = 11475`, generated calculations fit in signed 32-bit integers. Custom inputs must also keep all products and partial sums in range; convolution overflow is outside this lab's supported input domain. Invalid dimensions, a kernel size other than 3, malformed/truncated integers, out-of-range input integers, and invalid thread counts cause a nonzero exit.

## Algorithm and correctness

### Sequential

Before computing row `i`, `previous` and `current` contain the original rows `max(i-1,0)` and `i`. The next row is still untouched in the image; at the bottom edge, the next row is the saved `current` row.

All three source rows are therefore original, even while output values overwrite `image[i]`. After computing a row, swap `previous` and `current`, then copy the next untouched image row into `current`. This preserves the invariant for the next iteration. Two vectors are allocated once and reused; no result row or result matrix is allocated.

### Parallel

Let `q = min(p,N)` be the active thread count. Worker `t` owns:

```text
start = floor(t*N/q)
end   = floor((t+1)*N/q)
rows [start,end)
```

These ranges cover every row exactly once, never overlap, and differ in size by at most one row. Before starting **any** worker, the main thread copies three vectors for **every** worker: the row preceding its range, its first row, and the row following its range (clamped at image edges).

Each worker then uses the sequential algorithm in its own range. The last row in the range reads the saved following boundary, rather than another worker's live image row. The worker's first-row dependency on its predecessor is also saved. Interior next-row reads are safe because no other worker writes those rows, and the owning worker has not reached them yet.

No locks or per-row barriers are needed: snapshots precede thread start, and no worker subsequently reads another worker's modified rows. Thread startup publishes the snapshots; joining all threads makes results available before output is written. C++ passes each worker its own buffers by reference; Java passes its own `RowBuffers` object. Exceptions during C++ thread creation join already-started threads before propagating the error.

### Complexity

Here `N` is the row count, `M` the column count, and `q=min(p,N)`.

| Quantity | Sequential | Parallel |
|---|---|---|
| Filtering work, kernel fixed at 3×3 | O(NM) | O(NM) total |
| Ideal computation time | O(NM) | O(qM + ceil(N/q)M), plus thread overhead |
| Auxiliary row storage | 2M integers | 3qM integers |
| Other storage | O(N) row metadata, O(1) I/O buffers | O(N) row metadata, O(q) thread metadata and runtime stacks, O(1) I/O buffers |
| Total algorithm-managed storage | O(NM) | O(NM + qM), also O(NM) because q <= N |

The serial preparation of parallel snapshots costs O(qM). Each row is copied and filtered a constant number of times. The image itself is necessary storage, excluded when quoting auxiliary row storage.

At 10000×10000, the image payload is 400,000,000 bytes (about 381.5 MiB). Sequential row vectors add 80,000 bytes; 16 workers' row vectors add 1,920,000 bytes (about 1.83 MiB). Object/row metadata, I/O buffers, stacks, and JVM/runtime memory are additional. Lab 1's second 400 MB image is eliminated.

The assignment says auxiliary vectors must fit O(n). With square images and a fixed thread count, these bounds are linear in the side length. If thread count is also variable, the precise parallel bound is O(pM), capped by O(NM). This implementation does not claim a linear bound independent of thread count.

## Verification and measurement method

The small independent Python oracle computes out of place solely in the test process. Production programs never allocate a result matrix. Tests cover a hand-calculated asymmetric kernel, identity and zero kernels, negative values and extreme identity values, corners, single rows/columns, rectangular images, uneven partitions, one-row worker ranges, p=1, and p>N. Repeated parallel runs exercise scheduling differences. Invalid inputs and thread counts must fail. Source inspection confirms only the input image and fixed kernel are matrices; all convolution scratch storage consists of separate row vectors.

For each benchmark size, the input is generated once with seed `N*31 + M*17 + 3`. A separate, unmeasured C++ sequential execution produces a fixed reference. Every measured execution, including sequential ones, reloads the original input and must match this reference byte for byte. The script uses `same_content` (`cmp -s`) to compare outputs byte for byte without loading whole files into memory, following Lab 1’s test script. Small independent oracle checks guard against a shared error in the sequential and parallel implementations.

### Runner flow and output comparison

`scripts/run_tests.sh` owns the complete benchmark flow, following Lab 1's shell-script pattern:

1. Validate the requested run count and cases, build the programs, and run `verify.py`.
2. Generate `build/date.txt` once per case and save an unmeasured C++ sequential result in `build/output_seq.txt`.
3. For each repetition, run C++ sequential/parallel and Java sequential/parallel programs on that same input. Each measured program writes `build/output.txt`.
4. `run_program` captures the two timing values and calls `same_content`, which executes `cmp -s` on the output and reference. Every measured execution, including sequential execution, reports `OK` or `WRONG`.
5. Count mismatches and exit unsuccessfully if any occurred. Otherwise, use `awk` to calculate the means and speedups and write `averages.csv`.

There is no separate Python benchmark runner. Python is used for the independent correctness checks; Bash runs the benchmarks, `cmp` checks outputs, and `awk` summarizes the CSV.

| File | Contents |
|---|---|
| `times.csv` | `language,variant,rows,cols,threads,run,compute_ms,total_ms` for each measured execution |
| `averages.csv` | `language,variant,rows,cols,threads,runs,avg_compute_ms,avg_total_ms,speedup` |

`threads` records the requested count; parallel programs cap the active count at the number of rows. The default assignment cases do not require this cap. The raw CSV contains timings, not correctness statuses; read the console's final `PASS` or `FAILED` message before treating a run as valid.

### Timing clocks and boundaries

Both C++ entry points use the shared alias `using Clock = chrono::high_resolution_clock;`. Java uses `System.nanoTime()`. Timing starts before buffer preparation and ends after computation/joins. Reading, writing, and external correctness comparisons are excluded from `compute_ms`. Each run starts a fresh process/JVM without warm-up, matching Lab 1's approach; Java computation measurements can therefore include JIT activity. Mean speedup is `mean sequential compute time / mean parallel compute time` for the same language and dimensions.

The standard C++ name is `std::chrono::high_resolution_clock`. It provides the clock with the shortest tick period, but is not guaranteed to be monotonic on every platform. A compiled check on the tested Apple Clang/libc++ environment confirmed that it aliases `steady_clock` and has `is_steady == true`. The saved measurements used this same underlying clock before the source alias was renamed, so the clock change does not invalidate them on this machine.

There are 24 configurations and 240 measured runs in the required suite, plus three unmeasured reference executions. Every run reports `OK` or `WRONG` and is recorded immediately in `times.csv`. Mismatches are counted and cause a nonzero exit after the runs; a process failure stops the script immediately. `averages.csv` is written only when all requested outputs match. Existing averages are retained on failure, so check the final success message before using them.

## Measured results

The full suite completed on **29 October 2026**: all **240 measured outputs** matched the sequential reference. The saved dataset predates moving the benchmark loop from Python into `run_tests.sh`; the current shell runner preserves the same inputs, execution order, timing boundaries, and comparison rule. It was subsequently checked with 24 measured smoke runs, independent checks, and an explicit mismatch-detection check in an isolated directory. The full 240-run dataset was retained rather than regenerated. All **212 independent correctness/invalid-input checks** passed. A separate C++ build with AddressSanitizer and UndefinedBehaviorSanitizer also passed the same 212-check fixture set (using the instrumented C++ programs for both language slots); no sanitizer errors were reported.

Environment: Apple M4 Pro, 14 physical/logical CPU cores reported by `sysctl`, 48 GiB RAM, macOS 27.2 (build 26B123), Apple Clang 21.0.0 targeting arm64, and Temurin OpenJDK 17.0.19. Release C++ binaries used the build flags above; Java benchmark runs used `-Xmx1g`. Measurements were taken sequentially on this machine, with no Java warm-up and no CPU-affinity or frequency controls. Documentation was written while the suite ran, so the machine was not reserved exclusively for benchmarking.

The tables show arithmetic means of ten runs in **milliseconds**, timing computation only. Full precision, individual executions, and total times are in [times.csv](times.csv) and [averages.csv](averages.csv).

### 10 × 10, kernel 3 × 3

| Execution | Threads | C++ compute (ms) | C++ speedup | Java compute (ms) | Java speedup |
|---|---:|---:|---:|---:|---:|
| Sequential | 1 | 0.000496 | 1.000× | 0.144800 | 1.000× |
| Parallel | 2 | 0.040650 | 0.012× | 0.309329 | 0.468× |

### 1000 × 1000, kernel 3 × 3

| Execution | Threads | C++ compute (ms) | C++ speedup | Java compute (ms) | Java speedup |
|---|---:|---:|---:|---:|---:|
| Sequential | 1 | 1.405067 | 1.000× | 8.888400 | 1.000× |
| Parallel | 2 | 0.811241 | 1.732× | 11.110242 | 0.800× |
| Parallel | 4 | 0.473554 | 2.967× | 12.052425 | 0.737× |
| Parallel | 8 | 0.357467 | 3.931× | 19.220179 | 0.462× |
| Parallel | 16 | 0.357200 | 3.934× | 43.805817 | 0.203× |

### 10000 × 10000, kernel 3 × 3

| Execution | Threads | C++ compute (ms) | C++ speedup | Java compute (ms) | Java speedup |
|---|---:|---:|---:|---:|---:|
| Sequential | 1 | 174.433463 | 1.000× | 238.006812 | 1.000× |
| Parallel | 2 | 88.277479 | 1.976× | 127.179137 | 1.871× |
| Parallel | 4 | 45.185325 | 3.860× | 71.141446 | 3.346× |
| Parallel | 8 | 23.885046 | 7.303× | 50.930679 | 4.673× |
| Parallel | 16 | 23.253442 | 7.501× | 73.563954 | 3.235× |

### Performance analysis

- **10×10:** sequential execution is fastest in both languages. C++ computation takes roughly 0.000496 ms sequentially versus 0.040650 ms with two workers; Java takes 0.144800 ms versus 0.309329 ms. The actual filtering is too small to offset thread setup and snapshots. Such tiny timings are sensitive to noise and runtime initialization.
- **1000×1000:** C++ improves from 1.405 ms sequentially to approximately 0.357 ms with either 8 or 16 workers (3.93×). Their means differ by less than 0.1%, far below the observed per-run variation, so there is no useful distinction between them here. Java sequential is fastest at 8.888 ms; parallel means range from 11.110 to 43.806 ms. Fresh-JVM initialization/JIT activity and worker overhead are plausible contributors, but these runs do not isolate their individual costs.
- **10000×10000:** C++ improves from 174.433 ms to 23.253 ms with 16 workers (7.50×), compared with 23.885 ms at 8 workers (7.30×). Sixteen workers are only about 2.6% faster by the measured mean, so extra workers have diminishing returns. Java improves from 238.007 ms to 50.931 ms with 8 workers (4.67×); 16 workers are slower at 73.564 ms (3.24×), with greater run-to-run variation. Core availability, heterogeneous core speeds, scheduling, and memory traffic can limit scaling; these results alone do not identify a single bottleneck.
- **C++ versus Java:** C++ has the lower computation mean for every measured configuration. On the largest image, its sequential computation is about 1.36× faster than Java's, and its best parallel computation is about 2.19× faster than Java's best. This comparison applies to these implementations, compiler/JVM settings, and fresh-process measurements; it is not a universal language ranking.
- **Whole-program impact:** at 10000×10000, C++ total in-program time drops from 1457.289 ms sequentially to 1306.829 ms at 16 workers (about 1.12×). Java drops from 2138.531 ms sequentially to 1946.646 ms at 8 workers (about 1.10×). Reading and writing remain sequential and dominate total time, so large computation speedups give modest end-to-end improvements. The reported totals exclude process startup.
- **Space:** all variants use one image, not two. Sequential auxiliary rows are O(M); parallel rows are O(qM). In-place filtering saves approximately 400 MB of image storage for the largest case compared with an equally represented out-of-place implementation. These are calculated storage costs, not measured process peak memory.

By the observed computation means, choose sequential for 10×10 in both languages; for 1000×1000 choose 8 C++ workers (effectively tied with 16) and Java sequential; for 10000×10000 choose 16 C++ workers or 8 Java workers. These choices summarize this machine's measurements rather than portable optimal thread counts.

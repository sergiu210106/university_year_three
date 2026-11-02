# Lab 2 remake - Variant 1b: Copy, barrier, update

## Requirements and approach

This is a separate copy of [the original Lab 2 implementation](../lab2_assignment/README.md), changed to implement **Variant 1b** from the [L2 Utils notes](utils.md). Each parallel worker copies its original neighbours into private vectors, waits at a shared barrier, then updates its assigned image rows in place. The sequential algorithm, file formats, explicit-thread approach, and output comparisons are retained. The original assignment directory is unchanged.

| Requirement | Implementation |
|---|---|
| One image matrix, dynamically allocated | C++ `vector<vector<int>>`; Java `int[][]` |
| Sequential in-place filtering | `cpp/seq.cpp`, `java/Seq.java` |
| Explicit threads, row distribution, Variant 1b | `cpp/par.cpp` (`std::barrier`), `java/Par.java` (`CyclicBarrier`) |
| Shared I/O and row-buffer algorithm | `cpp/conv.hpp`, `java/Conv.java` |
| Random input generated before execution | `scripts/generate_input.cpp` |
| Independent small-case correctness checks | `scripts/verify.py` |
| Ten runs, output checks, averages and speedup | `scripts/run_tests.sh`, `times.csv`, `averages.csv` |
| Personal study and oral explanation guide | [information.md](information.md) |

## Build and run

Requirements: C++20 compiler and standard library with `std::barrier` support, a JDK (tested with Java 17), Bash, Python 3, and the standard `awk` and `cmp` utilities. No third-party libraries are needed. Run these commands from this directory:

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

The compiler defaults to `c++`; set `CXX` to choose another compiler. C++ is built with `-std=c++20 -O2 -Wall -Wextra -pedantic`, adding `-pthread` for parallel execution.

For VS Code, open the `labs` folder to use its `.vscode/settings.json`: C++ IntelliSense is set to C++20 with `/usr/bin/clang++` and the Apple Silicon target. If you open only this assignment as the workspace, set its C/C++ IntelliSense standard to C++20 and select your C++20-capable compiler. An editor configured for C++17 reports `barrier` as undefined even though `build.sh` compiles correctly. Editor settings do not change a terminal build command; keep using `build.sh` or pass `-std=c++20` explicitly. [VS Code configuration reference](https://code.visualstudio.com/docs/cpp/customize-cpp-settings).

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

The largest input has 100 million integers. Allow approximately 2 GB of free disk space for the input, sequential reference, and current output, plus at least 1 GB of available memory. The benchmark runs one process at a time and gives Java a 1 GiB maximum heap (`-Xmx1g`). Build products and generated matrices stay in ignored `build/`; the original Lab 1 and `lab2_assignment` directories are unchanged.

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

These ranges cover every row exactly once, never overlap, and differ in size by at most one row. The main thread creates a shared barrier with `q` participants and starts the workers. Each worker copies three original rows into its own vectors: the row preceding its range, its first row, and the row following its range (clamped at image edges). It then waits at the barrier. No worker may modify the image until all `q` workers have completed their snapshots.

Each worker then uses the sequential algorithm in its own range. The last row in the range reads the saved following boundary, rather than another worker's live image row. The worker's first-row dependency on its predecessor is also saved. Interior next-row reads are safe because no other worker writes those rows, and the owning worker has not reached them yet.

The synchronization point is `snapshots_ready.arrive_and_wait()` in C++20 and `snapshotsReady.await()` on Java's `CyclicBarrier`. It is reached once per worker, before filtering. The last arrival releases the group; earlier workers cannot overwrite rows while a slower worker is still copying. After this point, snapshots and row ownership eliminate live cross-worker reads. No per-pixel locks or per-row barriers are needed. Main joins all workers before output.

If C++ thread creation fails partway through, main calls `arrive_and_drop()` for each participant that could not start, joins the started workers, and reports failure without writing output. An uncaught allocation failure inside a C++ worker is fatal to the process. Java catches worker/startup failures and exits the command-line program unsuccessfully, so remaining barrier participants cannot wait forever. Ordinary parsing and I/O errors also cause nonzero exits.

### Complexity

Here `N` is the row count, `M` the column count, and `q=min(p,N)`.

| Quantity | Sequential | Parallel |
|---|---|---|
| Filtering work, kernel fixed at 3×3 | O(NM) | O(NM) total |
| Ideal computation time | O(NM) | O(M + ceil(N/q)M), plus thread creation, barrier and join overhead |
| Auxiliary row storage | 2M integers | 3qM integers |
| Other storage | O(N) row metadata, O(1) I/O buffers | O(N) row metadata, O(q) thread metadata and runtime stacks, O(1) I/O buffers |
| Total algorithm-managed storage | O(NM) | O(NM + qM), also O(NM) because q <= N |

Snapshot work is O(qM) in total, distributed across workers rather than performed serially by main. Each worker copies three rows in O(M) and waits for the slowest participant. Thread creation is still performed by main, and allocation, scheduling, barrier waiting, and memory contention limit actual speedup. Each image row is copied and filtered a constant number of times. The image itself is necessary storage, excluded when quoting auxiliary row storage. The shared barrier and worker metadata add at most O(q) synchronization bookkeeping, without changing O(qM) auxiliary row storage.

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

Both C++ entry points use the shared alias `using Clock = chrono::high_resolution_clock;`. Java uses `System.nanoTime()`. Timing starts before thread/buffer preparation and ends after computation/joins, including the barrier wait. Reading, writing, and external correctness comparisons are excluded from `compute_ms`. Each run starts a fresh process/JVM without warm-up, matching Lab 1's approach; Java computation measurements can therefore include JIT activity. Mean speedup is `mean sequential compute time / mean parallel compute time` for the same language and dimensions.

The standard C++ name is `std::chrono::high_resolution_clock`. It provides the clock with the shortest tick period, but is not guaranteed to be monotonic on every platform. A compiled check on the tested Apple Clang/libc++ environment confirmed that it aliases `steady_clock` and has `is_steady == true`. The remake keeps this clock alias; its timings include the Variant 1b snapshot and barrier phases.

There are 24 configurations and 240 measured runs in the required suite, plus three unmeasured reference executions. Every run reports `OK` or `WRONG` and is recorded immediately in `times.csv`. Mismatches are counted and cause a nonzero exit after the runs; a process failure stops the script immediately. `averages.csv` is written only when all requested outputs match. Existing averages are retained on failure, so check the final success message before using them.

## Measured results

The Variant 1b suite completed on **30 October 2026**: all **240 measured outputs** across 24 configurations matched the sequential reference. Each configuration used ten fresh-process executions, and these CSVs replace the copied original measurements. All **212 correctness/invalid-input checks** passed. C++ AddressSanitizer/UndefinedBehaviorSanitizer builds also passed the same fixture set using instrumented C++ binaries in both command slots.

Additional failure-path checks confirmed that an interrupted Java barrier exits with status 1, and an injected C++ thread-start failure drops the missing participant, joins the started worker, exits with status 1, and preserves the output file. Verification commands use a 30-second timeout to expose a stuck barrier.

Environment: Apple M4 Pro, 14 physical/logical cores, 48 GiB RAM, macOS 27.2 (26B123), Apple Clang 21.0.0 targeting arm64, and Temurin OpenJDK 17.0.19. C++ release builds use `-std=c++20 -O2 -Wall -Wextra -pedantic` and `-pthread` for the parallel program. Java benchmark processes use `-Xmx1g`, without warm-up. Documentation and additional checks ran while benchmarks were in progress; the machine was not reserved exclusively for timing and no affinity/frequency controls were applied.

Mean **computation times in milliseconds**, including worker-side snapshots, barrier waiting, and joins, are shown below. Raw runs are in [times.csv](times.csv); means, total times, and speedups are in [averages.csv](averages.csv).

### 10 × 10, kernel 3 × 3

| Execution | Threads | C++ compute (ms) | C++ speedup | Java compute (ms) | Java speedup |
|---|---:|---:|---:|---:|---:|
| Sequential | 1 | 0.000575 | 1.000× | 0.154537 | 1.000× |
| Parallel | 2 | 0.047496 | 0.012× | 1.022246 | 0.151× |

### 1000 × 1000, kernel 3 × 3

| Execution | Threads | C++ compute (ms) | C++ speedup | Java compute (ms) | Java speedup |
|---|---:|---:|---:|---:|---:|
| Sequential | 1 | 1.455421 | 1.000× | 9.630175 | 1.000× |
| Parallel | 2 | 0.854146 | 1.704× | 11.626433 | 0.828× |
| Parallel | 4 | 0.503813 | 2.889× | 11.344079 | 0.849× |
| Parallel | 8 | 0.383667 | 3.793× | 21.012471 | 0.458× |
| Parallel | 16 | 0.404025 | 3.602× | 44.872371 | 0.215× |

### 10000 × 10000, kernel 3 × 3

| Execution | Threads | C++ compute (ms) | C++ speedup | Java compute (ms) | Java speedup |
|---|---:|---:|---:|---:|---:|
| Sequential | 1 | 174.612121 | 1.000× | 242.180388 | 1.000× |
| Parallel | 2 | 91.081408 | 1.917× | 127.552700 | 1.899× |
| Parallel | 4 | 49.055487 | 3.559× | 71.911766 | 3.368× |
| Parallel | 8 | 25.582796 | 6.825× | 50.887779 | 4.759× |
| Parallel | 16 | 23.375062 | 7.470× | 79.030500 | 3.064× |

### Performance and space analysis

- **10×10:** sequential is fastest in both languages: about 0.000575 ms in C++ and 0.154538 ms in Java, compared with 0.047496 ms and 1.022246 ms using two workers. Thread startup, snapshotting, barrier setup/waiting and fresh-runtime costs outweigh this very small filtering task. Tiny durations are particularly sensitive to noise.
- **1000×1000:** C++ improves from 1.455421 ms sequentially to 0.383667 ms with eight workers (3.79×). Sixteen workers are slightly slower at 0.404025 ms. Java sequential is fastest at 9.630175 ms; all parallel configurations are slower, ranging from 11.344079 ms with four workers to 44.872371 ms with sixteen. Cold JVM/JIT behavior and coordination overhead are plausible contributors; this suite does not measure their costs separately.
- **10000×10000:** C++ improves from 174.612121 ms to 23.375062 ms with sixteen workers (7.47×). Java improves from 242.180388 ms to 50.887779 ms with eight workers (4.76×), while sixteen workers take 79.030500 ms. More workers do not guarantee faster completion: scheduling, core availability, heterogeneous cores, memory traffic and barrier waiting can limit scaling.
- **Language comparison:** C++ has a lower computation mean for every measured configuration. On the largest image, its sequential computation is about 1.39× faster than Java's; its best parallel computation is about 2.18× faster than Java's best. This describes the chosen implementations and fresh-process settings, not a universal property of either language.
- **Total in-program time:** for the largest image, C++ changes from 1472.533392 ms sequentially to 1333.288375 ms with sixteen workers (about 1.10× overall speedup). Java changes from 2158.055542 ms to 1967.209254 ms with eight workers (about 1.10×). File I/O remains sequential, so filtering speedup has a much smaller effect on total time.
- **Memory:** the barrier does not change the two sequential vectors or three vectors per active worker. Auxiliary row storage remains O(M) sequentially and O(qM) in parallel. At 10000×10000, the sole image payload is 400 MB, with 80 KB of sequential vectors or 1.92 MB for sixteen workers' vectors. These are calculated payloads, not measured peak process memory.
- **Compared with the original approach:** main no longer serially copies every worker's boundaries; each worker does that work itself, followed by one global synchronization. This changes coordination and moves O(qM) total snapshot work into the workers. It does not imply an automatic performance improvement, since a barrier and concurrent allocation have costs. This suite is not a controlled, paired comparison with the original implementation, nor does it benchmark variants 1f, 2b or 2f.

By the measured computation means, use sequential execution for 10×10 in both languages; eight C++ workers and Java sequential for 1000×1000; sixteen C++ workers and eight Java workers for 10000×10000. These choices are specific to this run and machine.

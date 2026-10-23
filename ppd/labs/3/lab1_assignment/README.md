# Lab 1 — Applying a convolution matrix (sequential vs. parallel, C++ and Java)

## 1. Requirements analysis

### 1.1 Topic

Given a matrix **F** of size `n x m` and a convolution matrix **C** of size `k x k` (k odd),
compute the matrix **V** (`n x m`) obtained by applying the convolution with C on F:

```
v[i][j] = Σ  c[a][b] · f[i + a − r][j + b − r]      a, b = 0 … k−1,  r = k / 2
```

For `k = 3` this is exactly the 9-term formula of the task (`f[i−1][j−1]·c[0][0] + … +
f[i+1][j+1]·c[2][2]`). F is *virtually bordered* with its boundary elements:
`f[−1][j] = f[0][j]`, `f[n][j] = f[n−1][j]`, `f[i][−1] = f[i][0]`, `f[i][m] = f[i][m−1]` and
the four corners. For a border of width `r` (k = 5) the same rule is applied repeatedly, so an
index outside the matrix is replaced by the nearest valid index (it is *clamped*).

Full statement: [`task.md`](task.md).

### 1.2 Requirements

| # | Requirement | Where it is solved |
|---|---|---|
| 1 | Sequential program | `cpp/seq.cpp`, `java/Seq.java` |
| 2 | Parallel program with `p` threads, explicitly created (no executors) | `cpp/par.cpp` (`std::thread`), `java/Par.java` (`Worker extends Thread`) |
| 2.1 | Horizontal distribution (rows to threads) | variant `horizontal` |
| 2.2 | Vertical distribution (columns to threads) | variant `vertical` |
| 2.3 | *Extra:* block distribution (submatrices to threads) | variant `block` |
| 2.4 | *Extra:* delta distribution function, linear and cyclic | variants `linear`, `cyclic` |
| 3 | Input from `MatrixF.txt`, `ConvolutionM.txt`, generated beforehand with random numbers | `scripts/generate_input.cpp` |
| 4 | Output to `output.txt`, checked against the sequential output for every one of the 10 runs | `scripts/run_tests.sh` |
| 5 | Java and C++ (C++11 or newer), C++ with static (`int f[MAX][MAX]`) and dynamic (`new`) allocation | `cpp/conv.hpp` (`-DSTATIC_ALLOC`) |
| 6 | Time only the computation; 10 runs per test, average | all programs, `times.csv`, `averages.csv` |
| 7 | Performance tables and analysis | this file, sections 5–6 |

### 1.3 Constraints

* All 10 runs of a test case use **the same input files**. The files are generated once per test
  case, with a fixed seed.
* **Correctness:** for every execution, the output file must be **identical** to the
  output of the sequential program on the same input.
* **Performance:** only the computation is timed for the tables. Reading and writing are not
  included. Every test case is executed 10 times by the script (10 separate processes), and the
  average is used.
* Threads are created and joined explicitly. No thread pools or executors are used.

Assumptions made:

* The elements are `int` (the task uses `int f[MAX][MAX]`). F holds random "pixels" in
  `[0, 255]`, and C holds random weights in `[−5, 5]`. So `|v[i][j]| ≤ 25 · 255 · 5 = 31 875`, and
  there is no overflow.
* Static allocation needs a compile-time size: `MAX = 10000` (the largest n and m in the
  tests). `k ≤ KMAX = 15`.
* `p ≥ 1` can be anything. It may be larger than the number of rows, columns or elements, and
  then some threads get no work.

## 2. Design

### 2.1 Data structures

| Data | C++ static | C++ dynamic | Java |
|---|---|---|---|
| F, V | global `int f[MAX][MAX], v[MAX][MAX]` | `int**`: `new int*[n]`, each row `new int[m]` | `int[n][m]` |
| C | global `int c[KMAX][KMAX]` | `int**`, `k` rows of `new int[k]` | `int[k][k]` |
| threads | `vector<thread>` | `vector<thread>` | `Par.Worker[]` (`Worker extends Thread`) |

All three are indexed as `f[i][j]`, so the computation code is the same for static and
dynamic allocation. The two C++ variants are the same source compiled with or without
`-DSTATIC_ALLOC`. Each thread writes its results directly into V, so no extra buffers are
needed.

Difference in memory layout: the static `f[MAX][MAX]` is one contiguous block with a row
stride of `MAX` ints, whatever m is. The dynamic matrix has `n` separately allocated rows of
exactly `m` ints, and access goes through an extra pointer (`f[i]`) per row.

### 2.2 Work distribution

Thread `t` (`t = 0 … p−1`) computes the elements of V below. All ranges are half-open and use
integer division, as in lab 0. So consecutive ranges follow each other with no gaps or overlaps,
cover everything, and differ in size by at most one unit.

| Variant | Thread `t` computes |
|---|---|
| `horizontal` | rows `[t·n/p, (t+1)·n/p)`, all columns |
| `vertical` | columns `[t·m/p, (t+1)·m/p)`, all rows |
| `block` | block `(t / pc, t mod pc)` of a `pr x pc` grid (`pr·pc = p`): rows `[br·n/pr, (br+1)·n/pr)` x columns `[bc·m/pc, (bc+1)·m/pc)` |
| `linear` | the elements with `δ_lin(i, j) = t` |
| `cyclic` | the elements with `δ_cyc(i, j) = t` |

**Block grid.** Among the factorisations `p = pr · pc`, the one whose blocks are closest to
square is chosen (minimum `|n/pr − m/pc|`). On a tie, the one with more block rows wins.

| matrix | p = 2 | p = 4 | p = 8 | p = 16 |
|---|---|---|---|---|
| 10 x 10 | — | 2 x 2 | — | — |
| 1000 x 1000 | 2 x 1 | 2 x 2 | 4 x 2 | 4 x 4 |
| 10 x 10000 | 1 x 2 | 1 x 4 | 1 x 8 | 1 x 16 |
| 10000 x 10 | 2 x 1 | 4 x 1 | 8 x 1 | 16 x 1 |

So in the tests `block` is identical to `horizontal` for 10000x10 and for 1000x1000 with p = 2,
and identical to `vertical` for 10x10000.

**Delta distribution functions.** The elements are numbered row by row, `idx = i·m + j`, with
`N = n·m`, and `δ : {0…n−1} x {0…m−1} → {0…p−1}`:

```
linear:  δ_lin(i, j) = the t with  t·N/p ≤ i·m + j < (t+1)·N/p      (contiguous ranges of indices)
cyclic:  δ_cyc(i, j) = (i·m + j) mod p                              (step p)
```

`δ_lin` is well defined: the ranges `[t·N/p, (t+1)·N/p)` for `t = 0 … p−1` split `[0, N)` with
no gaps or overlaps (same argument as for horizontal). `δ_cyc` is a function by definition.
So with both, every element is computed by exactly one thread.

A thread does not scan all N elements and test `δ(i, j) = t`. That would make every thread
do O(N) work, so p threads would together do p times more work than the sequential program.
Instead, thread t enumerates its own indices directly (`compute_indices(from, to, step)`):

* linear: `from = t·N/p`, `to = (t+1)·N/p`, `step = 1`
* cyclic: `from = t`, `to = N`, `step = p`

It moves `(i, j)` forward by `step` with a carry into the next row, so there is no division per
element. The functions `delta_linear` / `delta_cyclic` (`deltaLinear` / `deltaCyclic` in Java)
are in the code. In a check build (`-DCHECK_DELTA` in C++, `-Dconv.checkDelta=true` in Java),
every computed element is verified to satisfy `δ(i, j) = t` (section 4.2).

**No synchronization is needed during the computation.** F and C are only read, and every
`v[i][j]` is written by exactly one thread. The main thread waits for the workers with
`join()`, which also makes their writes visible to it, in both languages.

Note on `cyclic`: neighbouring elements of V belong to different threads, so the threads
constantly write the same cache lines (**false sharing**). Each thread also uses only every
p-th element of every cache line of F it reads. This variant is included because the task
asks for it, and its cost is measured in section 5.

### 2.3 Components and their specification

**`generate_input <n> <m> <k> <matrix_f_file> <convolution_file> <seed>`** (`scripts/generate_input.cpp`)
* *in:* sizes `n, m ≥ 1`, odd `k`, the two output file names, a seed
* *out:* `matrix_f_file`: `n m` on the first line, then n lines of m integers in `[0, 255]`.
  `convolution_file`: `k` on the first line, then k lines of k integers in `[−5, 5]`.
  Uses `mt19937`, so the same seed always produces the same files.

**`seq_static | seq_dynamic <F_file> <C_file> <output_file>`** (`cpp/seq.cpp`), **`java Seq …`** (`java/Seq.java`)
* *pre:* the input files have the format above. For `seq_static`, `n, m ≤ 10000`.
* *post:* the output file contains `n m` on the first line, then n lines with the m values of
  `V = F * C` separated by single spaces. Prints `<compute_ms> <total_ms>` (section 3.2) to
  stdout.

**`par_static | par_dynamic <F_file> <C_file> <output_file> <p> <variant>`** (`cpp/par.cpp`), **`java Par …`** (`java/Par.java`)
* *pre:* same as sequential. `p ≥ 1`, `variant ∈ {horizontal, vertical, block, linear, cyclic}`.
* *post:* same output file as the sequential program, computed by `p` threads with the given
  distribution (2.2).
* C++: thread `t` runs `thread_work(variant, t, p)`.
* Java: class **`Par.Worker extends Thread`**
  * `Worker(Variant variant, int t, int p)`: stores the variant, its index and the thread
    count.
  * `run()`: `Conv.threadWork(variant, t, p)`. It writes exactly the elements of V that 2.2
    assigns to thread `t`, and no others.

Shared code (`cpp/conv.hpp`, `java/Conv.java`), with the same functions in both languages:

| Function | Specification |
|---|---|
| `read_input(f_path, c_path)` | reads n, m, F, k, C. For dynamic allocation / Java, also allocates F, V, C |
| `write_output(path)` | writes `n m` and V in the output format |
| `compute_cell(i, j)` | returns `v[i][j]` (formula in 1.1, with clamped indices) |
| `compute_rect(r0, r1, c0, c1)` | `v[i][j] = compute_cell(i, j)` for `i ∈ [r0, r1)`, `j ∈ [c0, c1)` |
| `compute_indices(from, to, step, …)` | the same for the indices `from, from + step, … < to` (row by row) |
| `block_grid(p)` | `(pr, pc)` with `pr · pc = p`, closest to square blocks |
| `delta_linear(i, j, p)`, `delta_cyclic(i, j, p)` | owner thread of element `(i, j)` |
| `thread_work(variant, t, p)` | computes the elements of thread `t` (table in 2.2) |

The sequential programs call `compute_rect(0, n, 0, m)`.

**`scripts/run_tests.sh [runs]`**
* `same_content <file1> <file2>`: true if and only if the two files are identical byte by byte
  (`cmp`).
* `run_program <lang> <alloc> <variant> <threads> <command…>`: runs one program and appends
  its times to `times.csv`. Every program except the reference (C++ static sequential) must
  produce an output identical to the reference, otherwise it is reported as `WRONG`.
* At the end, computes `averages.csv` (averages and speedups) and exits with code 1 if any
  output was different.

### 2.4 Relations between components

```
                     fixed seed per test case
generate_input ───────────────────────────────► MatrixF.txt, ConvolutionM.txt
                                                        │  (same files for all 10 runs)
           ┌──────────────────────────┬─────────────────┴────────────┬─────────────────────────┐
           ▼                          ▼                              ▼                         ▼
     seq_static              seq_dynamic, java Seq        par_static, par_dynamic,       stdout of every
           │                          │                  java Par (5 variants × p)        program
           ▼                          ▼                              ▼                         ▼
   output_seq.txt ◄──── same_content ────► output.txt ◄──────────────┘               times.csv ──► averages.csv
    (reference)          OK / WRONG                                                  (all runs)    (avg + speedup)
```

Because the values are integers, C++ and Java print exactly the same text. So unlike lab 0,
**every** output (both languages, both allocations, all variants) is compared against one
reference.

## 3. Implementation details

### 3.1 Project structure

```
lab1_assignment/
├── task.md                           the assignment
├── cpp/conv.hpp                      shared: matrices, fast I/O, kernel, distributions
├── cpp/seq.cpp, cpp/par.cpp          C++ sequential / parallel (std::thread)
├── java/Conv.java                    shared: the same as conv.hpp
├── java/Seq.java, java/Par.java      Java sequential / parallel (java.lang.Thread)
├── scripts/generate_input.cpp        random input generator
├── scripts/run_tests.sh              build + correctness + performance testing
├── times.csv, averages.csv           results (generated)
└── build/                            binaries, .class files, input/output files (generated)
```

Tools: g++ 15.2 with `-O2` (`-pthread` for `par`), OpenJDK 21. The test script compiles
everything itself:
`seq_static`/`par_static` with `-DSTATIC_ALLOC`, `seq_dynamic`/`par_dynamic` without it.

### 3.2 Time measurement

As in lab 0, each program prints two times in milliseconds: `<compute_ms> <total_ms>`.

* **compute** (the value used in the tables, as the task requires): only the convolution.
  In the parallel programs it starts before the first thread is created and ends after the
  last `join()`, so creating and joining the threads is included: it is part of the cost of
  doing the work in parallel. Reading and writing the files are **not** included.
* **total**: the whole `main`, including reading and writing the files. Kept for reference.

Clocks: `std::chrono::steady_clock` (C++) and `System.nanoTime()` (Java), both monotonic.
Every execution does the work once. The 10 repetitions are 10 separate processes.

### 3.3 Fast I/O

The 10000 x 10000 input has 10⁸ numbers (357 MB of text), and the output is larger. `ifstream >>`
(C++), or `split` / `Scanner` (Java, which would also need the whole text in memory as strings),
would make every execution take tens of seconds. Both languages therefore use a small buffered
reader that parses integers by hand from 64 KiB blocks, and a buffered writer that formats
integers by hand. This does not change the timed computation. It only makes the 10 x 63
executions per test case feasible.

### 3.4 Other details

* **Border:** `compute_cell` clamps every row and column index into the matrix. That is the
  same as the virtual border of the task, for any `k`. For each of the `k` rows of the window,
  the row pointer `f[clamp(...)]` is computed once.
* **Java worker as a class, not a lambda:** the first lambda in a JVM costs ~15 ms of
  `invokedynamic` setup inside the timed region (found in lab 0).
* **Java `Enum.valueOf`:** the variant name is parsed with a loop over `Variant.values()`.
  `Enum.valueOf` uses reflection, and the first call added ~10 ms to the program's total time.
  That is outside the timed computation, but it distorted the total times.
* Times are printed with `.` as the decimal separator (`Locale.ROOT` in Java, `LC_ALL=C` in the
  script), as in lab 0.

## 4. Testing

### 4.1 Test matrix

| Test case | n | m | k | p |
|---|---:|---:|---:|---|
| 1 | 10 | 10 | 3 | 4 |
| 2 | 1000 | 1000 | 3 | 2, 4, 8, 16 |
| 3 | 10 | 10000 | 5 | 2, 4, 8, 16 |
| 4 | 10000 | 10 | 5 | 2, 4, 8, 16 |
| 5 | 10000 | 10000 | 5 | 2, 4, 8, 16 |

For every test case and every run, the script executes:
* C++ static: seq + 5 variants × |p|
* C++ dynamic: seq + 5 variants × |p|
* Java: seq + 5 variants × |p|

That is 63 executions per run (18 for test case 1), times 10 runs. In total
`10 · (18 + 4 · 63) = 2700` executions, and 2650 of them (all but the 50 reference runs) are compared against the reference.

### 4.2 Correctness verification

1. **Against the reference, for every execution:** in every run, `seq_static` writes
   `output_seq.txt`. Every other execution writes `output.txt`, which must be identical to it
   (`same_content`), otherwise the run is reported as `WRONG` and the script fails.
2. **Independent check of the reference:** a short Python script computed the convolution
   directly from the definition (with `min`/`max` for the border) for a 5 x 6 matrix. It was
   identical to the C++ output. By hand, for the corner element `(0, 0)` of that example
   (F rows `106 255 …` / `77 255 …`, C = `[[2,5,−1],[0,1,−1],[−4,5,−3]]`):
   `(2·106 + 5·106 − 255) + (0·106 + 1·106 − 255) + (−4·77 + 5·77 − 3·255) = 487 − 149 − 688 = −350`,
   which matches the program's `v[0][0]`.
3. **Edge cases**, with `p ∈ {1, 3, 4, 7, 16, 64}` on 5 x 6 and `CASES="10 10 3 4; 7 13 3 3 16"`
   in the script. These cover n and m not divisible by p, and p larger than n, m and even
   n·m, where some threads get nothing.
4. **Delta functions:** the check builds (2.2) on 5 x 6 and 1000 x 997 with p ∈ {2, 3, 7, 16}.
   Every element computed by thread t had `δ(i, j) = t`. Together with check 1 (every element
   computed correctly), this shows that each element is computed exactly by its owner.
5. **The check can fail:** on a copy of the project with a deliberate bug (in `horizontal`,
   each thread skips the last row of its range), the script reported
   `FAILED: 2 result(s) different from the sequential result` and exited with code 1.

### 4.3 How to run

All commands are run from the `lab1_assignment/` directory.

```bash
scripts/run_tests.sh                       # full test: all 5 cases, 10 runs (≈ 1 h 45 min)
scripts/run_tests.sh 3                     # choose the number of runs
CASES="10 10 3 4; 7 13 3 3 16" scripts/run_tests.sh 2          # custom cases: "n m k threads..."
VARIANTS="horizontal cyclic" CASES="1000 1000 3 4" scripts/run_tests.sh 1
```

Build and run by hand:

```bash
mkdir -p build
g++ -O2 -o build/generate_input scripts/generate_input.cpp
g++ -O2 -DSTATIC_ALLOC -o build/seq_static cpp/seq.cpp
g++ -O2 -o build/seq_dynamic cpp/seq.cpp
g++ -O2 -pthread -DSTATIC_ALLOC -o build/par_static cpp/par.cpp
g++ -O2 -pthread -o build/par_dynamic cpp/par.cpp
javac -d build java/*.java

build/generate_input 1000 1000 3 build/MatrixF.txt build/ConvolutionM.txt 42
build/seq_static  build/MatrixF.txt build/ConvolutionM.txt build/output_seq.txt
build/par_dynamic build/MatrixF.txt build/ConvolutionM.txt build/output.txt 4 block
java -cp build Par build/MatrixF.txt build/ConvolutionM.txt build/output.txt 8 cyclic
cmp build/output.txt build/output_seq.txt && echo IDENTICAL || echo DIFFERENT

# delta function check builds
g++ -O2 -pthread -DCHECK_DELTA -o build/par_check cpp/par.cpp
java -Dconv.checkDelta=true -cp build Par build/MatrixF.txt build/ConvolutionM.txt build/output.txt 7 linear
```

Inspect results: `column -s, -t < averages.csv`.

## 5. Results

### 5.1 Environment

Intel Core i7-1065G7: 4 cores / 8 hardware threads, 1.3 GHz base, 2 MiB L2 in total, 8 MiB L3.
15 GiB RAM, Linux 7.0, transparent huge pages in `madvise` mode (not used by these programs).
Compilers: g++ 15.2 `-O2`, OpenJDK 21.

Every value below is the **average of 10 runs**. All 2650 compared outputs were identical to
the reference. The full run took about 1 h 45 min of active time. Full data: `times.csv`
(every execution) and `averages.csv` (averages and speedups).

The laptop was suspended twice while test case 5 was running, and the run then continued.
Both clocks used are monotonic and stop during suspend, so the time spent suspended is not
counted. No execution of test case 5 is above 1.5x the median of its 10 runs. The only such
outliers in the whole run are a few millisecond-scale ones in the small test cases.

### 5.2 Results

Average **computation time** in ms (reading and writing not included), and
**speedup = T_seq / T_par**. The reference sequential time is the one of the same language
and the same allocation. A speedup above 1 means the parallel version is faster.

#### Test case 1: 10 x 10, k = 3

| Variant | p | C++ static time | C++ static speedup | C++ dynamic time | C++ dynamic speedup | Java time | Java speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| sequential | 1 | 0.026 | 1 | 0.004 | 1 | 0.202 | 1 |
| horizontal | 4 | 0.326 | 0.081 | 0.339 | 0.011 | 2.04 | 0.099 |
| vertical | 4 | 0.402 | 0.066 | 0.318 | 0.012 | 2.03 | 0.099 |
| block | 4 | 0.331 | 0.080 | 0.306 | 0.012 | 2.00 | 0.10 |
| linear | 4 | 0.315 | 0.084 | 0.307 | 0.012 | 1.98 | 0.10 |
| cyclic | 4 | 0.330 | 0.080 | 0.297 | 0.013 | 1.88 | 0.11 |

#### Test case 2: 1000 x 1000, k = 3

| Variant | p | C++ static time | C++ static speedup | C++ dynamic time | C++ dynamic speedup | Java time | Java speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| sequential | 1 | 34.14 | 1 | 26.83 | 1 | 52.10 | 1 |
| horizontal | 2 | 17.67 | 1.93 | 13.88 | 1.93 | 57.24 | 0.91 |
| horizontal | 4 | 9.74 | 3.50 | 7.20 | 3.72 | 69.19 | 0.75 |
| horizontal | 8 | 9.34 | 3.65 | 7.08 | 3.79 | 79.80 | 0.65 |
| horizontal | 16 | 10.34 | 3.30 | 8.63 | 3.11 | 86.12 | 0.60 |
| vertical | 2 | 18.63 | 1.83 | 13.95 | 1.92 | 58.98 | 0.88 |
| vertical | 4 | 10.69 | 3.19 | 7.31 | 3.67 | 66.45 | 0.78 |
| vertical | 8 | 11.23 | 3.04 | 7.55 | 3.55 | 81.05 | 0.64 |
| vertical | 16 | 11.82 | 2.89 | 9.51 | 2.82 | 119.8 | 0.43 |
| block | 2 | 17.18 | 1.99 | 13.89 | 1.93 | 57.96 | 0.90 |
| block | 4 | 9.31 | 3.67 | 7.71 | 3.48 | 72.30 | 0.72 |
| block | 8 | 9.24 | 3.70 | 7.41 | 3.62 | 83.50 | 0.62 |
| block | 16 | 11.00 | 3.10 | 8.64 | 3.10 | 84.30 | 0.62 |
| linear | 2 | 18.18 | 1.88 | 15.43 | 1.74 | 67.31 | 0.77 |
| linear | 4 | 9.56 | 3.57 | 8.28 | 3.24 | 78.31 | 0.67 |
| linear | 8 | 8.72 | 3.91 | 7.61 | 3.52 | 101.3 | 0.51 |
| linear | 16 | 10.94 | 3.12 | 9.68 | 2.77 | 102.3 | 0.51 |
| cyclic | 2 | 20.97 | 1.63 | 15.28 | 1.76 | 63.99 | 0.81 |
| cyclic | 4 | 13.45 | 2.54 | 7.68 | 3.49 | 79.42 | 0.66 |
| cyclic | 8 | 14.49 | 2.36 | 7.81 | 3.44 | 104.3 | 0.50 |
| cyclic | 16 | 13.99 | 2.44 | 9.54 | 2.81 | 113.4 | 0.46 |

#### Test case 3: 10 x 10000, k = 5

| Variant | p | C++ static time | C++ static speedup | C++ dynamic time | C++ dynamic speedup | Java time | Java speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| sequential | 1 | 7.56 | 1 | 6.96 | 1 | 29.94 | 1 |
| horizontal | 2 | 4.00 | 1.89 | 3.75 | 1.86 | 41.23 | 0.73 |
| horizontal | 4 | 2.68 | 2.82 | 2.43 | 2.86 | 46.75 | 0.64 |
| horizontal | 8 | 3.23 | 2.34 | 3.02 | 2.31 | 36.03 | 0.83 |
| horizontal | 16 | 3.32 | 2.27 | 2.94 | 2.36 | 42.17 | 0.71 |
| vertical | 2 | 3.98 | 1.90 | 3.74 | 1.86 | 41.33 | 0.72 |
| vertical | 4 | 2.26 | 3.34 | 2.14 | 3.25 | 47.81 | 0.63 |
| vertical | 8 | 2.22 | 3.41 | 2.08 | 3.34 | 67.88 | 0.44 |
| vertical | 16 | 2.77 | 2.73 | 2.56 | 2.72 | 90.57 | 0.33 |
| block | 2 | 3.95 | 1.91 | 3.83 | 1.82 | 41.38 | 0.72 |
| block | 4 | 2.19 | 3.45 | 2.10 | 3.32 | 48.04 | 0.62 |
| block | 8 | 2.22 | 3.40 | 2.10 | 3.32 | 64.92 | 0.46 |
| block | 16 | 2.65 | 2.85 | 2.66 | 2.61 | 86.39 | 0.35 |
| linear | 2 | 4.25 | 1.78 | 3.95 | 1.76 | 42.15 | 0.71 |
| linear | 4 | 2.29 | 3.31 | 2.19 | 3.17 | 49.94 | 0.60 |
| linear | 8 | 2.46 | 3.08 | 2.18 | 3.20 | 36.92 | 0.81 |
| linear | 16 | 2.68 | 2.82 | 2.71 | 2.56 | 88.83 | 0.34 |
| cyclic | 2 | 4.37 | 1.73 | 4.13 | 1.69 | 41.95 | 0.71 |
| cyclic | 4 | 2.53 | 2.99 | 2.52 | 2.76 | 48.88 | 0.61 |
| cyclic | 8 | 2.74 | 2.75 | 2.54 | 2.74 | 68.77 | 0.43 |
| cyclic | 16 | 3.28 | 2.30 | 3.19 | 2.18 | 87.17 | 0.34 |

#### Test case 4: 10000 x 10, k = 5

| Variant | p | C++ static time | C++ static speedup | C++ dynamic time | C++ dynamic speedup | Java time | Java speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| sequential | 1 | 35.03 | 1 | 7.15 | 1 | 20.40 | 1 |
| horizontal | 2 | 18.93 | 1.85 | 3.79 | 1.89 | 25.41 | 0.80 |
| horizontal | 4 | 10.03 | 3.49 | 2.09 | 3.42 | 26.85 | 0.76 |
| horizontal | 8 | 8.19 | 4.28 | 2.08 | 3.44 | 36.69 | 0.56 |
| horizontal | 16 | 9.54 | 3.67 | 2.71 | 2.64 | 42.05 | 0.48 |
| vertical | 2 | 32.74 | 1.07 | 3.67 | 1.95 | 24.85 | 0.82 |
| vertical | 4 | 31.09 | 1.13 | 2.32 | 3.08 | 28.33 | 0.72 |
| vertical | 8 | 45.33 | 0.77 | 3.00 | 2.38 | 38.40 | 0.53 |
| vertical | 16 | 45.41 | 0.77 | 3.15 | 2.27 | 54.70 | 0.37 |
| block | 2 | 19.49 | 1.80 | 3.69 | 1.94 | 25.49 | 0.80 |
| block | 4 | 10.14 | 3.46 | 2.12 | 3.38 | 27.68 | 0.74 |
| block | 8 | 7.76 | 4.51 | 2.08 | 3.44 | 35.55 | 0.57 |
| block | 16 | 9.72 | 3.60 | 2.52 | 2.83 | 45.46 | 0.45 |
| linear | 2 | 19.82 | 1.77 | 3.81 | 1.88 | 27.65 | 0.74 |
| linear | 4 | 9.98 | 3.51 | 2.12 | 3.38 | 28.45 | 0.72 |
| linear | 8 | 7.67 | 4.57 | 2.14 | 3.34 | 37.38 | 0.55 |
| linear | 16 | 9.56 | 3.66 | 2.81 | 2.54 | 45.02 | 0.45 |
| cyclic | 2 | 33.15 | 1.06 | 3.80 | 1.88 | 28.09 | 0.73 |
| cyclic | 4 | 31.37 | 1.12 | 2.20 | 3.25 | 29.16 | 0.70 |
| cyclic | 8 | 45.08 | 0.78 | 2.08 | 3.43 | 37.22 | 0.55 |
| cyclic | 16 | 30.48 | 1.15 | 2.66 | 2.69 | 48.18 | 0.42 |

#### Test case 5: 10000 x 10000, k = 5

| Variant | p | C++ static time | C++ static speedup | C++ dynamic time | C++ dynamic speedup | Java time | Java speedup |
|---|---:|---:|---:|---:|---:|---:|---:|
| sequential | 1 | 6797.3 | 1 | 6401.6 | 1 | 7697.3 | 1 |
| horizontal | 2 | 3547.0 | 1.92 | 3347.0 | 1.91 | 4109.0 | 1.87 |
| horizontal | 4 | 1782.1 | 3.81 | 1681.7 | 3.81 | 2102.0 | 3.66 |
| horizontal | 8 | 1672.0 | 4.07 | 1591.9 | 4.02 | 2105.4 | 3.66 |
| horizontal | 16 | 1712.4 | 3.97 | 1613.7 | 3.97 | 2214.8 | 3.48 |
| vertical | 2 | 3529.9 | 1.93 | 3345.7 | 1.91 | 4053.0 | 1.90 |
| vertical | 4 | 1782.2 | 3.81 | 1677.7 | 3.82 | 2098.5 | 3.67 |
| vertical | 8 | 1666.3 | 4.08 | 1570.7 | 4.08 | 2106.2 | 3.65 |
| vertical | 16 | 1710.5 | 3.97 | 1616.2 | 3.96 | 2174.2 | 3.54 |
| block | 2 | 3537.2 | 1.92 | 3352.0 | 1.91 | 4058.1 | 1.90 |
| block | 4 | 1771.3 | 3.84 | 1683.6 | 3.80 | 2191.2 | 3.51 |
| block | 8 | 1689.0 | 4.02 | 1570.7 | 4.08 | 2107.6 | 3.65 |
| block | 16 | 1711.0 | 3.97 | 1607.6 | 3.98 | 2177.7 | 3.54 |
| linear | 2 | 3699.7 | 1.84 | 3481.4 | 1.84 | 4069.9 | 1.89 |
| linear | 4 | 1861.3 | 3.65 | 1746.1 | 3.67 | 2108.5 | 3.65 |
| linear | 8 | 1730.5 | 3.93 | 1658.7 | 3.86 | 2114.4 | 3.64 |
| linear | 16 | 1770.5 | 3.84 | 1703.6 | 3.76 | 2177.5 | 3.54 |
| cyclic | 2 | 3811.5 | 1.78 | 3649.6 | 1.75 | 4079.2 | 1.89 |
| cyclic | 4 | 2054.9 | 3.31 | 1992.4 | 3.21 | 2128.9 | 3.62 |
| cyclic | 8 | 1965.6 | 3.46 | 1884.9 | 3.40 | 2176.0 | 3.54 |
| cyclic | 16 | 1815.5 | 3.74 | 1779.9 | 3.60 | 2329.6 | 3.30 |

Example of the task's table layout, filled in for test case 5 with the best variant (vertical):

| Matrix size | Allocation type | No threads | Execution time (ms) | Speedup (Tseq/Tpar) |
|---|---|---:|---:|---:|
| 10000 x 10000 (C++) | static | 1 | 6797.3 | 1 |
| 10000 x 10000 (C++) | static | 2 | 3529.9 | 1.93 |
| 10000 x 10000 (C++) | static | 4 | 1782.2 | 3.81 |
| 10000 x 10000 (C++) | static | 8 | 1666.3 | 4.08 |
| 10000 x 10000 (C++) | static | 16 | 1710.5 | 3.97 |
| 10000 x 10000 (C++) | dynamic | 1 | 6401.6 | 1 |
| 10000 x 10000 (C++) | dynamic | 8 | 1570.7 | 4.08 |
| 10000 x 10000 (Java) | — | 1 | 7697.3 | 1 |
| 10000 x 10000 (Java) | — | 4 | 2098.5 | 3.67 |

## 6. Analysis

### 6.1 Sequential vs. parallel

**Large matrix (10000 x 10000): the convolution scales well.**
* Each element of V needs `k² = 25` multiply-adds, and each element of F is reused by 25
  windows that are close to each other in memory. So the work is limited by the CPU, not by
  memory bandwidth. That is the opposite of the vector addition in lab 0, which did one
  operation per 24 bytes and reached at most 1.3x.
* C++ reaches **1.91x with 2 threads and 3.81x with 4 threads**: 95 % efficiency on the 4
  physical cores.
* With 8 threads, hyperthreading adds about 5–7 % more, up to **4.08x**. The two hardware
  threads of a core share its arithmetic units, so they cannot double its throughput.
* 16 threads are slightly slower than 8 (3.97x): there are only 8 hardware threads, so the
  extra threads only add context switches.
* Java follows the same curve: 1.87x, 3.66x, 3.66x, 3.48x.

**Whole program.** Reading and writing ~1 GB of text is sequential and takes ~5.6 s in C++
(`total − compute`: 12.39 s − 6.80 s) and ~8.6 s in Java. So the best speedup of the whole
program is only **1.71x** (C++) and 1.53x (Java), as Amdahl's law predicts. This is why the
task asks to time only the computation.

**Medium and small matrices.**
* 1000 x 1000 still reaches 3.5–3.9x in C++ with the best variants (best: 7.1 ms with 8
  threads vs. 26.8 ms sequential, dynamic).
* The thin matrices (10 x 10000, 10000 x 10, 10⁵ elements) take only ~7 ms sequentially.
  Creating the threads (≈ 0.07 ms each) is still small compared with that, so up to 3.4x is
  reached.
* For **10 x 10** the sequential computation takes 4 µs (dynamic). Creating 4 threads costs
  ~0.3 ms, so every parallel variant is **12–90x slower** than the sequential one.

**Java on small and medium matrices: the parallel versions are always slower** (e.g. 1000 x 1000:
52 ms sequential, 57–120 ms parallel, and worse with more threads). As in lab 0, the cause is
JIT compilation: every execution is a new JVM, and the work (≤ 10⁶ elements) is over before
the code is fully compiled. The workers spend much of their time in the interpreter, while
the JIT compiler threads compete with them for the cores. An extra experiment on 1000 x 1000
(horizontal, 3 executions each) confirms this:

| Java Par, 1000 x 1000, compute time | p = 1 | p = 2 | p = 4 | p = 8 |
|---|---:|---:|---:|---:|
| normal JVM | ≈ 59 ms | ≈ 59 ms | ≈ 69 ms | ≈ 82 ms |
| `-Xint` (interpreter only, no JIT) | ≈ 1505 ms | ≈ 774 ms | ≈ 403 ms | ≈ 362 ms |

Without the JIT, the parallel version scales almost perfectly (3.7x with 4 threads). So the
threads are not the problem: the JIT compilation is, and it is not parallel. At 10⁸ elements
(test case 5), compilation takes a negligible part of the 2–8 s, and Java scales like C++.

### 6.2 Comparing the distributions

**Square matrices (1000², 10000²): all variants except cyclic are equivalent.**
* For 10000 x 10000 with 4 threads (C++ static):
  * horizontal, vertical and block: 1771–1782 ms
  * linear: 1861 ms (+4 %)
  * cyclic: 2055 ms (+15 %)
* Linear is slower because of its index loop: it handles 1D indices with a row carry, while
  the 2D loops of the others are simpler, and its chunks can start and end in the middle of
  a row.
* Cyclic is slower because neighbouring elements of V belong to different threads:
  * threads keep invalidating each other's cache lines of V (false sharing);
  * every thread reads all of F, using only every p-th window.
* The penalty stays small because every write of V comes with 25 multiply-adds. With
  16 threads, cyclic even comes close to the others (3.74x vs. 3.97x).

**Thin matrices: a one-dimensional distribution along the short side is unbalanced.**
* 10 x 10000 has only 10 rows, so `horizontal` cannot balance the work:
  * with p = 4, the threads get 2, 3, 2, 3 rows, so the best possible speedup is 10/3 = 3.33
    (measured: 2.82);
  * with p = 16, six threads get no row at all.
* `vertical` splits the 10000 columns evenly and reaches 3.25–3.41x with 4–8 threads.
* 10000 x 10 is the same situation transposed: `vertical` drops to 2.27–2.38x at 8–16 threads
  (dynamic), while `horizontal` reaches 3.44x.
* `block` chooses its grid from the shape of the matrix (1 x p and p x 1 here). So it always
  picks the good direction, and in C++ it is among the best in **every** test case.
* `linear` and `cyclic` ignore the shape. They split the N = n·m elements into balanced
  parts and are also never badly unbalanced.

### 6.3 Java vs. C++

* **Large data:** C++ is faster, but not by much.
  * Sequential: 6.40 s (C++ dynamic) vs. 7.70 s (Java), Java +20 %.
  * Best parallel: 1.57 s vs. 2.10 s, Java +34 %.
  * Once compiled, the inner loop is about the same in both. Java pays for the JIT warm-up
    at the start, and probably for array bounds checks.
* **Small and medium data:** C++ is faster by much more.
  * Sequential, vs. C++ dynamic: 1.9x (1000 x 1000), 2.9x (10000 x 10), 4.3x (10 x 10000),
    54x (10 x 10).
  * Parallel: C++ benefits from threads while Java does not (6.1).
  * The cause is compilation ahead of time vs. JIT compilation at run time.
* **I/O:** the parsers and formatters are written by hand in both languages. C++ reads and
  writes the 10000 x 10000 files in ~5.6 s, and Java in ~8.6 s.

### 6.4 Static vs. dynamic allocation (C++)

Dynamic allocation is **as fast or faster in every test case**:

| Sequential compute (ms) | 10 x 10 | 1000 x 1000 | 10 x 10000 | 10000 x 10 | 10000 x 10000 |
|---|---:|---:|---:|---:|---:|
| static | 0.026 | 34.14 | 7.56 | **35.03** | 6797 |
| dynamic | 0.004 | 26.83 | 6.96 | **7.15** | 6402 |

The difference is not in the computation. It comes from **page faults on the first write to
V**, which happen inside the timed region:

* The static `v[MAX][MAX]` lives in zero-initialised memory (BSS). Linux maps its pages only
  when they are first touched. A row of V is `MAX = 10000` ints (40 000 bytes) apart from the
  next, so for 10000 x 10 **every row of V is on a different 4 KiB page**. Writing V causes
  10 000 page faults in the computation, ≈ 2.8 µs each.
* The dynamic rows are allocated with `new int[m]` right after each other. For small m,
  `malloc` writes its bookkeeping next to every row while allocating, so the pages of V are
  already mapped before the timer starts.

Measured with `getrusage` (minor page faults during the computation only), with and without
writing V once before the timer starts ("pre-touch"):

| Sequential, compute | static | static, V pre-touched | dynamic |
|---|---|---|---|
| 10000 x 10 | 44.0 ms, 10 164 faults | 8.5 ms, 9 faults | 7.1 ms, 9 faults |
| 1000 x 1000 | 34.8 ms, 1 976 faults | 29.8 ms, 9 faults | 29.8 ms, 9 faults |
| 10 x 10000 | 7.9 ms, 106 faults | 7.4 ms, 9 faults | 7.8 ms, 97 faults |
| 10000 x 10000 (8 threads, horizontal) | 1740 ms, 97 678 faults | — | 1607 ms, 87 718 faults |

With V pre-touched, static and dynamic are equal. For 10000 x 10000, both allocations take
~90 000 faults (400 MB / 4 KiB), because there each row is larger than a page.

**Page faults also explain the bad static results for `vertical` and `cyclic` on 10000 x 10**
(0.77x with 8 threads):
* In these two variants every thread writes into every row of V, so all threads fault on the
  same pages at the same time.
* The kernel serialises them, and threads retry faults on pages that another thread is
  already mapping: **67 871 faults instead of 10 178** with 8 threads (measured).
* In `horizontal`, `block` and `linear`, each thread faults on its own pages, so the faults
  are handled in parallel too. Their static speedups even go above 4x (4.57x for linear with
  8 threads), because the sequential time includes the 10 000 faults.
* The same effect, in a milder form, makes static `cyclic` slower on 1000 x 1000 (2.54x vs.
  3.49x dynamic).

### 6.5 Best variant per matrix size

| Matrix | C++ static | C++ dynamic | Java | Overall best |
|---|---|---|---|---|
| 10 x 10 | sequential (0.026 ms) | sequential (0.004 ms) | sequential (0.20 ms) | **sequential, C++ dynamic** |
| 1000 x 1000 | linear, p = 8 (8.72 ms, 3.91x) | horizontal, p = 8 (7.08 ms, 3.79x) | sequential (52.1 ms) | **horizontal, p = 8, C++ dynamic** |
| 10 x 10000 | block, p = 4 (2.19 ms, 3.45x) | vertical, p = 8 (2.08 ms, 3.34x) | sequential (29.9 ms) | **vertical, p = 8, C++ dynamic** |
| 10000 x 10 | linear, p = 8 (7.67 ms, 4.57x) | block, p = 8 (2.08 ms, 3.44x) | sequential (20.4 ms) | **block (= horizontal), p = 8, C++ dynamic** |
| 10000 x 10000 | vertical, p = 8 (1666 ms, 4.08x) | block / vertical, p = 8 (1571 ms, 4.08x) | vertical, p = 4 (2099 ms, 3.67x) | **block / vertical, p = 8, C++ dynamic** |

In general:
* **C++ with dynamic allocation and 8 threads** (= number of hardware threads) is the best
  choice from 10⁵ elements up.
* Among the variants, the winner within a case is often decided by noise (differences of
  1–3 %).
* The robust choice is **`block`**: it adapts to the shape of the matrix and is never far
  from the best.
* Choose the opposite of the matrix shape (vertical for short-wide matrices, horizontal for
  tall-narrow ones), and avoid `cyclic` for a cache-friendly program.
* For tiny inputs (10 x 10), and in Java up to at least 10⁶ elements when each execution
  is a new JVM, the sequential program is the fastest.

### 6.6 Conclusion

Convolution with a 5 x 5 kernel is compute-bound. With a balanced distribution it scales
almost linearly up to the number of physical cores (3.8x on 4 cores, 4.1x with
hyperthreading). The way the work is split matters mainly when the matrix is thin, because a
split along the short side cannot be balanced. For square matrices, every variant except
cyclic is within a few percent of the others. The largest unexpected effect was not in the
parallel code at all: the fixed row stride of a static `int[MAX][MAX]` turns the first write
of every row into a page fault. That made the static program up to 5x slower on 10000 x 10,
and made two of the distributions slower than sequential.

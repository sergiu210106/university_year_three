# Lab 0 — Vector addition (sequential vs. parallel, C++ and Java)

## 1. Requirements analysis

### 1.1 Topic

Given two vectors **A** and **B** with *n* real elements, compute the vector
**C = A + B**, i.e. `C[i] = A[i] + B[i]` for `i = 0 … n-1`, and measure the execution time.

### 1.2 Requirements

| # | Requirement | Where it is solved |
|---|---|---|
| 1 | Programs in C++ and Java, sequential and parallel | `cpp/seq.cpp`, `cpp/par.cpp`, `java/Seq.java`, `java/Par.java` |
| 1.1 | Input read from a file, result written to a file; input files randomly generated | all programs, `scripts/generate_input.cpp` |
| 1.2 | Execution time printed to stdout | all programs |
| 2 | Scripts for automated testing of both languages | `scripts/run_tests.sh` |
| 2.1 | Correctness checked for 10 independent runs | `scripts/run_tests.sh` (section 4.2) |
| 3 | Execution times collected, average computed for each test case | `times.csv`, `averages.csv` |
| 4 | Documentation | this file |

### 1.3 Constraints

From the course rules:

* Data is **always loaded from an input file**, and every result is written to an output file.
* **Correctness:** for *every* execution (all 10 runs of every test), the output file
  of the parallel program must be **identical** to the output file of the sequential
  program on the same input. This is checked by a function that compares the
  content of two files.
* **Performance:** every variant is executed **at least 10 times** and the average time is
  the result. The time of the **whole program** is measured, plus optionally the time of
  specific sections. Here the optional section is the computation.
* The parallel variants are compared with a **sequential** implementation (speedup).
* A script runs the programs repeatedly and saves every time in a `.csv` file. A program
  must **not** repeat the work internally and time each repetition.

Assumptions made:

* "Real" numbers are represented as `double` (64-bit IEEE 754) in both languages.
* `n` fits in an `int`. The input is plain text, so very large `n` is limited anyway by
  the file size: n = 10⁶ already gives a ~22 MB input file.
* The number of threads `p` is ≥ 1. It may be larger than `n` (some threads then get no work).

## 2. Design

### 2.1 Data structures

| Data | C++ | Java |
|---|---|---|
| vectors A, B, C | `vector<double>` of size n | `double[]` of length n |
| threads | `vector<thread>` | `Worker[]` (`Worker extends Thread`) |

C is allocated once, before the computation. Each thread writes directly into its
own part of C, so no extra buffers are needed.

### 2.2 Thread partitioning

The index range `[0, n)` is split into **p contiguous blocks**, one per thread.
Thread `t` (`t = 0 … p-1`) computes

```
C[i] = A[i] + B[i]     for i in [ t·n/p , (t+1)·n/p )       (integer division)
```

Properties:

* The blocks follow each other with no gaps or overlaps (block `t` ends where block
  `t+1` starts). The first starts at 0 and the last ends at `n`, so every element is
  computed exactly once.
* Block sizes differ by at most one element, so the work is balanced.
  Example: `n = 10, p = 3` gives `[0,3) [3,6) [6,10)`.
* **No synchronization is needed during the computation.** A and B are only read,
  and every C[i] is written by exactly one thread. The main thread waits for all
  workers with `join()`. In both C++ and Java, `join()` also guarantees that the
  workers' writes are visible to the main thread.
* Contiguous blocks (instead of giving thread `t` the indices `t, t+p, t+2p, …`) mean
  two threads almost never write the same cache line, so there is no false sharing.

### 2.3 Components and their specification

**`generate_input <n> <file> <seed>`** (`scripts/generate_input.cpp`)
* *in:* size `n ≥ 0`, output file name, seed for the random generator
* *out:* writes `n` on the first line, n values of A on the second line and n values
  of B on the third line. Values are uniformly random reals in `[-1000, 1000]`,
  written with 10 significant digits. The same seed always produces the same file.

**`seq <input_file> <output_file>`** (`cpp/seq.cpp`) and **`java Seq <input_file> <output_file>`** (`java/Seq.java`)
* *pre:* the input file has the format above
* *post:* the output file contains `n` on the first line and `C[0] … C[n-1]` on the
  second, with `C[i] = A[i] + B[i]`. Prints one line to stdout,
  `<compute_ms> <total_ms>` (section 3.2).

**`par <input_file> <output_file> <p>`** (`cpp/par.cpp`) and **`java Par <input_file> <output_file> <p>`** (`java/Par.java`)
* *pre:* same as sequential; `p ≥ 1`
* *post:* same as sequential. The output file must be identical to the sequential
  program's output on the same input, in the same language. The computation is
  done by `p` threads using the partitioning in 2.2.
* C++: each thread runs a lambda that captures A, B, C by reference and its block
  bounds `from`, `to` by value.
* Java: class **`Par.Worker extends Thread`**
  * `Worker(double[] a, double[] b, double[] c, int from, int to)`: stores the
    references to the arrays and the bounds of the block (`0 ≤ from ≤ to ≤ n`).
  * `run()`: post: `c[i] = a[i] + b[i]` for every `i` in `[from, to)`. It touches
    no other element of `c`.

**`scripts/run_tests.sh [runs]`**
* `same_content <file1> <file2>`: true if and only if the two files have exactly the
  same content (byte by byte, via `cmp`).
* `run_program <impl> <threads> <command…>`: runs one program and appends its two times
  to `times.csv`. For a parallel program (`*_par`) it also checks with `same_content`
  that its output equals the sequential output, and counts a failure if not.
* At the end, computes `averages.csv` (average times and speedup per test case). The
  script exits with code 1 if any parallel result differed from the sequential one.

### 2.4 Relations between components

```
                    seed = n + run
generate_input ──────────────────────► in.txt
                                          │
            ┌─────────────────────────────┼──────────────────────────────┐
            ▼                             ▼                              ▼
   seq / java Seq                par / java Par (p = 2,4,8,16)    (stdout of every program)
            │                             │                              │
            ▼                             ▼                              ▼
       out_seq.txt ◄── same_content ──► out_par.txt         times.csv ──► averages.csv
     (reference)       OK / WRONG                           (all runs)     (avg + speedup)
```

## 3. Implementation details

### 3.1 Project structure

```
0/
├── cpp/seq.cpp, cpp/par.cpp          C++ sequential / parallel (std::thread)
├── java/Seq.java, java/Par.java      Java sequential / parallel (java.lang.Thread)
├── scripts/generate_input.cpp        random input generator
├── scripts/run_tests.sh              build + correctness + performance testing
├── times.csv, averages.csv           results (generated)
└── build/                            binaries, .class files, current input/outputs (generated)
```

Tools: g++ 15.2 with `-O2` (`-pthread` for `par`), OpenJDK 21. The test script
compiles everything itself.

### 3.2 Time measurement

Each program measures two intervals and prints them, in milliseconds, on one line:
`<compute_ms> <total_ms>`.

* **compute**: only the addition. In the sequential programs this is the loop. In the
  parallel programs it starts before the threads are created and ends after the last
  `join()`, so creating and joining threads is included: that is part of the cost of
  doing the work in parallel.
* **total**: the whole program, from the start of `main` to after the output file is
  written and closed: reading, computing, writing. Starting the process / the JVM
  happens before `main` and is not included.

Clocks: `std::chrono::steady_clock` in C++ and `System.nanoTime()` in Java. Both are
monotonic, so they are not affected by changes to the system time.

Every program does the work **once** per execution. The 10 repetitions are 10
separate executions started by the script.

### 3.3 Output format

* C++: fixed notation with 10 decimals (`fixed << setprecision(10)`).
* Java: `Double.toString`, the shortest text that converts back to exactly the same `double`.

The sequential and parallel programs of the same language use the same code for
writing, so their output files are identical exactly when the computed values are
identical. Outputs are not compared *across* languages. I tried a common format
(`%.10f` in Java), but Java rounds starting from the shortest decimal representation
while C++ rounds the exact binary value. The two then differ in the last decimal for
about 5 values in 10⁶.

### 3.4 Other details

* Java reads the input line by line (`readLine()` + `split(" ")`), which relies on the
  3-line input format. C++ reads it with `>>`.
* In Java the worker is a class (`Worker extends Thread`) and not a lambda. The first
  lambda a JVM creates costs about 15 ms of `invokedynamic` setup, and that would land
  inside the timed region. Measured at n = 10: ~16 ms with a lambda, 2–5 ms without.
* Times are printed with 6 decimals, using `Locale.ROOT` in Java, and the script runs
  with `LC_ALL=C`. On a system with a Romanian locale, decimals would otherwise be
  printed with a comma, which breaks the CSV files.

## 4. Test cases

### 4.1 Test matrix

| Parameter | Values |
|---|---|
| vector size n | 10, 1 000, 100 000, 1 000 000 |
| variants | C++ seq, C++ par with p = 2, 4, 8, 16, Java seq, Java par with p = 2, 4, 8, 16 |
| runs per test case | 10 |
| input | a new random file per run, seed `n + run` (reproducible) |

This gives 4 sizes × 10 variants × 10 runs = **400 executions**: 320 parallel ones,
each checked against the sequential result. Inside one run, all variants work on the
same input file.

Some edge cases were also run separately (`SIZES="7 100003" THREADS="3 16"`): `n` not
divisible by `p`, and `n < p` (n = 7, p = 16, so some threads have empty blocks).

### 4.2 Correctness verification

For every run and every language:

1. the sequential program writes the reference output `out_seq.txt`;
2. every parallel variant writes `out_par.txt`;
3. `same_content out_par.txt out_seq.txt` must be true, otherwise the run is
   reported as `WRONG` and the script fails.

To show that the check can actually fail, the test was also run on a copy of the
project with a deliberate bug in `par.cpp`: each thread skips the last element of its
block. The script reported `FAILED: 1 parallel result(s) different from the sequential
result` and exited with code 1.

### 4.3 Performance testing

* Every variant runs **10 times** for every n. `times.csv` stores both times of
  every execution.
* `averages.csv` stores, for every test case `(impl, n, threads)`, the average compute
  time, the average total time, and the **speedup** against the sequential program
  of the same language: `speedup = T_seq / T_par`. A value above 1 means the parallel
  version is faster.

### 4.4 How to run

All commands are run from the `0/` directory.

**Full test (build + correctness + performance).** The script compiles everything
itself and writes `times.csv` and `averages.csv`.

```bash
scripts/run_tests.sh                             # full test, 10 runs per test case
scripts/run_tests.sh 3                           # choose the number of runs
SIZES="10 1000000" THREADS="4 8" scripts/run_tests.sh 3   # choose sizes / thread counts
SIZES="7 100003" THREADS="3 16" scripts/run_tests.sh 10   # edge cases: n % p != 0, n < p
```

**Build by hand** (only needed to run the single programs below without the test script):

```bash
mkdir -p build
g++ -O2 -o build/generate_input scripts/generate_input.cpp
g++ -O2 -o build/seq cpp/seq.cpp
g++ -O2 -pthread -o build/par cpp/par.cpp        # -pthread is required for the parallel one
javac -d build java/Seq.java java/Par.java
```

**Generate a random input file** (`generate_input <n> <output_file> <seed>`, same seed → same file):

```bash
build/generate_input 1000000 build/in.txt 42
```

**Run a single program.** Each prints one line to stdout: `<compute_ms> <total_ms>`.

```bash
build/seq build/in.txt build/out_seq.txt         # C++ sequential
build/par build/in.txt build/out_par.txt 4       # C++ parallel, 4 threads
java -cp build Seq build/in.txt build/out_seq.txt       # Java sequential
java -cp build Par build/in.txt build/out_par.txt 8     # Java parallel, 8 threads
```

`<threads>` can be any `p ≥ 1`; it may exceed `n` (extra threads get empty blocks).

**Check two outputs are identical** (the correctness test, done by hand):

```bash
cmp build/out_par.txt build/out_seq.txt && echo IDENTICAL || echo DIFFERENT
```

**Inspect the results:**

```bash
column -s, -t < averages.csv     # averages + speedup, pretty-printed
head times.csv                   # per-run data
```

If your shell uses a comma as the decimal separator, prefix the single programs with
`LC_ALL=C` (the test script already does this), e.g.
`LC_ALL=C build/par build/in.txt build/out_par.txt 4`.

## 5. Results analysis

### 5.1 Environment

Intel Core i7-1065G7 (4 cores / 8 hardware threads, 1.3 GHz base, 8 MiB L3 cache),
15 GiB RAM, Linux 7.0, g++ 15.2 `-O2`, OpenJDK 21.
Every value below is the average of **10 runs**. All 320 parallel results were
identical to the corresponding sequential results.
Full data: `times.csv` (every execution) and `averages.csv` (averages and speedups).

### 5.2 Results

#### Table 1 — average computation time (ms)

| n | C++ seq | C++ p=2 | C++ p=4 | C++ p=8 | C++ p=16 | Java seq | Java p=2 | Java p=4 | Java p=8 | Java p=16 |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 10 | 0.0002 | 0.252 | 0.450 | 0.570 | 0.918 | 0.0015 | 1.777 | 2.421 | 2.848 | 4.251 |
| 1000 | 0.0014 | 0.268 | 0.361 | 0.837 | 1.198 | 0.041 | 2.236 | 2.652 | 3.267 | 4.428 |
| 100000 | 0.198 | 0.490 | 0.489 | 0.785 | 1.104 | 0.804 | 5.957 | 6.328 | 6.474 | 6.840 |
| 1000000 | 1.874 | 1.575 | 1.448 | 1.561 | 1.588 | 10.845 | 24.524 | 43.389 | 47.177 | 46.626 |

#### Table 2 — average total program time (ms)

| n | C++ seq | C++ p=2 | C++ p=4 | C++ p=8 | C++ p=16 | Java seq | Java p=2 | Java p=4 | Java p=8 | Java p=16 |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 10 | 0.348 | 0.586 | 0.839 | 0.884 | 1.244 | 5.200 | 7.155 | 8.101 | 8.523 | 9.944 |
| 1000 | 2.466 | 2.842 | 2.879 | 3.482 | 3.741 | 38.781 | 39.868 | 40.578 | 40.792 | 41.245 |
| 100000 | 204.3 | 204.3 | 209.0 | 207.3 | 203.9 | 400.7 | 397.9 | 403.4 | 412.0 | 398.6 |
| 1000000 | 1857.3 | 1825.3 | 1882.1 | 1835.4 | 1846.2 | 1664.7 | 1628.4 | 1660.4 | 1687.5 | 1642.7 |

#### Table 3 — speedup of the computation (T_seq / T_par, same language)

| n | C++ p=2 | C++ p=4 | C++ p=8 | C++ p=16 | Java p=2 | Java p=4 | Java p=8 | Java p=16 |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 10 | 0.001 | 0.000 | 0.000 | 0.000 | 0.001 | 0.001 | 0.001 | 0.000 |
| 1000 | 0.005 | 0.004 | 0.002 | 0.001 | 0.018 | 0.016 | 0.013 | 0.009 |
| 100000 | 0.404 | 0.405 | 0.252 | 0.179 | 0.135 | 0.127 | 0.124 | 0.118 |
| 1000000 | 1.190 | 1.294 | 1.200 | 1.180 | 0.442 | 0.250 | 0.230 | 0.233 |

#### Table 4 — speedup of the whole program (T_seq / T_par, same language)

| n | C++ p=2 | C++ p=4 | C++ p=8 | C++ p=16 | Java p=2 | Java p=4 | Java p=8 | Java p=16 |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 10 | 0.594 | 0.415 | 0.394 | 0.280 | 0.727 | 0.642 | 0.610 | 0.523 |
| 1000 | 0.868 | 0.857 | 0.708 | 0.659 | 0.973 | 0.956 | 0.951 | 0.940 |
| 100000 | 1.000 | 0.978 | 0.986 | 1.002 | 1.007 | 0.993 | 0.973 | 1.005 |
| 1000000 | 1.018 | 0.987 | 1.012 | 1.006 | 1.022 | 1.003 | 0.986 | 1.013 |

(A speedup above 1 means the parallel version is faster; `0.000` means below 0.0005.)

### 5.3 Analysis

**1. The whole program is dominated by reading and writing the files.** For C++ at
n = 10⁶, the program takes ≈ 1857 ms, of which the addition is ≈ 1.9 ms: about
**0.1 %**. The other 99.9 % is parsing 2·10⁶ numbers from text and writing 10⁶
numbers back, and that work is sequential. By Amdahl's law, with a parallel fraction
f ≈ 0.001 the whole-program speedup cannot exceed `1 / (1 − f) ≈ 1.001`, whatever
the number of threads. Table 4 matches this: for n ≥ 10⁵ every speedup is between
0.97 and 1.03, i.e. the parallel and sequential programs take the same time within
measurement noise. For small n (10, 1000) the whole program is so short that the
cost of starting threads becomes visible, and the speedup drops below 1
(down to 0.28).

**2. The computation itself barely scales: it is limited by memory bandwidth.** Each
element needs one addition and 24 bytes of memory traffic (two reads, one write).
At n = 10⁶ the three arrays take 24 MB, more than the 8 MiB L3 cache, so all the data
comes from RAM. One core can already use most of the bandwidth. The best C++ result
is **1.29× with 4 threads** (1.45 ms vs. 1.87 ms). 8 and 16 threads do not help
further: the CPU has 4 physical cores, and all of them wait on the same memory bus.

**3. For small n, starting threads costs more than the work.** Creating and joining a
thread costs tens of microseconds. The sequential C++ loop takes 0.0002 ms for
n = 10 and 0.2 ms for n = 10⁵. So below 10⁶ every parallel variant is slower than the
sequential one (Table 3), and it generally gets slower as threads are added
(n = 10: 0.25 ms with 2 threads, 0.92 ms with 16).

**4. Java parallel is dominated by JIT compilation.** Each execution starts a new JVM,
so the code always runs from a cold start. In `Seq`, the loop is inside `main` and
gets compiled once while it runs (on-stack replacement). In `Par`, every
`Worker.run()` starts in the interpreter, and the JIT compiler threads compete with
the workers for the cores while they compile. That is why Java Par at n = 10⁶ is
*slower* than Java Seq (24–47 ms vs. 10.8 ms), and mostly slower with more threads. To
confirm this explanation, Java Par was also run separately with different JVM settings:

| Experiment (n = 10⁶, Java Par, compute time) | p = 1 | p = 2 | p = 8 |
|---|---|---|---|
| Normal JVM, cold start | ≈ 12 ms | ≈ 27 ms | ≈ 58 ms |
| `-XX:+UseSerialGC` (rules out the garbage collector) | ≈ 12 ms | ≈ 28 ms | ≈ 62 ms |
| `-Xint` (interpreter only, no JIT) | ≈ 42 ms | ≈ 24 ms | ≈ 16 ms (1 run) |
| JIT-compiled (same JVM, last 3 of 20 repetitions) | ≈ 3 ms | ≈ 2.6 ms | ≈ 3 ms |

With the JIT turned off, the parallel version gets faster as threads are added
(42 → 24 → 16 ms), because the work is then CPU-bound. Once the code is compiled,
the time is about 3 ms whatever the thread count, close to C++ and limited by memory
bandwidth (point 2). (The last row runs the work 20 times inside one program. That
was done only for this experiment, never for the measurements in 5.2.)

**5. C++ vs. Java.** For the computation, C++ is about 6× faster at n = 10⁶
(1.9 ms vs. 10.8 ms): it is compiled ahead of time with `-O2`, while Java still has
to compile the loop while running it. For the whole program, the order reverses at
n = 10⁶: Java takes 1665 ms and C++ 1857 ms. Most of the time goes into reading and
writing text. That part was not timed separately, but the likely cause is that C++
formatted stream I/O (`>>` / `<<` on doubles) is slower than Java's `split` +
`Double.parseDouble` / `Double.toString`. For small n Java is
slower overall (5.2 ms vs. 0.35 ms at n = 10) because of class loading and code that
still runs in the interpreter.

### 5.4 Conclusion

Vector addition does one operation per element and is limited by memory bandwidth.
Splitting it across threads speeds up the computation by at most ~1.3× (C++,
n = 10⁶, 4 threads), and makes it slower for small inputs. For the whole program the
computation is only ~0.1 % of the time; the rest is file I/O, so parallelizing the
addition does not change the total run time. Parallelism pays off when each element
needs much more computation than memory traffic, or when the sequential part
(here, the I/O) is small.

# Laboratory 2: In-place Convolution

Consider the problem from [Laboratory 1](../../3/lab1_assignment/task.md) with the following modification.

## Objective and Constraints

Optimize space complexity while achieving high performance.

**Postcondition:** The initial matrix contains the filtered image.

- **No other result matrix and no temporary matrix may be allocated.**
- Only temporary vectors may be used, with space complexity within `O(n)`.
- The sequential implementation also requires temporary auxiliary vectors.

## Input and Output

- Read input from `date.txt`, created beforehand using randomly generated numbers.
- Save the result in `output.txt`.

## Implementation Requirements

Implement both:

- Java.
- C++ (at least C++11).

Compared with Laboratory 1, only the following are required:

- Row-based distribution.
- Kernel size `k = 3`.
- Dynamic allocation for C++.

## Testing

Measure execution time for these cases, including sequential execution in each case:

| Image dimensions (`N × M`) | Kernel dimensions (`n × m`) | Parallel thread counts (`p`) |
|---|---|---|
| 10 × 10 | 3 × 3 | 2 |
| 1000 × 1000 | 3 × 3 | 2, 4, 8, 16 |
| 10000 × 10000 | 3 × 3 | 2, 4, 8, 16 |

- Repeat each test **10 times** and report the arithmetic mean of the execution times.
- For every execution of every test case, use the same input file and verify correctness against the output file obtained from sequential execution.

## Analysis

- Compare performance for each case.
- Compare Java and C++ execution times.
- Evaluate space complexity.

## Deadline

**Week 5.**

# Lab 3 study notes

For the full beginner-to-code walkthrough, see the [complete study guide](study_guide.md). This file preserves the implementation review and recorded performance analysis.

## Representation and carry

Digits are `unsigned char` values from 0 to 9, not character codes. Index zero is the least significant digit, so processing indices in increasing order follows the ordinary carry dependency. At each digit, `sum = first[i] + second[i] + carry`, the result is `sum % 10`, and the next carry is `sum / 10`.

The largest digit sum is 19, so each carry is zero or one. Operand lengths are validated before any digit buffer is filled. Padding the shorter input preserves the larger declared width instead of trimming leading zeroes.

## Professor review before testing

The review found no unresolved assignment violations. These were the main checks and implementation decisions:

| Check | Review conclusion |
| --- | --- |
| Stack capacities | `100000 + 16 + 1 = 100017` bounds operand padding plus a possible carry. The padded collective length is explicitly checked. No digit buffer is global, static, dynamically allocated, or variable-length. |
| Complete coverage | Quotient/remainder counts and offsets create adjacent slices totaling exactly `N`. Workers can pass a carry through a zero-length slice. |
| Streaming rank 0 | Variants 1 and 1.1 reuse operand chunk arrays and write each received result segment immediately. Variant 3 reads each slice immediately before posting its sends, retaining disjoint slices until completion. None of these masters adds digits. |
| Output header | Six character positions reserve enough space for counts through 100001. Updating the header cannot overwrite the first digit. Every variant uses the same formatting. |
| Carry preservation | Variant 1.1 keeps the speculative carry when correcting local digits. Variant 2 examines `result[N]` when padding absorbed the true final carry. |
| Blocking safety | Each worker receives both inputs before its carry, then sends its carry before its result. Root continues distribution without waiting for results. This remains safe with synchronous sends. |
| Nonblocking lifetimes | All active requests fit their local fixed arrays; each request is waited before its buffer is read, overwritten, or destroyed. |
| Readability and scope | Short procedural helpers share file format, arithmetic, partitioning, blocking communication, and timing. Separate entry points select each required algorithm. |

The required suite confirmed all 42 parallel outputs against the sequential implementation, including all 14 synchronous-send runs. The required cases do not exercise every supported limit or every remainder/padding path, so the capacity and coverage arguments above also rely on source review.

## Why Variant 1 cannot deadlock with synchronous sends

Assume every send waits until its matching receive starts:

1. Root sends both operands to worker 1. Worker 1 has matching operand receives and starts with carry zero.
2. Worker 1 may block sending its carry to worker 2, because worker 2 must receive its operands first. Root has finished sending to worker 1 and proceeds to send both operands to worker 2. Root does not wait for worker 1's carry or result.
3. Once worker 2 has both operands, it posts its carry receive and releases worker 1. The same argument applies successively to every worker.
4. Workers send the carry before sending their result. A worker blocked returning its result cannot prevent the next worker from receiving the carry it needs.
5. Root finishes distribution, then receives and writes results from rank 1 upward. The last worker sends its final carry after its result, matching root's final result receive followed by its final carry receive.

There is no circular dependency: distribution can always advance, and afterward result collection can always advance. Variant 1.1 performs speculative local computation between the operand receives and the carry receive, preserving this ordering. `SYNCHRONOUS_SEND` switches the shared send helper from `MPI_Send` to `MPI_Ssend` for inputs, carries, and results, so the verification checks all blocking point-to-point sends.

## Variant 1.1 correction and cost

Each worker first computes its slice with incoming carry zero, saving both the result digits and outgoing carry. An actual incoming carry of one requires incrementing the result from its least significant digit. The increment stops at the first digit below nine, or overflows after turning an entire slice of nines into zeroes.

The outgoing carry is the speculative carry OR the increment overflow. The speculative carry must survive correction. A speculative outgoing carry and increment overflow cannot both occur for a nonempty slice: for base `B = 10^k`, two operands sum to at most `2B-2`, whereas an outgoing carry plus an all-nine result would require `2B-1`. Empty slices have speculative carry zero and pass an incoming carry through the increment helper.

This moves the full local addition ahead of the incoming carry. In Variant 1, the carry chain includes every worker's addition. In Variant 1.1, the chain mostly waits for short corrections once speculative work is complete. The worst correction still scans the entire slice. Root's incremental reading and distribution delay when later workers can start, and file I/O, communication, and process scheduling can dominate the cheap decimal arithmetic.

The observed standard-send comparison was:

| Operand lengths | Processes | Variant 1 ms | Variant 1.1 ms | Variant 1 / Variant 1.1 |
| --- | ---: | ---: | ---: | ---: |
| 16 and 16 | 5 | 0.971 | 0.954 | 1.018 |
| 10,000 and 10,000 | 5 | 1.786 | 1.617 | 1.105 |
| 10,000 and 10,000 | 9 | 1.699 | 1.575 | 1.079 |
| 10,000 and 10,000 | 17 | 2.356 | 2.503 | 0.941 |
| 100 and 100,000 | 5 | 7.555 | 7.392 | 1.022 |
| 100 and 100,000 | 9 | 7.293 | 6.862 | 1.063 |
| 100 and 100,000 | 17 | 9.513 | 9.175 | 1.037 |

Ratios above one favor Variant 1.1. Its measured reductions range from 1.75% to 9.46% in the six favorable configurations; the 17-process equal-length run takes 6.24% longer. With synchronous sends, Variant 1.1 wins four of seven configurations, with ratios ranging from 0.932 to 1.642. Those runs verify communication safety; the timing variation also illustrates why one measurement cannot establish a consistent optimization benefit.

## Variant 2: padding and collectives

Rank 0 reads both operands and pads to `ceil(N/p) * p`. Two scatters distribute equal-sized slices. Rank 0 adds its slice with initial carry zero; every other rank waits for the previous carry. Each rank forwards its carry before entering the gather. The final carry is broadcast from the last rank after gathering.

The ordered carry chain has a receive at every next rank after both scatters complete. A rank entering the gather cannot prevent a later rank from receiving the already forwarded carry. Every rank calls the collectives in the same order.

When padded length equals `N`, the last rank's carry determines whether to append a digit. When padded length exceeds `N`, carry propagation has already placed the true overflow at `result[N]`, and subsequent padding digits are zero. Inspect that digit before removing the padding. Using only the carry after the padded range would incorrectly discard an overflow absorbed by padding.

The required 1,000-digit case with 16 processes exercises collective padding to 1,008 digits. The largest required 100,000-digit case divides evenly by 4, 8, and 16.

## Variant 3: asynchronous communication

Root stores two operand arrays and one result array on its stack. The operand arrays are filled incrementally, one worker slice at a time. After posting an `MPI_Isend`, root fills only later, disjoint positions. It also posts one result receive per worker and a separate receive for the final carry. Up to 32 input-send requests and 17 result/carry-receive requests fit the fixed arrays.

Workers post both operand receives and, except for worker 1, an incoming carry receive before waiting. This permits the carry and operands to arrive in any order. They compute only after all applicable receives complete. Each worker posts its outgoing carry and result sends, then waits for both before returning. Root waits for all operand sends and all receives before appending a carry or writing output.

All matching receives can be posted independently of send completion, so the carry chain has no cyclic wait. Buffers remain alive throughout every associated request. Nonblocking calls allow overlap but do not guarantee background progress during file reading; actual progress depends on the MPI implementation and scheduling.

The assignment's `N/p` wording would leave digits undistributed with one I/O-only rank and `p-1` workers. This implementation partitions over `p-1` so all digits participate, as documented in the README.

## Interpreting the measurements

The suite uses a single sequential baseline for each of four operand pairs and a single measurement for every required parallel configuration. Speedups in both CSV files use the matching baseline. Input generation and `cmp` run outside the timed region. MPI measurements include the completion barrier so root's closed output is included even when workers finish earlier.

For 10,000 digits, all three worker-only variants were slower than sequential at 17 processes, with speedups from 0.732 to 0.803. For 100 and 100,000 digits, their 17-process speedups were 0.803 to 0.858, while Variant 2 at 16 processes measured 1.024. These observations are consistent with scheduling and communication costs, but the suite does not isolate their contributions.

All runs used local oversubscription; the host reports 14 logical CPUs. Sequential references run before their parallel comparisons, so cache warmth and fixed execution order can bias apparent gains. The large 1.652 speedup for Variant 2's 1,000-digit, eight-process run should therefore be treated as a noisy observation. More repetitions would be needed for performance conclusions, but were outside the requested test scope.

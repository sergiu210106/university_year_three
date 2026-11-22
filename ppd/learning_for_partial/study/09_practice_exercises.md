# Practice exercises (exam style, answer on paper)

Solutions are in `10_practice_solutions.md` (kept separate on purpose). Write first, then compare.
Unless stated otherwise: C/C++ with MPI, `MPI_COMM_WORLD`, write only the MPI-relevant code but include
Init/rank/size/Finalize. Suggested time per programming problem: 10-15 minutes.

---

**P1. Master/worker sum (point-to-point).**
Rank 0 holds an array `a` of `N = 1000` ints. Write a program with `p >= 2` processes where rank 0 does **no summation**; it sends `N/(p-1)` consecutive elements (the last worker takes the remainder) to each worker with `MPI_Send`; each worker sums its part and sends the partial sum back; rank 0 adds the partial sums in rank order and prints the total.

**P2. Same sum with collectives.**
Write the sum of `N` ints using `MPI_Bcast`, `MPI_Scatter` and `MPI_Reduce`. Assume `N` is divisible by `p` and that rank 0 also takes part in the computation.

**P3. Dot product.**
Vectors `x`, `y` (doubles, length `N`, `N % p == 0`) are on rank 0. Compute `sum x[i]*y[i]` so that **every** rank ends up knowing the result. Which collective do you use at the end?

**P4. Running (prefix) sum with a chain.**
Each rank `r` has one int `v = r + 1`. Rank `r` must print the sum `v_0 + ... + v_r`. Use only `MPI_Send`/`MPI_Recv` (no collectives). Then write the one-call version with a collective.

**P5. Uneven split.**
`N = 10`, `p = 4`. Rank 0 holds `a[10]`. Use `MPI_Scatterv` so that every rank (rank 0 included) gets a slice (3,3,2,2 elements). Fill in `counts[]` and `displs[]` and write the call. Then gather the slices back with `MPI_Gatherv`.

**P6. Count occurrences.**
Rank 0 reads `N` (arbitrary) and an array; the value `target` is broadcast; each rank counts how often `target` occurs in its slice; the total is printed by rank 0. Handle `N` not divisible by `p` by padding with a value that cannot equal `target`, or with `Scatterv`.

**P7. Find the deadlock.**
```cpp
// p = 4 (ring)
int right = (rank + 1) % size, left = (rank + size - 1) % size;
MPI_Send(&x, 1, MPI_INT, right, 0, MPI_COMM_WORLD);
MPI_Recv(&y, 1, MPI_INT, left,  0, MPI_COMM_WORLD, &st);
```
(a) Is it safe? Explain using the `Ssend` test. (b) Give three different fixes (one with `Sendrecv`, one with `Isend`/`Irecv`, one with message ordering by rank parity).

**P8. Non-blocking squares.**
Rank 0 sends one distinct int to each of the other ranks with `MPI_Isend`; each worker receives with `MPI_Irecv`, computes its square, returns it with `MPI_Isend`; rank 0 collects all results with `MPI_Irecv`, completes everything with `MPI_Waitall`, and prints the squares in rank order. Use arrays of requests.

**P9. Big number times a digit (carry chain).**
A big number `A` (N digits, LSD first, `N` up to 100000) is multiplied by a single digit `k` (0-9). Variant of Lab 3 V1: rank 0 reads `A`, distributes slices to `p-1` workers (blocking sends), worker `i` waits for the carry from `i-1` after receiving its slice, computes `digit*k + carry`, sends carry onward; rank 0 collects the slices in order. (a) What is the maximum carry value? (b) Write the worker code. (c) Why is the carry here an `int` that can exceed 1, unlike in addition?

**P10. Padding in Scatter/Gather.**
In Lab V2, `N = 1000` and `p = 16`. (a) How many digits does each process get and how large is the padded array? (b) Where is the final carry? (c) What goes wrong if you call `MPI_Scatter(a, N/p, ...)` with `N/p = 62`?

**P11. Matrix-vector product.**
Matrix `M` (`R x C`, `R % p == 0`, stored row-major) and vector `v` (length `C`) are on rank 0. Compute `M*v` on `p` processes: broadcast `v`, scatter the rows, compute locally, gather the result on rank 0.

**P12. Find the bugs (at least 6).**
```cpp
int main(int argc, char **argv) {
    int rank, size, n = 0;
    int data[100];
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Init(&argc, &argv);
    if (rank == 0) {
        n = 100;
        MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
        for (int i = 1; i < size; i++)
            MPI_Send(data, n * sizeof(int), MPI_INT, i, 0, MPI_COMM_WORLD);
    } else {
        MPI_Recv(data, 50, MPI_INT, 0, 1, MPI_COMM_WORLD);
    }
    MPI_Isend(&n, 1, MPI_INT, (rank+1)%size, 5, MPI_COMM_WORLD, &req);
    n = rank;
    MPI_Recv(&n, 1, MPI_INT, MPI_ANY_SOURCE, 5, MPI_COMM_WORLD, &st);
}
```

**P13. Ping-pong timing.**
Write a program for two processes that bounces a message of `m` doubles back and forth `R = 100` times and prints the average one-way time using `MPI_Wtime`. Which side sends first? Why is this exchange safe?

**P14. Unknown-length message.**
Rank 0 sends several strings of different lengths to rank 1 (tags 0, 1, 2). Rank 1 must print each without knowing the lengths beforehand. Which function do you need?

**P15. Lab V1 modifications (theory).**
(a) The worker is changed to `Recv` the carry **before** its chunks. Is the program still correct? Is it deadlock-free with `Ssend`? What changes in performance? (b) The master is changed so that it **collects the result of worker 1 right after sending it its chunks**, before sending chunks to worker 2. Show with a wait-for graph that this deadlocks when `Send` is `Ssend` and `p >= 3`. (c) Explain why V1.1 (speculative addition) can produce the same output as V1.

**P16. Choose the function.**
Which single call(s) do the following? (a) every process needs the global maximum of a local double; (b) one process must collect one int from every process into an array; (c) a value known by rank 2 must reach everyone; (d) rank 0 sends a different int to each rank; (e) wait until all processes have finished a phase; (f) check without blocking whether any message is waiting; (g) wait for whichever of three posted receives finishes first; (h) exchange an int with a partner without deadlock.

**P17. Lab V3 worker.**
Write the code of a V3 worker (rank `id > 0`, `w` workers): non-blocking receives for the two digit chunks and the carry from `id-1` (not for `id = 1`), compute, and non-blocking sends of the carry and the result. State exactly which buffers must not be touched and until when.

**P18. Explain the output.**
```cpp
// p = 3
int x = rank + 1, y;
MPI_Scatter(buf3, 1, MPI_INT, &y, 1, MPI_INT, 1, MPI_COMM_WORLD);   // buf3 = {7,8,9} on every rank
MPI_Allreduce(&y, &x, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
printf("%d ", x);
```
Hint: who is the root, and whose `buf3` is used?

**P19. Mini-exam, 30 minutes.**
Write Lab 3 Variant 1 (without bonus) from scratch with p processes: rank 0 reads `Numar1.txt`, `Numar2.txt` (first `N`, then digits, LSD first), distributes, workers add with carry chain, rank 0 writes `Numar3.txt`. Then answer: why does it work with `MPI_Ssend`? Compare with `04_lab_walkthrough.md`.

**P20. Mini-exam, 30 minutes.**
Write Lab 3 Variant 2 from scratch: Scatter/Gather, rank 0 also computes, padding, carry chain by `Send`/`Recv`.

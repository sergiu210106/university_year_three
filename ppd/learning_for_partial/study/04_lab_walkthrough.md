# Lab 3 walkthrough: exam-style solutions

Task: add two large numbers stored as digit arrays (index 0 = least significant digit). See `../course/02_lab_task.md`.
These solutions are written to be **reproducible on paper**: plain C-style arrays, no classes, short helpers.
All of them were checked against a big-integer reference (lengths 1..100000, p = 2..17, including the 16-digit
`9999 4444 4444 9999 + 9999 5555 5555 9999` case, all-nines carry chains and a `Send`->`Ssend` run) using a small thread-based
MPI simulator, because no MPI installation was available here. Compile with real MPI and re-run before trusting them for grading-level details.
Where the assignment's exact wording differs (e.g. `N/p` vs `N/(p-1)`), a note says so.

## 0. Ideas shared by every variant

**Input format:** a file starts with the digit count `N`, then `N` digits (whitespace separated), least significant first.
**Output:** count, then the sum digits (LSD first). The final carry may add one digit.

**Addition with carry**
```cpp
int add_digits(unsigned char *a, unsigned char *b, unsigned char *r, int n, int carry) {
    for (int i = 0; i < n; i++) { int s = a[i] + b[i] + carry; r[i] = s % 10; carry = s / 10; }
    return carry;                       // 0 or 1 (max 9+9+1 = 19)
}
```

**Different lengths:** `N = max(N1, N2)`, pad the shorter number with zeros (`read_digits` does it: digits at index >= its length are 0).

**Why it is hard to parallelise:** digit `i` needs the carry from digit `i-1`. Splitting into slices does not remove the dependency; slice `k` needs the carry out of slice `k-1`.
Parallelism comes from reading/sending/adding in different processes at the same time, and from tricks (V1.1: speculative addition).

**Chunk sizes** (`w` workers, slice `k = 0..w-1`):
```cpp
int cnt_of(int N, int w, int k) { return N / w + (k < N % w ? 1 : 0); }
int off_of(int N, int w, int k) { return k * (N / w) + (k < N % w ? k : N % w); }
```
(Simplest alternative on paper: every worker gets `N/w`, the last one gets `N - (w-1)*(N/w)`.)

**Tags:** use distinct tags (`A, B, CARRY, RES, FINAL`) so a worker can tell the two operand chunks, the carry and the result apart.

---

## 1. Variant 0: sequential
```c
#include <stdio.h>
#define MAXN 100100

int main(void) {
    static unsigned char a[MAXN], b[MAXN], r[MAXN];
    int n1, n2, N, i, x;
    FILE *f1 = fopen("Numar1.txt", "r"), *f2 = fopen("Numar2.txt", "r");
    fscanf(f1, "%d", &n1);
    fscanf(f2, "%d", &n2);
    N = n1 > n2 ? n1 : n2;
    for (i = 0; i < N; i++) {                      // zero padding of the shorter number
        x = 0; if (i < n1) fscanf(f1, "%d", &x); a[i] = x;
        x = 0; if (i < n2) fscanf(f2, "%d", &x); b[i] = x;
    }
    int carry = 0;
    for (i = 0; i < N; i++) {                      // LSD first: carry flows upward
        int s = a[i] + b[i] + carry;
        r[i] = s % 10;
        carry = s / 10;
    }
    if (carry) r[N++] = carry;
    FILE *out = fopen("Numar3.txt", "w");
    fprintf(out, "%d\n", N);
    for (i = 0; i < N; i++) fprintf(out, "%d ", r[i]);
    fclose(out);
    return 0;
}
```

---

## 2. Variant 1: blocking `Send`/`Recv`

Roles for `p` processes: rank 0 = master (I/O only, does **no** additions), ranks `1..p-1` = workers (`w = p-1`).
Each worker `k` gets slice `k-1`.

Steps:
1. Rank 0 reads the lengths, everybody learns `N` (`MPI_Bcast`).
2. Rank 0 loops over workers: read that worker's digits from both files, `Send` them.
3. Worker: `Recv` both chunks **first**, then (if rank > 1) `Recv` the carry from `rank-1`, add, `Send` carry to `rank+1` (if not last), `Send` the result slice to rank 0.
4. Rank 0 receives result slices in rank order and writes each one immediately. The last worker also sends its final carry; if it is 1 an extra digit is written, and the header (digit count) is rewritten.

```cpp
#include <mpi.h>
#include <stdio.h>
#define MAXN 100100
enum { A = 1, B, CARRY, RES, FINAL };

int cnt_of(int N, int w, int k) { return N / w + (k < N % w ? 1 : 0); }     // size of slice k
int off_of(int N, int w, int k) { return k * (N / w) + (k < N % w ? k : N % w); }

// read `cnt` digits of a number of declared length `len`, starting at digit `off`; zero padding
void read_digits(FILE *f, int len, int off, unsigned char *d, int cnt) {
    for (int i = 0; i < cnt; i++) {
        int x = 0;
        if (off + i < len) fscanf(f, "%d", &x);
        d[i] = x;
    }
}

int add_digits(unsigned char *a, unsigned char *b, unsigned char *r, int n, int carry) {
    for (int i = 0; i < n; i++) {
        int s = a[i] + b[i] + carry;
        r[i] = s % 10;
        carry = s / 10;
    }
    return carry;
}

int main(int argc, char *argv[]) {
    int rank, size, N = 0, n1 = 0, n2 = 0;
    FILE *f1 = NULL, *f2 = NULL;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    int workers = size - 1;

    if (rank == 0) {
        f1 = fopen("Numar1.txt", "r"); f2 = fopen("Numar2.txt", "r");
        fscanf(f1, "%d", &n1); fscanf(f2, "%d", &n2);
        N = n1 > n2 ? n1 : n2;
    }
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        unsigned char a[MAXN], b[MAXN], r[MAXN];
        for (int w = 1; w <= workers; w++) {                       // distribute while reading
            int cnt = cnt_of(N, workers, w - 1), off = off_of(N, workers, w - 1);
            read_digits(f1, n1, off, a, cnt);
            read_digits(f2, n2, off, b, cnt);
            MPI_Send(a, cnt, MPI_UNSIGNED_CHAR, w, A, MPI_COMM_WORLD);
            MPI_Send(b, cnt, MPI_UNSIGNED_CHAR, w, B, MPI_COMM_WORLD);
        }
        FILE *out = fopen("Numar3.txt", "w");
        fprintf(out, "%6d\n", N);                                 // header, rewritten at the end
        for (int w = 1; w <= workers; w++) {                       // write each segment on receipt
            int cnt = cnt_of(N, workers, w - 1);
            MPI_Recv(r, cnt, MPI_UNSIGNED_CHAR, w, RES, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for (int i = 0; i < cnt; i++) fprintf(out, "%d ", r[i]);
        }
        int carry;
        MPI_Recv(&carry, 1, MPI_INT, workers, FINAL, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (carry) fprintf(out, "1 ");
        fseek(out, 0, SEEK_SET);
        fprintf(out, "%6d", N + carry);
        fclose(out);
    } else {
        unsigned char a[MAXN], b[MAXN], r[MAXN];
        int cnt = cnt_of(N, workers, rank - 1), carry = 0;
        MPI_Recv(a, cnt, MPI_UNSIGNED_CHAR, 0, A, MPI_COMM_WORLD, MPI_STATUS_IGNORE);   // chunks first
        MPI_Recv(b, cnt, MPI_UNSIGNED_CHAR, 0, B, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (rank > 1)                                                                   // then the carry
            MPI_Recv(&carry, 1, MPI_INT, rank - 1, CARRY, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        carry = add_digits(a, b, r, cnt, carry);
        if (rank < workers)
            MPI_Send(&carry, 1, MPI_INT, rank + 1, CARRY, MPI_COMM_WORLD);              // carry first
        MPI_Send(r, cnt, MPI_UNSIGNED_CHAR, 0, RES, MPI_COMM_WORLD);                    // then result
        if (rank == workers)
            MPI_Send(&carry, 1, MPI_INT, 0, FINAL, MPI_COMM_WORLD);
    }
    MPI_Finalize();
    return 0;
}
```

**Why the order of operations matters (deadlock freedom with `MPI_Ssend`)**
- Assume every send waits until its receive starts.
- Rank 0 sends to worker 1, then 2, ... It never waits for results/carries while distributing.
- Worker `k` receives its chunks before the carry, so when worker `k-1` blocks in `Ssend(carry)` waiting for worker `k`, worker `k` is guaranteed to get to its carry receive once rank 0 has sent it the chunks, which rank 0 will do because it is not blocked on anyone else.
- The carry is sent **before** the result: a worker blocked in `Ssend(result)` waits for rank 0 to start collecting; rank 0 starts collecting only after distributing to everyone, and distribution never depends on a result. No cycle in the "waits for" graph.
- Receiving the carry before the chunks would not deadlock here but would **serialise** everything (rank 0's `Ssend` to worker `k` could not complete until worker `k-1` had finished), and the assignment requires chunks first.
- The dangerous variant is a master that **collects before it has distributed to everyone**: with `Ssend`, worker 1 blocks sending its carry to worker 2 (who has no chunks yet), rank 0 blocks waiting for worker 1's result, nobody can move: cycle. Rule: **rank 0 distributes everything first, collects afterwards**, and workers send the carry before the result.
- To test: replace every `MPI_Send` by `MPI_Ssend` (the lab uses `#ifdef SYNCHRONOUS_SEND`).

**Walk-through on the supplied example** (`9999 4444 4444 9999` + `9999 5555 5555 9999`, p = 5, w = 4, 4 digits per worker):
worker 1: 9999 + 9999 = 19998 -> digits `8 9 9 9` carry 1 -> worker 2 (digits `4444`+`5555` = 9999) with incoming 1 -> `0 0 0 0` carry 1 -> worker 3 same -> `0 0 0 0` carry 1 -> worker 4 (`9999`+`9999`+1 = 19999) -> `9 9 9 9` carry 1 -> final carry 1.
Output: `8 9 9 9 0 0 0 0 0 0 0 0 9 9 9 9 1`, count 17 (matches the repo's `supplied_seq.txt`).

---

## 3. Variant 1.1 (bonus): speculative addition

Question: can worker `id > 1` start adding before the carry from `id-1` arrives? **Yes**: add with incoming carry 0 immediately; when the real carry arrives, only fix the result if it is 1.

```cpp
// Variant 1.1: only the worker part differs from Variant 1.
int increment(unsigned char *r, int n) {          // add 1 to the slice, return overflow (0/1)
    for (int i = 0; i < n; i++) {
        if (r[i] < 9) { r[i]++; return 0; }
        r[i] = 0;                                  // 9 + 1 = 10 -> 0, carry continues
    }
    return 1;
}
// ... inside the worker branch:
        MPI_Recv(a, cnt, MPI_UNSIGNED_CHAR, 0, A, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(b, cnt, MPI_UNSIGNED_CHAR, 0, B, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        int out = add_digits(a, b, r, cnt, 0);     // speculative: assume incoming carry 0
        int in = 0;
        if (rank > 1)
            MPI_Recv(&in, 1, MPI_INT, rank - 1, CARRY, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (in) out |= increment(r, cnt);          // fix up only if the real carry was 1
        if (rank < workers) MPI_Send(&out, 1, MPI_INT, rank + 1, CARRY, MPI_COMM_WORLD);
        MPI_Send(r, cnt, MPI_UNSIGNED_CHAR, 0, RES, MPI_COMM_WORLD);
        if (rank == workers) MPI_Send(&out, 1, MPI_INT, 0, FINAL, MPI_COMM_WORLD);
```

Why `out |= increment(...)`: the speculative addition may already produce a carry out; the fix-up produces a carry out only if the slice was all nines and became all zeros. Both cannot happen together for a non-empty slice (the max of `a+b` for k digits is `2*10^k-2`; a carry out plus an all-9 result would need `2*10^k-1`), so OR is exact.

**Trade-offs**
- Benefit: the full-slice addition is done while waiting for the carry. The carry chain then costs one cheap fix-up per worker instead of a full slice addition per worker.
- Cost: an extra pass (the increment) when the carry is 1; worst case it scans the whole slice (all nines), expected O(1) for random digits.
- Real timings are close: reading files, message passing and scheduling dominate the cheap per-digit addition, so the gain is small (a few percent in the repo measurements and not consistently positive; at 17 processes it was slower).
- Needs a result buffer separate from the inputs only if you want to keep inputs; here the result slice is fixed in place.

---

## 4. Variant 2: `MPI_Scatter` / `MPI_Gather`

Rank 0 **also computes**. All ranks call the collectives. The carry still travels with point-to-point messages `i -> i+1`.

Steps: rank 0 reads and pads both numbers; `Bcast` N; two `Scatter`s; chain the carry; `Gather`; rank 0 writes.

Padding: `Scatter` needs equal pieces, so the arrays are padded with zeros up to `padded = per * p`.
The solution below uses `per = N / p + 1`, so `padded > N` **always**. That guarantees room for the final carry: the carry out of digit `N-1` is added to the zero pad digit `N` (0+0+carry) and ends up in `r[N]`, and no carry leaves the padded range. So the output length is `N + r[N]`.
(If you use `per = ceil(N/p)` and `padded == N`, the final carry is the one returned by the **last rank** and must be sent/broadcast to rank 0; if `padded > N`, the carry is in `r[N]`. Choosing `padded > N` avoids the case split.)

```cpp
#include <mpi.h>
#include <stdio.h>
#define MAXN 100100
enum { CARRY = 1 };

int add_digits(unsigned char *a, unsigned char *b, unsigned char *r, int n, int carry) {
    for (int i = 0; i < n; i++) {
        int s = a[i] + b[i] + carry;
        r[i] = s % 10;
        carry = s / 10;
    }
    return carry;
}

int main(int argc, char *argv[]) {
    int rank, size, N = 0;
    unsigned char a[MAXN], b[MAXN], r[MAXN];        // only used on rank 0
    unsigned char la[MAXN], lb[MAXN], lr[MAXN];     // local slices
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) {
        FILE *f1 = fopen("Numar1.txt", "r"), *f2 = fopen("Numar2.txt", "r");
        int n1, n2;
        fscanf(f1, "%d", &n1); fscanf(f2, "%d", &n2);
        N = n1 > n2 ? n1 : n2;
        int per = N / size + 1, padded = per * size;   // padded > N: always room for the final carry
        for (int i = 0; i < padded; i++) {             // zero padding (shorter number AND up to padded)
            int x = 0; if (i < n1) fscanf(f1, "%d", &x); a[i] = x;
            x = 0;     if (i < n2) fscanf(f2, "%d", &x); b[i] = x;
        }
    }
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    int per = N / size + 1;

    MPI_Scatter(a, per, MPI_UNSIGNED_CHAR, la, per, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
    MPI_Scatter(b, per, MPI_UNSIGNED_CHAR, lb, per, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    int carry = 0;
    if (rank > 0)
        MPI_Recv(&carry, 1, MPI_INT, rank - 1, CARRY, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    carry = add_digits(la, lb, lr, per, carry);
    if (rank < size - 1)
        MPI_Send(&carry, 1, MPI_INT, rank + 1, CARRY, MPI_COMM_WORLD);

    MPI_Gather(lr, per, MPI_UNSIGNED_CHAR, r, per, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        int len = N + r[N];                            // r[N] is the final carry (0 or 1)
        FILE *out = fopen("Numar3.txt", "w");
        fprintf(out, "%d\n", len);
        for (int i = 0; i < len; i++) fprintf(out, "%d ", r[i]);
        fclose(out);
    }
    MPI_Finalize();
    return 0;
}
```

Collective rules used: the two `Scatter`s and the `Gather` are executed by **all** ranks (outside any `if (rank == 0)`), same root, same counts; the `sendcount` is the amount **per process**.
Ranks other than 0 pass garbage/uninitialised `a`, `b`, `r` as send/receive buffers, which MPI ignores there.

---

## 5. Variant 3: non-blocking `Isend`/`Irecv`

Same roles as V1, but every transfer is non-blocking.
- Master: reads each worker's slice and `Isend`s it; posts `Irecv` for the result slices and the final carry; at the end `Waitall`s (sends first, then receives), writes the output.
- Worker: must use `Irecv` for the chunks **and** the carry: the carry from `id-1` may arrive before the chunks from rank 0, or after. It posts all receives, `Waitall`, computes, then `Isend`s carry and result and `Waitall`s the sends.

```cpp
#include <mpi.h>
#include <stdio.h>
#define MAXN 100100
#define MAXW 16
enum { A = 1, B, CARRY, RES, FINAL };

int cnt_of(int N, int w, int k) { return N / w + (k < N % w ? 1 : 0); }
int off_of(int N, int w, int k) { return k * (N / w) + (k < N % w ? k : N % w); }
void read_digits(FILE *f, int len, int off, unsigned char *d, int cnt) {
    for (int i = 0; i < cnt; i++) { int x = 0; if (off + i < len) fscanf(f, "%d", &x); d[i] = x; }
}
int add_digits(unsigned char *a, unsigned char *b, unsigned char *r, int n, int carry) {
    for (int i = 0; i < n; i++) { int s = a[i] + b[i] + carry; r[i] = s % 10; carry = s / 10; }
    return carry;
}

int main(int argc, char *argv[]) {
    int rank, size, N = 0, n1 = 0, n2 = 0;
    FILE *f1 = NULL, *f2 = NULL;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    int workers = size - 1;
    if (rank == 0) {
        f1 = fopen("Numar1.txt", "r"); f2 = fopen("Numar2.txt", "r");
        fscanf(f1, "%d", &n1); fscanf(f2, "%d", &n2);
        N = n1 > n2 ? n1 : n2;
    }
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        unsigned char a[MAXN], b[MAXN], r[MAXN];       // whole arrays: slices must stay untouched
        MPI_Request sends[2 * MAXW], recvs[MAXW + 1];
        int carry = 0;
        MPI_Irecv(&carry, 1, MPI_INT, workers, FINAL, MPI_COMM_WORLD, &recvs[workers]);
        for (int w = 1; w <= workers; w++) {
            int cnt = cnt_of(N, workers, w - 1), off = off_of(N, workers, w - 1);
            read_digits(f1, n1, off, a + off, cnt);
            read_digits(f2, n2, off, b + off, cnt);
            MPI_Irecv(r + off, cnt, MPI_UNSIGNED_CHAR, w, RES, MPI_COMM_WORLD, &recvs[w - 1]);
            MPI_Isend(a + off, cnt, MPI_UNSIGNED_CHAR, w, A, MPI_COMM_WORLD, &sends[2 * (w - 1)]);
            MPI_Isend(b + off, cnt, MPI_UNSIGNED_CHAR, w, B, MPI_COMM_WORLD, &sends[2 * (w - 1) + 1]);
        }
        MPI_Waitall(2 * workers, sends, MPI_STATUSES_IGNORE);
        MPI_Waitall(workers + 1, recvs, MPI_STATUSES_IGNORE);
        int len = N;
        if (carry) r[len++] = 1;
        FILE *out = fopen("Numar3.txt", "w");
        fprintf(out, "%d\n", len);
        for (int i = 0; i < len; i++) fprintf(out, "%d ", r[i]);
        fclose(out);
    } else {
        unsigned char a[MAXN], b[MAXN], r[MAXN];
        int cnt = cnt_of(N, workers, rank - 1), carry = 0;
        MPI_Request rq[3], sq[2];
        int nr = 2;
        MPI_Irecv(a, cnt, MPI_UNSIGNED_CHAR, 0, A, MPI_COMM_WORLD, &rq[0]);
        MPI_Irecv(b, cnt, MPI_UNSIGNED_CHAR, 0, B, MPI_COMM_WORLD, &rq[1]);
        if (rank > 1) {
            MPI_Irecv(&carry, 1, MPI_INT, rank - 1, CARRY, MPI_COMM_WORLD, &rq[2]);
            nr = 3;
        }
        MPI_Waitall(nr, rq, MPI_STATUSES_IGNORE);      // only now a, b, carry may be read
        carry = add_digits(a, b, r, cnt, carry);
        int dest = rank < workers ? rank + 1 : 0;      // last worker reports the final carry to rank 0
        int tag  = rank < workers ? CARRY : FINAL;
        MPI_Isend(&carry, 1, MPI_INT, dest, tag, MPI_COMM_WORLD, &sq[0]);
        MPI_Isend(r, cnt, MPI_UNSIGNED_CHAR, 0, RES, MPI_COMM_WORLD, &sq[1]);
        MPI_Waitall(2, sq, MPI_STATUSES_IGNORE);       // do not leave before the sends completed
    }
    MPI_Finalize();
    return 0;
}
```

Rules respected (examiners like to check them):
- The master reads into **disjoint slices** of whole arrays (`a + off`): no `Isend` buffer is modified while in flight, and every `Irecv` buffer is untouched until `Waitall`.
- One request per outstanding operation (`sends[2*MAXW]`, `recvs[MAXW+1]`, worker `rq[3]`, `sq[2]`).
- Carry buffer `&carry` is not changed between `Isend` and the `Waitall` that follows.
- Every request is completed before `MPI_Finalize`.
- Why not `MPI_Recv` in the worker? Receiving the chunks first with a blocking receive would stall if the carry arrived first, and the other way round; with `Irecv` both are posted and whichever arrives is stored.

Note on `N/p`: the assignment says each worker gets `N/p` digits, but with one I/O-only rank only `p-1` workers add; the solution divides by `w = p-1` so all digits are covered (as in the repo README). Mention this assumption if the exam wording differs.

---

## 6. Comparison table

| | V0 | V1 | V1.1 | V2 | V3 |
|---|---|---|---|---|---|
| Rank 0 adds? | yes (only process) | no | no | yes | no |
| Reads | all at once | chunk by chunk, streaming | same | all at once, padded | chunk by chunk |
| Distribution | none | `Send` | `Send` | `Scatter` | `Isend` |
| Carry | loop | `Send`/`Recv` | `Send`/`Recv` + fix-up | `Send`/`Recv` | `Isend`/`Irecv` |
| Collection | none | `Recv` + write each | same | `Gather` | `Irecv` + `Waitall` |
| Special | | order: chunks, carry; Ssend-safe | speculative add + increment | padding to multiple of p | requests, `Waitall`, buffer lifetimes |
| Min processes | 1 | 2 | 2 | 1 | 2 |

# Guide: solving the MPI exam models on paper

Solutions: [`solutions/1.cpp`](solutions/1.cpp) (AI group) and [`solutions/2.cpp`](solutions/2.cpp) (CS group).
Both were checked on many inputs, including n not divisible by p and n < p.

Conventions used everywhere:
- **Files** use `<fstream>`: `ifstream in("input.txt"); in >> n;` and `ofstream out("x.txt"); out << v << "\n";`
- **Arrays** are static: `int A[MAX];` with `const int MAX = 1000;`. No malloc, no new.
- **Distribution** uses only plain `MPI_Scatter` / `MPI_Gather` (no `v` variants).

Both models follow the same skeleton. Learn the skeleton once, then map each sentence of the statement onto one MPI call.

---

## 1. The universal skeleton

```
1. Frame          Init, rank, size ... Finalize
2. Read           if (rank == 0) { ifstream in("input.txt"); in >> n >> ...; }
3. Share sizes    MPI_Bcast(n)  (+ any parameter like w)
4. Distribute     MPI_Scatter  (+ padding or a 2nd Scatter if p does not divide n)
5. Local work     a plain for-loop over MY block, no MPI inside
6. Combine        MPI_Reduce / MPI_Gather / a Send-Recv ring
7. Write          if (rank == 0) { ofstream out("output.txt"); out << ...; }
```

**Before you write code**, label each sentence of the statement with the step it belongs to.
Model 2 is already split into stages, so this is easy. Model 1 is not, so do it yourself.

Frame with the includes you need:
```cpp
#include <mpi.h>
#include <fstream>
#include <iostream>      // only if you print with cout
using namespace std;

const int MAX = 1000, MAXP = 100;

int main(int argc, char *argv[]) {
    int rank, p;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &p);
    /* ... */
    MPI_Finalize();
    return 0;
}
```

---

## 2. Translating statement phrases into MPI calls

| The statement says... | Write... |
|---|---|
| "only process 0 reads / writes files" | `if (rank == 0) { ifstream in(...); ... }`. Everyone else gets data through MPI. |
| "p_0 broadcasts w" | `MPI_Bcast(&w, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);` |
| "assume p divides n" + "use MPI_Scatter" | `per = n / p; MPI_Scatter(A, per, ..., local, per, ..., 0, ...)` |
| "as equally as possible" | Scatter `n/p` to everyone, then a **2nd Scatter of 1 element** for the `n%p` leftovers (section 3.2) |
| "p_0 participates in computation" / "all processes participate" | Use a **collective** (Scatter). The root gets a block too. Do not write a master/worker split where rank 0 only distributes. |
| "in the original index order" | `MPI_Gather`. Blocks come back in rank order, which is the original order. |
| "compute the global sum" (no method given) | `MPI_Reduce(&local, &g, 1, MPI_DOUBLE, MPI_SUM, 0, ...)` |
| "using a cascade p0 -> p1 -> ... -> p0" | Hand-written **ring** with `MPI_Send` / `MPI_Recv` (section 3.3). Reduce is **not** accepted here. |
| "send X to p_1" | Collective with **root = 1** (`MPI_Gather(..., 1, ...)` or `MPI_Reduce(..., 1, ...)`) |
| "count how many times..." | Plain local `int` counters. Combine later with Gather/Reduce. |

---

## 3. Patterns you need (memorise these four)

### 3.1 Read, Bcast n, Scatter (p divides n)
```cpp
int n, A[MAX], local[MAX];
if (rank == 0) {
    ifstream in("input.txt");
    in >> n;
    for (int i = 0; i < n; i++) in >> A[i];
}
MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);       // everyone needs n to compute per
int per = n / p;
MPI_Scatter(A, per, MPI_INT, local, per, MPI_INT, 0, MPI_COMM_WORLD);
```
Rules that cost points:
- The count in Scatter/Gather is **per process**, not total.
- Collectives are called by **all** ranks, **outside** `if (rank == 0)`.
- `A` is declared on every rank (static arrays exist everywhere), but only rank 0's copy is read by Scatter.
- `Bcast n` must happen **before** anyone computes `per`.
- With static arrays, rank 0 can read `n` **and** all numbers in one go before the Bcast.

### 3.2 "As equally as possible" with plain Scatter (Model 2)
```cpp
int q = n / P, r = n % P;                  // everyone gets q; ranks 0..r-1 get 1 more
int cnt = q + (rank < r ? 1 : 0);          // my block size
MPI_Scatter(data, q, MPI_DOUBLE, local, q, MPI_DOUBLE, 0, MPI_COMM_WORLD);    // data[0..qP-1]

double extra[MAXP];
if (r > 0) {                               // r is the same on all ranks -> safe
    if (rank == 0) for (int i = 0; i < r; i++) extra[i] = data[q * P + i];   // leftovers
    MPI_Scatter(extra, 1, MPI_DOUBLE, &local[q], 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
}
/* ... work on local[0 .. cnt-1] ... */
MPI_Gather(local, q, MPI_DOUBLE, data, q, MPI_DOUBLE, 0, MPI_COMM_WORLD);     // back to 0..qP-1
if (r > 0) {
    MPI_Gather(&local[q], 1, MPI_DOUBLE, extra, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    if (rank == 0) for (int i = 0; i < r; i++) data[q * P + i] = extra[i];   // back to its index
}
```
- Sanity check: n = 10, P = 4 -> q = 2, r = 2 -> sizes 3,3,2,2 (differ by at most 1).
- The leftover of rank i came from index `q*P + i` and goes back to `q*P + i`, so the original order is kept.
- An `if` around a collective is fine **only** when the condition is the same on every rank (`r` is, `rank` is not).
- Loop to `cnt`, not to `q`, or the leftover is never processed.

### 3.3 Ring / cascade (p0 -> p1 -> ... -> p_{P-1} -> p0)
```cpp
if (rank == 0) {
    MPI_Send(&mine, 1, MPI_DOUBLE, 1, 0, MPI_COMM_WORLD);           // 0 SENDS first
    MPI_Recv(&total, 1, MPI_DOUBLE, P-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
} else {
    MPI_Recv(&acc, 1, MPI_DOUBLE, rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    acc += mine;
    MPI_Send(&acc, 1, MPI_DOUBLE, (rank+1) % P, 0, MPI_COMM_WORLD); // last one -> 0
}
```
- **Deadlock reasoning to write in a comment:** rank 0 sends before it receives, and all other ranks receive before they send. So the token always moves forward. This is safe even if Send behaves like Ssend.
- If every rank (rank 0 included) did `Recv` first, nobody would ever send: **deadlock**.
- The `(rank+1) % P` is what closes the ring back to 0.

### 3.4 Gathering results of different lengths with plain Gather (Model 1)
Each rank produced `cx` useful values (unknown in advance, between 0 and per).
Plain Gather needs the **same** count from everybody, so send the counts plus fixed-size blocks:
```cpp
int cntX[MAXP], allX[MAX];
MPI_Gather(&cx, 1,   MPI_INT, cntX, 1,   MPI_INT, 0, MPI_COMM_WORLD);
MPI_Gather(lx,  per, MPI_INT, allX, per, MPI_INT, 0, MPI_COMM_WORLD);
// rank 0: block r is allX[r*per ... r*per + cntX[r] - 1]; the rest is ignored
if (rank == 0)
    for (int r = 0; r < p; r++)
        for (int j = 0; j < cntX[r]; j++) out << allX[r * per + j] << "\n";
```

---

## 4. "What if p does not divide n?" (the 2-point question)

First **state what breaks**, then fix it. Graders want both.

> With `per = n / p` (integer division), `p * per < n`. The last `n % p` elements are never
> scattered, so they are missing from the result. Example: n = 10, p = 4 -> per = 2, only 8
> elements are processed.

Three fixes that only use plain Scatter, from least to most code:

| Fix | Idea | When it is valid |
|---|---|---|
| **A. Padding** | `per = (n + p - 1) / p`, set `A[n .. per*p-1] = 0`, then Scatter as before | Only if some value cannot affect the result. In Model 1, **0** is neither positive nor negative, so it never shows up in X or Y. For a sum, 0 is neutral. For a product, 1. For a min, a huge value. Array must hold `n + p - 1` elements. |
| **B. 2nd Scatter for leftovers** | Scatter `n/p`, then Scatter 1 element each from the `n%p` leftovers (section 3.2) | Always valid, and the blocks are "as equal as possible". Used in Model 2. |
| **C. Remainder on root** | Scatter `n/p` to everyone, then rank 0 also processes the last `n%p` elements itself | Valid, but rank 0 does more work, so it is *not* "as equally as possible". Order: those results come **last**. |

For Model 1, the solution uses **A**: two lines change. Say explicitly *why* 0 is safe.
For Model 2, padding is **wrong**: h would also transform the padding values, which changes A, B and the sum. Use **B**.

---

## 5. Walkthrough: Model 1 (positives / negatives)

1. Rank 0 reads `n` and all of A. `MPI_Bcast(n)`.
2. Everyone computes `per`. Rank 0 pads with 0 (V2 only). `MPI_Scatter(A -> local)`.
3. Local loop: `if (v > 0) lx[cx++] = v; else if (v < 0) ly[cy++] = v;`
   Left to right, so the order inside the block is kept. 0 goes nowhere.
4. Gather `cx`, `cy` (1 int each), then gather `lx`, `ly` (`per` ints each).
5. Rank 0: for each rank r in order, write the first `cntX[r]` values of block r to `outputX.txt` (same for Y).

**Why the order is preserved** (write this as a sentence on the exam): Scatter gives contiguous blocks in rank order. Gather returns blocks in rank order. Inside each block we keep the left-to-right order. So X and Y follow the order of A.

Traps:
- `local[i] >= 0` would put zeros into X. The statement says *positive*, so use `> 0`.
- Writing `lx` to a file on each rank is wrong: only rank 0 may touch files.
- `MPI_Gather(lx, cx, ...)` is **wrong**: `cx` differs per rank, and plain Gather needs the same count from all. Send `per`.

## 6. Walkthrough: Model 2 (function h, ring, two roots)

| Stage | What to write |
|---|---|
| 1 | rank 0 reads `w`, `n`, data. `Bcast(w)`, `Bcast(n)`. q, r, cnt. Scatter q + Scatter leftovers (3.2). |
| 2 | loop to `cnt`: `if (v < w) { v += w; A++; } else { v -= w; B++; } sum += v;` (add **after** transforming) |
| 3 | ring (3.3), rank 0 prints with `cout` |
| 4a | Gather q + Gather leftovers back into `data` on rank 0. Rank 0 writes `output.txt`. |
| 4b | `int pair[2] = {A, B}; MPI_Gather(pair, 2, MPI_INT, allPairs, 2, MPI_INT, 1, ...)`. Rank 1 sums the even and odd positions and prints. |

Traps:
- Broadcast **n** as well, even though the statement only says "broadcast w". Every rank needs n to compute q, r, cnt.
- Root = 1 for the (A, B) gather. Rank 1's `allPairs` is the one that gets filled.
- Note the assumption P >= 2 (p1 must exist, and the ring needs a neighbour).
- 4b alternative in one line: `MPI_Reduce(pair, tot, 2, MPI_INT, MPI_SUM, 1, MPI_COMM_WORLD);`
  This is fine to mention. Gather + loop matches the wording "p1 computes the total" most literally.

---

## 7. Exam-day procedure (about 45-60 min)

1. **(3 min) Map the statement.** Next to each sentence, write the MPI call it becomes (table in section 2).
2. **(2 min) Frame.** includes, `const int MAX`, `main`, Init, rank, size, Finalize, `return 0`.
3. **(10 min) Read + distribute.** Rank 0 reading with ifstream, Bcast, Scatter (+ padding / leftovers).
4. **(5 min) Local loop.** No MPI calls inside it.
5. **(10 min) Combine + write.** Gather / ring / reduce, then rank 0 writes with ofstream.
6. **(5 min) Comments.** One line per block explaining *why* (order preserved, deadlock-free, which root).
7. **(5 min) Checklist pass** (below).
8. If there is a "what if p does not divide n" part: write **what breaks, an example, the fix, and why the fix is correct**.

## 8. Final checklist

- [ ] `MPI_Init` first, `MPI_Finalize` last, on **every** path (including early returns).
- [ ] Every collective is called by all ranks in the same order with the same root. An `if` around it is OK only when its condition is the same on every rank.
- [ ] Scatter/Gather counts are **per process** and **the same** on every rank.
- [ ] Static arrays are big enough: `n` (+ padding up to `p - 1`), and `MAXP` for arrays indexed by rank.
- [ ] Each `Send` has a matching `Recv`: opposite ranks, same tag, same type, recv count >= send count.
- [ ] The ring has one rank that sends first, so there is no cycle of Recvs.
- [ ] MPI types match C++ types: `int` with `MPI_INT`, `double` with `MPI_DOUBLE`.
- [ ] Files are opened only on rank 0 (`ifstream` / `ofstream`).
- [ ] The "p does not divide n" case is handled or analysed if asked.

## 9. Likely variations (practise these by editing the solutions)

- Model 1, V2 with the leftover trick (3.2) instead of padding.
- Model 2 with the ring going the other way (0 -> P-1 -> ... -> 1 -> 0): send to `(rank - 1 + P) % P`, receive from `(rank + 1) % P`.
- Model 2 where the global sum is needed **on all ranks**: `MPI_Allreduce`, or have rank 0 Bcast it after the ring.
- Counts where the padding is not neutral (example: "how many v < w") mean padding is **not** allowed, so use the leftover trick.
- Replace the collectives with explicit `Send`/`Recv` loops from rank 0 (master/worker). See `study/03_templates.md` T1.

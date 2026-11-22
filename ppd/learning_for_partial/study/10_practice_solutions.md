# Practice solutions

Code is shown inside `main` after the usual frame (`MPI_Init`, `rank`, `size`, `MPI_Status st;`) unless stated.
There are often several correct answers; what matters is matching sends/receives, correct arguments and no deadlock.

---

## P1. Master/worker sum
```cpp
int workers = size - 1, N = 1000;
if (rank == 0) {
    int a[1000];
    for (int i = 0; i < N; i++) a[i] = i;
    int chunk = N / workers;
    for (int w = 1; w <= workers; w++) {
        int cnt = (w == workers) ? N - (w - 1) * chunk : chunk;   // last takes the remainder
        MPI_Send(&cnt, 1, MPI_INT, w, 0, MPI_COMM_WORLD);
        MPI_Send(a + (w - 1) * chunk, cnt, MPI_INT, w, 1, MPI_COMM_WORLD);
    }
    long total = 0;
    for (int w = 1; w <= workers; w++) {
        long part;
        MPI_Recv(&part, 1, MPI_LONG, w, 2, MPI_COMM_WORLD, &st);
        total += part;
    }
    printf("%ld\n", total);
} else {
    int cnt, mine[1000];
    MPI_Recv(&cnt, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &st);
    MPI_Recv(mine, cnt, MPI_INT, 0, 1, MPI_COMM_WORLD, &st);
    long part = 0;
    for (int i = 0; i < cnt; i++) part += mine[i];
    MPI_Send(&part, 1, MPI_LONG, 0, 2, MPI_COMM_WORLD);
}
```
Safe with `Ssend`: rank 0 distributes to everyone before it receives; workers receive first, send last.

## P2. Collectives
```cpp
int N = 1000, per, local = 0, total = 0, a[1000], mine[1000];
if (rank == 0) { for (int i = 0; i < N; i++) a[i] = i; }
MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);       // (only needed if N is not a constant everywhere)
per = N / size;
MPI_Scatter(a, per, MPI_INT, mine, per, MPI_INT, 0, MPI_COMM_WORLD);
for (int i = 0; i < per; i++) local += mine[i];
MPI_Reduce(&local, &total, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
if (rank == 0) printf("%d\n", total);
```

## P3. Dot product, result everywhere
```cpp
int per = N / size;
double lx[1000], ly[1000], local = 0.0, dot;
MPI_Scatter(x, per, MPI_DOUBLE, lx, per, MPI_DOUBLE, 0, MPI_COMM_WORLD);
MPI_Scatter(y, per, MPI_DOUBLE, ly, per, MPI_DOUBLE, 0, MPI_COMM_WORLD);
for (int i = 0; i < per; i++) local += lx[i] * ly[i];
MPI_Allreduce(&local, &dot, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);   // everyone gets dot
```
(`Reduce` + `Bcast` would also work; `Allreduce` is the single call.)

## P4. Prefix sum
Chain:
```cpp
int v = rank + 1, partial = 0;
if (rank > 0) MPI_Recv(&partial, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, &st);
partial += v;
if (rank < size - 1) MPI_Send(&partial, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
printf("rank %d: %d\n", rank, partial);
```
Collective: `MPI_Scan(&v, &partial, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);`

## P5. Scatterv / Gatherv
```cpp
int counts[4] = {3, 3, 2, 2}, displs[4] = {0, 3, 6, 8};
int mine[3], out[10];
MPI_Scatterv(a, counts, displs, MPI_INT, mine, counts[rank], MPI_INT, 0, MPI_COMM_WORLD);
/* ... work on mine[0..counts[rank]-1] ... */
MPI_Gatherv(mine, counts[rank], MPI_INT, out, counts, displs, MPI_INT, 0, MPI_COMM_WORLD);
```
General computation: `counts[i] = N/p + (i < N%p ? 1 : 0)`, `displs[i] = displs[i-1] + counts[i-1]`. `counts`/`displs` are only used on the root for the send side but every rank may compute them.

## P6. Count occurrences, N arbitrary
```cpp
int N, target, local = 0, total;
int counts[MAXP], displs[MAXP], a[MAXN], mine[MAXN];
if (rank == 0) { /* read N, target, a[0..N-1] */ }
MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
MPI_Bcast(&target, 1, MPI_INT, 0, MPI_COMM_WORLD);
for (int i = 0; i < size; i++) {
    counts[i] = N / size + (i < N % size ? 1 : 0);
    displs[i] = (i == 0) ? 0 : displs[i-1] + counts[i-1];
}
MPI_Scatterv(a, counts, displs, MPI_INT, mine, counts[rank], MPI_INT, 0, MPI_COMM_WORLD);
for (int i = 0; i < counts[rank]; i++) if (mine[i] == target) local++;
MPI_Reduce(&local, &total, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
if (rank == 0) printf("%d\n", total);
```
With padding instead: `padded = ((N+size-1)/size)*size`, fill `a[N..padded-1]` with a sentinel that never equals `target`, `per = padded/size`, `Scatter`.

## P7. Ring
(a) Not safe. With `Ssend` every rank blocks in the send waiting for its right neighbour to receive, but that neighbour is blocked in its own send: a cycle of 4. Works with `MPI_Send` only if the implementation buffers.
Fixes:
```cpp
// 1) Sendrecv
MPI_Sendrecv(&x, 1, MPI_INT, right, 0, &y, 1, MPI_INT, left, 0, MPI_COMM_WORLD, &st);

// 2) non-blocking
MPI_Request rq;
MPI_Irecv(&y, 1, MPI_INT, left, 0, MPI_COMM_WORLD, &rq);
MPI_Send (&x, 1, MPI_INT, right, 0, MPI_COMM_WORLD);
MPI_Wait(&rq, &st);

// 3) parity ordering (p even)
if (rank % 2 == 0) { MPI_Send(&x,1,MPI_INT,right,0,MPI_COMM_WORLD); MPI_Recv(&y,1,MPI_INT,left,0,MPI_COMM_WORLD,&st); }
else               { MPI_Recv(&y,1,MPI_INT,left,0,MPI_COMM_WORLD,&st); MPI_Send(&x,1,MPI_INT,right,0,MPI_COMM_WORLD); }
```
(Another fix: `MPI_Bsend` with an attached buffer.)

## P8. Non-blocking squares
```cpp
MPI_Request sreq[MAXP], rreq[MAXP];
int out[MAXP], in[MAXP];
if (rank == 0) {
    for (int w = 1; w < size; w++) {
        out[w] = w + 10;                                                   // distinct values
        MPI_Irecv(&in[w], 1, MPI_INT, w, 2, MPI_COMM_WORLD, &rreq[w]);     // post receives early
        MPI_Isend(&out[w], 1, MPI_INT, w, 1, MPI_COMM_WORLD, &sreq[w]);
    }
    MPI_Waitall(size - 1, sreq + 1, MPI_STATUSES_IGNORE);
    MPI_Waitall(size - 1, rreq + 1, MPI_STATUSES_IGNORE);
    for (int w = 1; w < size; w++) printf("%d ", in[w]);
} else {
    int x, sq;
    MPI_Request r1, r2;
    MPI_Irecv(&x, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &r1);
    MPI_Wait(&r1, MPI_STATUS_IGNORE);               // x valid only after this
    sq = x * x;
    MPI_Isend(&sq, 1, MPI_INT, 0, 2, MPI_COMM_WORLD, &r2);
    MPI_Wait(&r2, MPI_STATUS_IGNORE);               // sq must stay unchanged until here
}
```
Distinct elements of `out[]`/`in[]` and distinct requests; `sreq + 1` passes the sub-array starting at index 1.

## P9. Big number times a digit
(a) `9*9 + carry` with carry <= 8 gives at most 89, so carry = `s / 10` <= **8**. (c) Because each digit product can be up to 81, the carry can be 0..8, so it must be sent as an `int` (and the final carry becomes one extra most-significant digit 1..8, or 0).
(b) Worker (after `Bcast` of `k` and `N`; rank 0 distributes slices of `A` as in V1):
```cpp
int cnt = cnt_of(N, workers, rank - 1), carry = 0;
unsigned char a[MAXN], r[MAXN];
MPI_Recv(a, cnt, MPI_UNSIGNED_CHAR, 0, 1, MPI_COMM_WORLD, &st);            // slice first
if (rank > 1) MPI_Recv(&carry, 1, MPI_INT, rank - 1, 2, MPI_COMM_WORLD, &st);   // then carry
for (int i = 0; i < cnt; i++) {
    int s = a[i] * k + carry;
    r[i] = s % 10;
    carry = s / 10;
}
if (rank < workers) MPI_Send(&carry, 1, MPI_INT, rank + 1, 2, MPI_COMM_WORLD);
MPI_Send(r, cnt, MPI_UNSIGNED_CHAR, 0, 3, MPI_COMM_WORLD);
if (rank == workers) MPI_Send(&carry, 1, MPI_INT, 0, 4, MPI_COMM_WORLD);   // final carry to rank 0
```
Rank 0 writes the slices in order, then receives the final carry (tag 4) and appends it as a digit if non-zero.

## P10. Padding
(a) `ceil(1000/16) = 63` digits each, padded array `63*16 = 1008` (8 extra zeros). (b) After the carry has passed through digit 999, it is added to the zero at index 1000: it ends up in `r[1000]`; the carry out of the padded range is 0. So output length = `1000 + r[1000]`. (If padded were exactly `N`, the final carry would be what the **last rank** sends/broadcasts.)
(c) `N/p = 62` (integer division) scatters only `62*16 = 992` digits: the last 8 digits are never distributed and the sum is wrong.

## P11. Matrix-vector product
```cpp
int rows = R / size;
double M[...], v[C], lm[rows_max * C], lr[rows_max], res[R];     // M and res meaningful on rank 0
MPI_Bcast(v, C, MPI_DOUBLE, 0, MPI_COMM_WORLD);
MPI_Scatter(M, rows * C, MPI_DOUBLE, lm, rows * C, MPI_DOUBLE, 0, MPI_COMM_WORLD);   // count per process = rows*C
for (int i = 0; i < rows; i++) {
    lr[i] = 0;
    for (int j = 0; j < C; j++) lr[i] += lm[i * C + j] * v[j];
}
MPI_Gather(lr, rows, MPI_DOUBLE, res, rows, MPI_DOUBLE, 0, MPI_COMM_WORLD);
```

## P12. Bugs
1. `MPI_Comm_rank`/`MPI_Comm_size` called **before** `MPI_Init`.
2. `MPI_Bcast` inside `if (rank == 0)`: collectives must be called by all ranks.
3. `n * sizeof(int)` with `MPI_INT`: `count` is in elements, should be `n`.
4. Receive capacity 50 < 100 sent: truncation error.
5. Tag mismatch: send tag 0, receive tag 1 (never matches: receiver hangs).
6. `MPI_Recv` is missing the `status` argument (`&st` or `MPI_STATUS_IGNORE`); `st` and `req` are never declared.
7. `MPI_Isend` is never completed with `Wait`; `n = rank` modifies the send buffer while the send is outstanding; then `Recv` writes into the same `n`.
8. `data` is never initialised on rank 0, so garbage is sent (minor).
9. `MPI_Finalize` missing at the end.

## P13. Ping-pong
```cpp
double *buf = (double*)malloc(m * sizeof(double));
int R = 100;
MPI_Barrier(MPI_COMM_WORLD);
double t0 = MPI_Wtime();
for (int i = 0; i < R; i++) {
    if (rank == 0) {
        MPI_Send(buf, m, MPI_DOUBLE, 1, 0, MPI_COMM_WORLD);
        MPI_Recv(buf, m, MPI_DOUBLE, 1, 0, MPI_COMM_WORLD, &st);
    } else if (rank == 1) {
        MPI_Recv(buf, m, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, &st);
        MPI_Send(buf, m, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
    }
}
double t1 = MPI_Wtime();
if (rank == 0) printf("one-way time: %g s\n", (t1 - t0) / (2.0 * R));
```
Safe: opposite order on the two sides (rank 0 send-then-recv, rank 1 recv-then-send), so it works with `Ssend` too.

## P14. Probe
```cpp
if (rank == 1) {
    for (int i = 0; i < 3; i++) {
        MPI_Status s; int n;
        MPI_Probe(0, i, MPI_COMM_WORLD, &s);            // wait for message with tag i, do not receive yet
        MPI_Get_count(&s, MPI_CHAR, &n);                // its length
        char *b = (char*)malloc(n);
        MPI_Recv(b, n, MPI_CHAR, 0, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("%s\n", b);
        free(b);
    }
}
```
Function needed: `MPI_Probe` (+ `MPI_Get_count`).

## P15. V1 modifications
(a) Still correct (same data flow) and deadlock-free (no cycle), but the carry `Recv` before the chunk `Recv` makes worker `k` wait for worker `k-1`'s carry before it accepts its chunks. With `Ssend` rank 0's send to worker `k` cannot complete until worker `k-1` is done, so distribution and addition are serialised: slower, and it violates the required order.
(b) With `Ssend`, `p >= 3`: rank 0 is in `Recv(result from worker 1)`. Worker 1 (not last) is in `Ssend(carry -> worker 2)` (it sends the carry before the result), which needs worker 2 to post the carry `Recv`. Worker 2 is still in its first `Recv(chunk from rank 0)`, waiting for rank 0, which never sends because it waits for worker 1. Graph: `0 -> 1 -> 2 -> 0`, a cycle: deadlock. Fix: distribute to everyone first, then collect.
(c) The speculative sum computes `a+b` with carry 0, then adds the incoming carry 0/1 by incrementing. Since `a + b + c = (a + b) + c`, the final digits and outgoing carry are identical (outgoing = speculative carry OR increment overflow; both cannot be 1 at the same time).

## P16. Which function?
(a) `MPI_Allreduce(&x, &g, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD)` (b) `MPI_Gather` (c) `MPI_Bcast(..., 2, ...)` (d) `MPI_Scatter` (e) `MPI_Barrier` (f) `MPI_Iprobe` (g) `MPI_Waitany` (h) `MPI_Sendrecv`.

## P17. V3 worker
```cpp
unsigned char a[MAXN], b[MAXN], r[MAXN];
int cnt = cnt_of(N, workers, rank - 1), carry = 0, nr = 2;
MPI_Request rq[3], sq[2];
MPI_Irecv(a, cnt, MPI_UNSIGNED_CHAR, 0, 1, MPI_COMM_WORLD, &rq[0]);
MPI_Irecv(b, cnt, MPI_UNSIGNED_CHAR, 0, 2, MPI_COMM_WORLD, &rq[1]);
if (rank > 1) { MPI_Irecv(&carry, 1, MPI_INT, rank - 1, 3, MPI_COMM_WORLD, &rq[2]); nr = 3; }
MPI_Waitall(nr, rq, MPI_STATUSES_IGNORE);
carry = add_digits(a, b, r, cnt, carry);
int dest = (rank < workers) ? rank + 1 : 0, tag = (rank < workers) ? 3 : 5;
MPI_Isend(&carry, 1, MPI_INT, dest, tag, MPI_COMM_WORLD, &sq[0]);
MPI_Isend(r, cnt, MPI_UNSIGNED_CHAR, 0, 4, MPI_COMM_WORLD, &sq[1]);
MPI_Waitall(2, sq, MPI_STATUSES_IGNORE);
```
Rules: `a`, `b`, `carry` must not be read until the `Waitall` on `rq` returns (and `carry` must not be written by the program in between, since the receive writes into it); `carry` and `r` must not be modified and must stay in scope until the `Waitall` on `sq` returns. The number given to `Waitall` equals the number of requests actually posted (`nr`).

## P18. Scatter with root 1
The root is rank 1, so rank 1's `buf3 = {7,8,9}` is used: rank 0 gets 7, rank 1 gets 8, rank 2 gets 9 (in `y`). `MPI_Allreduce` sums `y` into `x` on every rank: 24. Every rank prints `24` (three lines). The initial `x = rank + 1` is overwritten.

## P19 / P20
See `04_lab_walkthrough.md`, sections 2 and 4 (V1 and V2). Grade yourself on: Init/Finalize present, `N` known by everyone, chunk arithmetic correct (remainder not lost), chunks before carry, carry before result, last worker's carry delivered to rank 0 (V1), root distributes before it collects, padding (V2), collectives called by all ranks, and a correct argument for `Ssend` safety.

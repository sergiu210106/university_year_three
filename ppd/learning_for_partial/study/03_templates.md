# Paper-writing templates

Short enough to write by hand in a couple of minutes each. Practise: read once, cover, write, compare.
All use `MPI_COMM_WORLD` shortened to `W` in comments only; **write the full name on paper**.

## T0. Frame (every answer starts here)
```cpp
#include <mpi.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) {
        /* master */
    } else {
        /* workers */
    }

    MPI_Finalize();
    return 0;
}
```

## T1. Master sends chunks, workers return results (blocking)
```cpp
int workers = size - 1;
if (rank == 0) {
    for (int w = 1; w <= workers; w++) {
        /* read/prepare chunk for worker w into buf, cnt elements */
        MPI_Send(buf, cnt, MPI_INT, w, 1, MPI_COMM_WORLD);
    }
    for (int w = 1; w <= workers; w++) {            // collect in order
        MPI_Recv(res, cnt, MPI_INT, w, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        /* write res */
    }
} else {
    MPI_Recv(buf, cnt, MPI_INT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    /* compute res from buf */
    MPI_Send(res, cnt, MPI_INT, 0, 2, MPI_COMM_WORLD);
}
```
Check: tags equal on both sides, ranks opposite, same count/type.

## T2. Carry chain between workers (blocking)
```cpp
int carry = 0;
if (rank > 1)                                       // receive chunks FIRST (T1), then the carry
    MPI_Recv(&carry, 1, MPI_INT, rank - 1, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
carry = add_digits(a, b, r, cnt, carry);
if (rank < workers)
    MPI_Send(&carry, 1, MPI_INT, rank + 1, 3, MPI_COMM_WORLD);
MPI_Send(r, cnt, MPI_UNSIGNED_CHAR, 0, 2, MPI_COMM_WORLD);   // results to master
```

## T3. Scatter - compute - Gather
```cpp
int per = padded / size;
MPI_Scatter(a, per, MPI_UNSIGNED_CHAR, la, per, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
MPI_Scatter(b, per, MPI_UNSIGNED_CHAR, lb, per, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
/* local compute -> lr */
MPI_Gather(lr, per, MPI_UNSIGNED_CHAR, r, per, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
```
All ranks execute these lines (not inside `if (rank == 0)`). `a`, `b`, `r` only need to be valid on rank 0.

## T4. Bcast a value, reduce a result
```cpp
if (rank == 0) n = 100;
MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
/* local work -> local */
MPI_Reduce(&local, &total, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
if (rank == 0) printf("%d\n", total);
```

## T5. Non-blocking worker (receive data + carry in any order)
```cpp
MPI_Request rq[3]; int nr = 2;
MPI_Irecv(a, cnt, MPI_UNSIGNED_CHAR, 0, 1, MPI_COMM_WORLD, &rq[0]);
MPI_Irecv(b, cnt, MPI_UNSIGNED_CHAR, 0, 2, MPI_COMM_WORLD, &rq[1]);
if (rank > 1) { MPI_Irecv(&carry, 1, MPI_INT, rank-1, 3, MPI_COMM_WORLD, &rq[2]); nr = 3; }
MPI_Waitall(nr, rq, MPI_STATUSES_IGNORE);
/* compute */
MPI_Request sq[2];
MPI_Isend(&carry, 1, MPI_INT, rank+1, 3, MPI_COMM_WORLD, &sq[0]);
MPI_Isend(r, cnt, MPI_UNSIGNED_CHAR, 0, 2, MPI_COMM_WORLD, &sq[1]);
MPI_Waitall(2, sq, MPI_STATUSES_IGNORE);
```

## T6. Non-blocking master
```cpp
MPI_Request sends[2 * MAXW], recvs[MAXW];
for (int w = 1; w <= workers; w++) {
    MPI_Isend(a + off, cnt, MPI_UNSIGNED_CHAR, w, 1, MPI_COMM_WORLD, &sends[2*(w-1)]);
    MPI_Isend(b + off, cnt, MPI_UNSIGNED_CHAR, w, 2, MPI_COMM_WORLD, &sends[2*(w-1)+1]);
    MPI_Irecv(r + off, cnt, MPI_UNSIGNED_CHAR, w, 4, MPI_COMM_WORLD, &recvs[w-1]);
}
MPI_Waitall(2*workers, sends, MPI_STATUSES_IGNORE);
MPI_Waitall(workers,   recvs, MPI_STATUSES_IGNORE);
```
Different `off` per worker -> disjoint slices of one array: allowed because nothing is modified while sends are in flight.

## T7. Safe pairwise exchange
```cpp
MPI_Sendrecv(&x, 1, MPI_INT, other, 0,  &y, 1, MPI_INT, other, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
```
or ordered: even rank `Send` then `Recv`, odd rank `Recv` then `Send`.

## T8. Timing
```cpp
MPI_Barrier(MPI_COMM_WORLD);
double t0 = MPI_Wtime();
/* work (include file reading if asked) */
MPI_Barrier(MPI_COMM_WORLD);
double ms = (MPI_Wtime() - t0) * 1000.0;
```

## T9. Probe a message of unknown length
```cpp
MPI_Status st; int n;
MPI_Probe(0, 5, MPI_COMM_WORLD, &st);
MPI_Get_count(&st, MPI_INT, &n);
int *p = (int*)malloc(n * sizeof(int));
MPI_Recv(p, n, MPI_INT, 0, 5, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
```

## Checklist before handing in
- Init first, Finalize last, on every path.
- Each Send has exactly one matching Recv (rank, tag, count <= capacity, type).
- Collectives are called by all ranks, in the same order, same root.
- Receive order in each process cannot create a cycle (also when Send = Ssend).
- Non-blocking: buffers untouched until Wait; all requests waited; request arrays big enough.

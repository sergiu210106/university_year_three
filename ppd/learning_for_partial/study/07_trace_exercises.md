# Trace-the-output exercises

Method: for each rank, write the local variables after each call. Do it on paper, then check the solutions at the bottom.
All programs have the usual frame (`Init`, `Comm_rank`, `Comm_size`, `Finalize`), `c` stands for `MPI_COMM_WORLD`.

---

## E1. Bcast + Reduce (p = 4)
```cpp
int n = 0;
if (rank == 0) n = 5;
MPI_Bcast(&n, 1, MPI_INT, 0, c);
int local = rank + n;
int sum = -1, mx = -1;
MPI_Reduce(&local, &sum, 1, MPI_INT, MPI_SUM, 2, c);
MPI_Reduce(&local, &mx,  1, MPI_INT, MPI_MAX, 0, c);
printf("rank %d: n=%d local=%d sum=%d mx=%d\n", rank, n, local, sum, mx);
```
What does each rank print (ignore the line order)?

## E2. Scatter + Gather (p = 3)
```cpp
int a[6] = {1,2,3,4,5,6}, mine[2], out[6] = {0,0,0,0,0,0};
MPI_Scatter(a, 2, MPI_INT, mine, 2, MPI_INT, 0, c);
for (int i = 0; i < 2; i++) mine[i] = mine[i] * 2 + rank;
MPI_Gather(mine, 2, MPI_INT, out, 2, MPI_INT, 1, c);
if (rank == 1) for (int i = 0; i < 6; i++) printf("%d ", out[i]);
```
What is printed, and by whom? What do `out` on ranks 0 and 2 contain?

## E3. Carry chain (p = 4, 3 workers, N = 6, 2 digits each, LSD first)
`a = {9,9,5,1,2,3}`, `b = {1,0,4,8,7,6}`. Worker `k` (rank `k`, slice `k-1`) does:
```cpp
if (rank > 1) MPI_Recv(&carry, 1, MPI_INT, rank-1, 3, c, &st);   // carry = 0 initially
carry = add_digits(la, lb, lr, 2, carry);
if (rank < 3) MPI_Send(&carry, 1, MPI_INT, rank+1, 3, c);
MPI_Send(lr, 2, MPI_UNSIGNED_CHAR, 0, 2, c);
if (rank == 3) MPI_Send(&carry, 1, MPI_INT, 0, 5, c);
```
Give for every worker the incoming carry, result digits and outgoing carry, then the final output (with the final carry). Write the two numbers in normal notation and check the sum.

## E4. Ring shift with Sendrecv (p = 4)
```cpp
int x = rank * rank, y = -1;
int right = (rank + 1) % size, left = (rank + size - 1) % size;
MPI_Sendrecv(&x, 1, MPI_INT, right, 0, &y, 1, MPI_INT, left, 0, c, &st);
printf("rank %d got %d from %d\n", rank, y, st.MPI_SOURCE);
```

## E5. Tag matching (p = 2), standard `MPI_Send` with small messages (buffered)
```cpp
if (rank == 0) {
    int x = 1, y = 2;
    MPI_Send(&x, 1, MPI_INT, 1, 1, c);
    MPI_Send(&y, 1, MPI_INT, 1, 2, c);
} else if (rank == 1) {
    int a, b;
    MPI_Recv(&a, 1, MPI_INT, 0, 2, c, &st);
    MPI_Recv(&b, 1, MPI_INT, 0, 1, c, &st);
    printf("a=%d b=%d\n", a, b);
}
```
(a) What is printed? (b) What happens if both `MPI_Send` are `MPI_Ssend`?

## E6. Wildcards (p = 3)
```cpp
if (rank != 0) {
    int v = rank * 100;
    MPI_Send(&v, 1, MPI_INT, 0, rank, c);
} else {
    int v;
    for (int i = 0; i < 2; i++) {
        MPI_Recv(&v, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, c, &st);
        printf("got %d from %d (tag %d)\n", v, st.MPI_SOURCE, st.MPI_TAG);
    }
}
```
Which outputs are possible?

## E7. Non-overtaking and different tags (p = 2)
```cpp
if (rank == 0) {
    int v[3] = {10, 20, 30};
    for (int i = 0; i < 3; i++) MPI_Send(&v[i], 1, MPI_INT, 1, 0, c);
} else {
    int w;
    for (int i = 0; i < 3; i++) { MPI_Recv(&w, 1, MPI_INT, 0, 0, c, &st); printf("%d ", w); }
}
```
What is printed? Is it guaranteed? What if rank 1 received from `MPI_ANY_SOURCE` and rank 2 also sent to rank 1?

## E8. Reduce with count 2 (p = 3)
```cpp
int v[2] = { rank, 10 * rank };
int s[2] = { 0, 0 }, m[2] = { 0, 0 };
MPI_Reduce(v, s, 2, MPI_INT, MPI_SUM, 0, c);
MPI_Reduce(v, m, 2, MPI_INT, MPI_MAX, 0, c);
if (rank == 0) printf("%d %d | %d %d\n", s[0], s[1], m[0], m[1]);
```

## E9. Pi (lecture 5) by hand: n = 4, p = 2
```cpp
h = 1.0 / n; sum = 0.0;
for (i = myid + 1; i <= n; i += numprocs) { x = h * ((double)i - 0.5); sum += 4.0 / (1.0 + x*x); }
mypi = h * sum;
MPI_Reduce(&mypi, &pi, 1, MPI_DOUBLE, MPI_SUM, 0, c);
```
Which `i` does each rank handle? What are `mypi` on each rank and `pi` on rank 0 (3 decimals)?

## E10. Non-blocking, what is printed? (p = 2)
```cpp
int x, y = 0;
MPI_Request rq;
if (rank == 0) {
    x = 7;
    MPI_Isend(&x, 1, MPI_INT, 1, 0, c, &rq);
    MPI_Wait(&rq, MPI_STATUS_IGNORE);
    x = 8;                                  // reuse after completion
    MPI_Send(&x, 1, MPI_INT, 1, 1, c);
} else {
    MPI_Irecv(&y, 1, MPI_INT, 0, 0, c, &rq);
    MPI_Wait(&rq, MPI_STATUS_IGNORE);
    int z;
    MPI_Recv(&z, 1, MPI_INT, 0, 1, c, MPI_STATUS_IGNORE);
    printf("y=%d z=%d\n", y, z);
}
```

## E11. Which line is wrong? (p = 2, two doubles)
```cpp
double s[2] = {1.5, 2.5}, r[2];
if (rank == 0) MPI_Send(s, 2, MPI_INT, 1, 0, c);
else           MPI_Recv(r, 2, MPI_DOUBLE, 0, 0, c, &st);
```

## E12. Probe (p = 2)
```cpp
if (rank == 0) {
    char m[] = "Hello";                         // 6 bytes with the terminator
    MPI_Send(m, 6, MPI_CHAR, 1, 9, c);
} else {
    MPI_Status st; int n;
    MPI_Probe(0, 9, c, &st);
    MPI_Get_count(&st, MPI_CHAR, &n);
    char *b = (char*)malloc(n);
    MPI_Recv(b, n, MPI_CHAR, 0, 9, c, MPI_STATUS_IGNORE);
    printf("%d %s\n", n, b);
}
```

---

# Solutions

**E1.** After the Bcast `n = 5` everywhere. `local` = 5, 6, 7, 8 for ranks 0..3. `sum` (SUM) is valid only on rank 2: 26. `mx` (MAX) valid only on rank 0: 8. The other ranks keep their initial `-1`.
```text
rank 0: n=5 local=5 sum=-1 mx=8
rank 1: n=5 local=6 sum=-1 mx=-1
rank 2: n=5 local=7 sum=26 mx=-1
rank 3: n=5 local=8 sum=-1 mx=-1
```

**E2.** Scatter: rank 0 gets {1,2}, rank 1 {3,4}, rank 2 {5,6} (root takes part). After the loop: rank 0: {2,4}; rank 1: {3*2+1, 4*2+1} = {7,9}; rank 2: {5*2+2, 6*2+2} = {12,14}. Gather to rank 1 in rank order: `out = {2,4,7,9,12,14}`. Rank 1 prints `2 4 7 9 12 14`. On ranks 0 and 2, `out` is unchanged (all zeros) and they print nothing.

**E3.** Digits: `a = 321599`, `b = 678401` (reading LSD first), sum = 1000000.
- Worker 1 (`9,9`+`1,0`): in 0; 9+1=10 -> 0 c1; 9+0+1=10 -> 0 c1. Result `0 0`, out 1.
- Worker 2 (`5,1`+`4,8`): in 1; 5+4+1=10 -> 0 c1; 1+8+1=10 -> 0 c1. Result `0 0`, out 1.
- Worker 3 (`2,3`+`7,6`): in 1; 2+7+1=10 -> 0 c1; 3+6+1=10 -> 0 c1. Result `0 0`, out 1 (sent to rank 0, tag 5).
Output file: `7` then `0 0 0 0 0 0 1` (count 7, LSD first) = 1000000.

**E4.** Values `x` = 0, 1, 4, 9. Rank `r` receives from `left = (r-1) mod 4`: rank 0 gets 9 from 3, rank 1 gets 0 from 0, rank 2 gets 1 from 1, rank 3 gets 4 from 2. No deadlock (`Sendrecv`).

**E5.** (a) With buffering: matching is by tag, not by order, so `a=2 b=1`. (b) With `Ssend`: rank 0's first `Ssend` (tag 1) completes only when a receive with tag 1 starts, but rank 1 first waits for tag 2 -> deadlock. This program is therefore *unsafe*.

**E6.** Two lines, in either order: `got 100 from 1 (tag 1)` and `got 200 from 2 (tag 2)`. Both orders are possible; arrival order from different senders is not defined.

**E7.** `10 20 30`. Guaranteed: same sender, same receiver, same communicator: messages do not overtake. With a second sender (rank 2) and `MPI_ANY_SOURCE`, the interleaving between the two senders is undefined (each sender's own messages stay in order).

**E8.** `s = {0+1+2, 0+10+20} = {3, 30}`, `m = {2, 20}`. Rank 0 prints `3 30 | 2 20`. Reduce works element-wise on `count` elements.

**E9.** Rank 0: `i = 1, 3` (x = 0.125, 0.625); rank 1: `i = 2, 4` (x = 0.375, 0.875). Terms `4/(1+x^2)`: 3.938, 3.507, 2.876, 2.266. Rank 0 sum = 3.938 + 2.876 = 6.815, `mypi = 0.25 * 6.815 = 1.704`. Rank 1 sum = 3.507 + 2.266 = 5.772, `mypi = 1.443`. `pi` on rank 0 = 3.147 (approximation with n = 4; true value 3.14159).

**E10.** Rank 1 prints `y=7 z=8`. The reuse of `x` happens after `Wait`, which is allowed. (Modifying `x` before the `Wait` would be an error.) Rank 0's blocking `Send` (tag 1) matches rank 1's blocking `Recv`.

**E11.** The send uses `MPI_INT` for a `double` array; the types must match (`MPI_DOUBLE` on both sides). Result: garbage/undefined (and the byte counts differ).

**E12.** Prints `6 Hello`. `Probe` blocks until the message is available and does not consume it; `Get_count` reports 6 chars; the `Recv` then consumes it.

# Deadlock, safety and how to analyse them

## 1. Definitions (lecture 5 wording)

- **Deadlock:** a set of processes each waiting for an event that only another process in the set can cause, so none can proceed.
- **Safe program:** does **not rely on system buffering** to complete. Portable rule: *every* `MPI_Send` may be replaced by `MPI_Ssend` and the program must still finish.
- **Unsafe program:** works only because the implementation happens to buffer small messages (works with small data, hangs with big data or on another MPI).
- **Fairness:** MPI does not guarantee it. If tasks 0 and 1 both send messages that match task 2's single receive, only one send completes; preventing starvation is the programmer's job.
- **Completion:** `Recv` completes when the data is in the buffer. `Send` completes when the buffer is reusable (standard: may be before the receiver got it; synchronous: after the receive started; buffered: after copying to the attached buffer).

Buffering: the implementation may be *non-local* (send waits for the receiver, data goes straight to the receive buffer) or *local* (data copied to a system/user buffer, send returns before the receive is posted). Which one `MPI_Send` uses is decided by the implementation and usually depends on message size.

## 2. Reading code for deadlock: the wait-for graph method

1. Pretend **every** `Send` is `Ssend` (the worst case).
2. For each process, list its operations in program order.
3. Simulate step by step: a process in a `Send` waits until the target is **inside the matching `Recv`**; a process in a `Recv` waits until a matching send is **posted** by the sender.
4. Draw an arrow "A waits for B". A cycle (or a wait for an operation that is never executed) means deadlock.
5. Also check: wrong tag/rank/communicator (message never matches), count too small (truncation error), missing participant in a collective, `MPI_Finalize` reached with pending requests.

## 3. Patterns

### 3.1 Receive-before-send on both sides: ALWAYS deadlocks
```cpp
// rank 0                                         // rank 1
MPI_Recv(&y,1,MPI_INT,1,20,c,&st);                MPI_Recv(&y,1,MPI_INT,0,10,c,&st);
MPI_Send(&x,1,MPI_INT,1,10,c);                    MPI_Send(&x,1,MPI_INT,0,20,c);
```
Both are blocked in `Recv`; the sends are never reached. Buffering cannot help. (Lecture 5, "Deadlock example".)

### 3.2 Send-before-receive on both sides: UNSAFE (buffering-dependent)
```cpp
// rank 0                                         // rank 1
MPI_Send(&x,1,MPI_INT,1,10,c);                    MPI_Send(&x,1,MPI_INT,0,20,c);
MPI_Recv(&y,1,MPI_INT,1,20,c,&st);                MPI_Recv(&y,1,MPI_INT,0,10,c,&st);
```
Works if at least one send buffers and returns; deadlocks if neither does (always with `Ssend`).

### 3.3 Safe: order the operations
```cpp
// rank 0                                         // rank 1
MPI_Send(&x,1,MPI_INT,1,10,c);                    MPI_Recv(&y,1,MPI_INT,0,10,c,&st);
MPI_Recv(&y,1,MPI_INT,1,20,c,&st);                MPI_Send(&x,1,MPI_INT,0,20,c);
```
Arrows: rank 0 waits for rank 1's `Recv` (which is its first operation, so it is reached), then rank 1 sends, rank 0 receives. No cycle.

### 3.4 Safe: `MPI_Sendrecv`
```cpp
MPI_Sendrecv(&x,1,MPI_INT,other,10, &y,1,MPI_INT,other,10, c, &st);
```
(Use the same tag on both sides, or send tag of one = receive tag of the other.) Needs two different buffers.

### 3.5 Safe: post the receive first (non-blocking)
```cpp
MPI_Irecv(&y,1,MPI_INT,other,10,c,&rq);
MPI_Send (&x,1,MPI_INT,other,10,c);
MPI_Wait(&rq,&st);
```
Both sides can send because the receive is already posted.

### 3.6 Safe: buffered send
`MPI_Buffer_attach` + `MPI_Bsend` returns immediately; if the attached buffer is too small you get a buffer-overflow **error** instead of a deadlock.

### 3.7 Circular deadlock (ring)
Every rank does `Send(right)` then `Recv(left)`. With synchronous sends every process waits for its right neighbour, which is itself in `Send`: a cycle of `p` processes.
Fixes: even ranks send first / odd ranks receive first (needs even `p`, or special-case the last rank), `MPI_Sendrecv`, or `Irecv` first + `Waitall`.
```cpp
int right = (rank + 1) % size, left = (rank - 1 + size) % size;
if (rank % 2 == 0) { MPI_Send(&x,1,MPI_INT,right,0,c); MPI_Recv(&y,1,MPI_INT,left,0,c,&st); }
else               { MPI_Recv(&y,1,MPI_INT,left,0,c,&st); MPI_Send(&x,1,MPI_INT,right,0,c); }
```
With odd `p` the ranks `p-1` and `0` are neighbours and both are even, so both send first. It still completes (rank 0's send goes to odd rank 1, which receives first, and only then does rank 0 receive from `p-1`), but the exchange is partly serialised; `MPI_Sendrecv` avoids the whole question.

### 3.8 Collective + point-to-point mix (lecture 4, last slide)
```cpp
if (rank == 1) MPI_Recv(&v,1,MPI_INT,0,0,c,&st);
MPI_Bcast(&b,1,MPI_INT,0,c);
if (rank == 0) MPI_Send(&v,1,MPI_INT,1,0,c);
```
Rank 1 blocks in `Recv` (waits for rank 0's send), rank 0 blocks in `Bcast` (waits for rank 1 to enter). Deadlock. In most libraries collectives synchronise, so never assume a collective returns before the others arrive.

### 3.9 Collective not called by all ranks
```cpp
if (rank == 0) MPI_Bcast(&n,1,MPI_INT,0,c);   // WRONG: others never call it -> hang or garbage
```
Correct: **all** ranks call `MPI_Bcast` with the same root.

### 3.10 Different order of collectives
Rank 0: `Bcast(a)` then `Bcast(b)`; rank 1: `Bcast(b)` then `Bcast(a)` -> mismatch, undefined/hang. All ranks must call collectives in the same order.

### 3.11 Message never matches
Sender tag 1, receiver tag 2; or sender sends to rank 3 but rank 3 receives from rank 2; or sender sends 10 ints, receiver's capacity 5 (truncation error); or the receive is missing altogether. With a blocking receive the receiver hangs.

### 3.12 Non-blocking pitfalls (not deadlocks, but equally fatal)
- Reading an `Irecv` buffer before `Wait` -> garbage.
- Changing an `Isend` buffer before `Wait` -> corrupted/unspecified message.
- Forgetting to `Wait` -> request leak, possibly lost data at `Finalize`.
- `Wait` for a message nobody sends -> deadlock moved to the `Wait`.
- Reusing the same `MPI_Request` variable for two outstanding operations -> the first can never be completed.

## 4. Why Lab V1 is safe (the standard argument to give on an exam)

(Code: `04_lab_walkthrough.md`, section 2.)

1. Assume all sends are synchronous.
2. Rank 0 only **sends** during distribution; it waits for nothing from workers until all chunks are delivered. Each of its `Ssend`s is matched by a worker's chunk `Recv`, which is the worker's **first** operation.
3. Worker `k` can block only in: (a) chunk `Recv` from 0 (will be satisfied), (b) carry `Recv` from `k-1` (will be satisfied as soon as worker `k-1` finishes adding, which needs only its chunks and its own carry), (c) carry `Ssend` to `k+1` (satisfied when `k+1` reaches its carry `Recv`, after it received its chunks, which rank 0 sends without waiting for anyone), (d) result `Ssend` to rank 0 (satisfied when rank 0 starts collecting, after distribution, which never depends on results).
4. The dependencies form a chain 0 -> 1 -> 2 -> ... -> w, never a cycle. Hence no deadlock.
5. The carry is sent **before** the result so a worker waiting to hand its result to rank 0 can never prevent the next worker from getting its carry.

Variant 3 argument: all receives are posted up front with `Irecv`, so no send waits for a receive that has not been posted; the `Wait` calls wait on operations whose matching counterpart is always eventually posted.

## 5. "Is this safe?" exercises (answers at the end)

**Q1.**
```cpp
if (rank == 0) { MPI_Ssend(&a,1,MPI_INT,1,0,c); MPI_Recv(&b,1,MPI_INT,1,0,c,&st); }
if (rank == 1) { MPI_Ssend(&a,1,MPI_INT,0,0,c); MPI_Recv(&b,1,MPI_INT,0,0,c,&st); }
```
**Q2.**
```cpp
if (rank == 0) { MPI_Isend(&a,1,MPI_INT,1,0,c,&r1); MPI_Recv(&b,1,MPI_INT,1,0,c,&st); MPI_Wait(&r1,&st); }
if (rank == 1) { MPI_Isend(&a,1,MPI_INT,0,0,c,&r1); MPI_Recv(&b,1,MPI_INT,0,0,c,&st); MPI_Wait(&r1,&st); }
```
**Q3.** `p = 4`; each rank `i` does `MPI_Recv` from `(i+3)%4`, then `MPI_Send` to `(i+1)%4`.

**Q4.**
```cpp
// all ranks
MPI_Bcast(&n,1,MPI_INT,0,c);
if (rank != 0) MPI_Send(&n,1,MPI_INT,0,0,c);
if (rank == 0) for (int i = 1; i < size; i++) MPI_Recv(&m,1,MPI_INT,i,0,c,&st);
```
**Q5.** Rank 0 does `MPI_Recv(buf,5,MPI_INT,1,0,c,&st)`; rank 1 does `MPI_Send(buf,10,MPI_INT,0,0,c)`.

**Q6.** Rank 0: `MPI_Recv(&x,1,MPI_INT,MPI_ANY_SOURCE,0,c,&st)` executed twice. Rank 1 and rank 2 each send one int with tag 0 to rank 0.

**Q7.** Lab V1 modified: worker receives the carry first, then the chunks. Safe with `Ssend`? What is the performance consequence?

**Q8.**
```cpp
// p = 2
MPI_Irecv(&b,1,MPI_INT,other,0,c,&rq);
MPI_Wait(&rq,&st);
MPI_Send(&a,1,MPI_INT,other,0,c);
```
(same code on both ranks)

### Answers
- **Q1:** Deadlock. Both in `Ssend`, nobody receives. Unsafe even with `Send` (buffer-dependent).
- **Q2:** Safe. `Isend` returns at once, the blocking `Recv`s then match the other side's `Isend`, `Wait` completes.
- **Q3:** Deadlock: everyone is in `Recv` first, nobody sends. (Always, regardless of buffering.)
- **Q4:** Safe. The `Bcast` is called by all; then each non-root sends once and rank 0 receives from 1, 2, ... in order. Even with `Ssend`: rank `i` blocks until rank 0 reaches the receive for `i`, which happens in order.
- **Q5:** Error: message truncation (10 elements into a buffer of capacity 5). Receiver count must be >= sent count.
- **Q6:** Safe; `MPI_ANY_SOURCE` takes whichever arrives first. Output order of `st.MPI_SOURCE` is non-deterministic (so a program must not assume 1 before 2).
- **Q7:** Safe (no cycle) but the chain becomes serial: rank 0's send to worker `k` cannot complete (synchronous) until worker `k-1` has finished and sent its carry, so reading/distribution no longer overlaps with computation. It also violates the assignment's required order.
- **Q8:** Deadlock. Both ranks wait for the receive to complete before sending. Move the `Send` before the `Wait`.

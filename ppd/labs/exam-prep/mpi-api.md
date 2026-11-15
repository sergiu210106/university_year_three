# MPI API reference for the exam

Everything here is C/C++ MPI. Functions are grouped by what you use them for.
Items marked **(used in 03-assignment)** appear in the lab code; learn those first.

---

## 0. The mental model

- You launch **p processes** (`mpirun -np p ./prog`). Each runs the **same program**.
- Processes do **not share memory**. They cooperate only by sending messages.
- Each process has a **rank** (its id, `0 … p-1`). Code branches on the rank
  (`if (rank == 0) { master } else { worker }`).
- All processes belong to a **communicator**. `MPI_COMM_WORLD` = everyone.
- Every call returns an `int` error code (`MPI_SUCCESS` on success). By default an
  error aborts the whole program, so exam code usually ignores the return value.

Minimal program:

```cpp
#include <mpi.h>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);   // who am I?
    MPI_Comm_size(MPI_COMM_WORLD, &size);   // how many of us?

    // ... work, branching on rank ...

    MPI_Finalize();
    return 0;
}
```

---

## 1. Setup and teardown

### `MPI_Init(int* argc, char*** argv)` (used)
Starts MPI. Must be called **before any other MPI call**, once per process.
Pass `&argc, &argv`.

### `MPI_Finalize()` (used)
Shuts MPI down. Call last. No MPI calls after it. Every process must call it.

### `MPI_Comm_rank(MPI_Comm comm, int* rank)` (used)
Writes the calling process's id in `comm` into `*rank`.

### `MPI_Comm_size(MPI_Comm comm, int* size)` (used)
Writes the number of processes in `comm` into `*size`.

### `MPI_Abort(MPI_Comm comm, int errorcode)` (used)
Kills **all** processes in `comm`. Use for unrecoverable errors (e.g. file not found).
Plain `exit()` in one process can leave the others hanging.

### `double MPI_Wtime()` (used)
Wall-clock time in seconds (as a `double`). Time a region with
`t0 = MPI_Wtime(); ...; t1 = MPI_Wtime();` then `(t1 - t0)`.

---

## 2. The arguments you keep seeing

| Argument | Meaning |
|---|---|
| `buf` | Pointer to the data (send: data to send; receive: where to put it) |
| `count` | Number of **elements** (not bytes) |
| `datatype` | Type of each element, an MPI constant (table below) |
| `dest` / `source` | Rank of the other process |
| `tag` | Integer label on the message (used to tell messages apart) |
| `comm` | Communicator, almost always `MPI_COMM_WORLD` |
| `status` | Receive only: info about what actually arrived. Pass `MPI_STATUS_IGNORE` if you don't need it |
| `request` | Non-blocking only: a handle you later wait on |

### Datatypes

| MPI constant | C/C++ type |
|---|---|
| `MPI_CHAR` | `char` |
| `MPI_UNSIGNED_CHAR` | `unsigned char` (the digit arrays in the lab) |
| `MPI_INT` | `int` |
| `MPI_UNSIGNED` | `unsigned int` |
| `MPI_LONG` | `long` |
| `MPI_LONG_LONG` | `long long` |
| `MPI_FLOAT` | `float` |
| `MPI_DOUBLE` | `double` |

The datatype must **match** the C type on both sides. `count` is in elements:
sending 100 `unsigned char` digits is `count = 100, MPI_UNSIGNED_CHAR`.

### Wildcards (receive side)
- `MPI_ANY_SOURCE`: accept from any sender.
- `MPI_ANY_TAG`: accept any tag.
- After the receive, `status.MPI_SOURCE` and `status.MPI_TAG` tell you who/what it was.

### Message matching
A receive matches a send when **communicator, source, and tag** all match (wildcards
aside). Between one sender and one receiver, messages **do not overtake** each other
(they arrive in send order for the same comm/tag).

---

## 3. Blocking point-to-point

### `MPI_Send(buf, count, datatype, dest, tag, comm)` (used)
```cpp
MPI_Send(chunk, n, MPI_UNSIGNED_CHAR, /*dest*/ 1, /*tag*/ 0, MPI_COMM_WORLD);
```
**Blocking:** returns when `buf` is safe to **reuse/modify**. That does *not* mean the
receiver got it.
- Small message: MPI may copy it into an internal buffer and return immediately
  (**buffered**).
- Large message: may wait until the matching receive starts (**rendezvous**).
- Which one happens is implementation-dependent. **Never rely on buffering.**
  Correct code must work even if `MPI_Send` behaves like `MPI_Ssend`.

### `MPI_Ssend(buf, count, datatype, dest, tag, comm)` (used)
**Synchronous send.** Same arguments as `MPI_Send`, but completes only after the
receiver has **started receiving**. No buffering ambiguity. Used in the lab to
*test* that the code is deadlock-free (swap `Send` for `Ssend`; if it hangs, it was
relying on buffering).

### `MPI_Recv(buf, count, datatype, source, tag, comm, status)` (used)
```cpp
MPI_Recv(chunk, n, MPI_UNSIGNED_CHAR, /*src*/ 0, /*tag*/ 0,
         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
```
**Blocking:** returns only when the message has fully arrived in `buf`.
- `count` is the **capacity** of `buf`. The message may be shorter, but longer is
  an error (truncation).
- To find the real length: `MPI_Get_count(&status, datatype, &n)`.

### Other send modes (know the names)
| Call | Behaviour |
|---|---|
| `MPI_Send` | Standard: MPI decides whether to buffer |
| `MPI_Ssend` | Synchronous: waits for the receive to start |
| `MPI_Bsend` | Buffered: copies into a buffer *you* attach (`MPI_Buffer_attach`); always returns quickly |
| `MPI_Rsend` | Ready: only legal if the receive is already posted |

### `MPI_Sendrecv(sendbuf, scount, stype, dest, stag, recvbuf, rcount, rtype, source, rtag, comm, status)`
Does a send and a receive in one call, safely. Handy to **swap** data between two
processes without deadlock (two plain blocking sends would deadlock if unbuffered).

### `MPI_Probe(source, tag, comm, status)` / `MPI_Get_count(status, datatype, count)`
`Probe` blocks until a matching message is *available* without receiving it, so you
can inspect its size with `Get_count` and allocate the right buffer first.

---

## 4. Non-blocking point-to-point

The `I` means "immediate": the call **returns at once** and the transfer happens in
the background. You get an `MPI_Request` and must **complete** it later.

### `MPI_Isend(buf, count, datatype, dest, tag, comm, MPI_Request* request)` (used)
### `MPI_Irecv(buf, count, datatype, source, tag, comm, MPI_Request* request)` (used)
Same as `Send`/`Recv` but no `status` argument and a `request` out-parameter.
```cpp
MPI_Request req;
MPI_Irecv(buf, n, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, &req);
// ... do other useful work, don't touch buf ...
MPI_Wait(&req, MPI_STATUS_IGNORE);   // now buf is valid
```

**Golden rules**
1. Don't **read** an `Irecv` buffer until the request has completed.
2. Don't **modify** (or free / let go out of scope) an `Isend` buffer until it has completed.
3. Every request must eventually be completed with `Wait`/`Test`.
4. Non-blocking calls themselves **can't deadlock** (they return immediately); the
   deadlock risk moves to the `Wait` calls.

### `MPI_Wait(MPI_Request* request, MPI_Status* status)`
Blocks until that one operation completes.

### `MPI_Waitall(int count, MPI_Request requests[], MPI_Status statuses[])` (used)
Blocks until **all** listed requests complete. Pass `MPI_STATUSES_IGNORE` if unused.

### `MPI_Waitany(count, requests, int* index, status)`
Blocks until **any one** completes; `*index` says which. This is the tool for
"messages may arrive in either order" (like carry vs. digits in Variant 3).

### `MPI_Test(request, int* flag, status)`
**Non-blocking check**: sets `*flag` to true if complete, never waits. Use in a
polling loop while doing other work.

### Why V3 uses `Irecv`
A worker waits for two messages (digits from rank 0, carry from rank id-1) that can
arrive in either order. A blocking `Recv` for the wrong one first would stall while
the other is already waiting. Post both `Irecv`s, then `Waitall` (or `Waitany`).

---

## 5. Collective communication

A collective is called by **every process in the communicator**, with matching
arguments. They are all blocking. They imply cooperation, so no tags are needed.

### `MPI_Barrier(MPI_Comm comm)` (used)
Nobody passes until everyone has arrived. Used to line up timing measurements.

### `MPI_Bcast(buf, count, datatype, root, comm)` (used)
`root` sends the same `buf` to **everyone**. On root `buf` is input; on the others
it's output. All processes call it with the same `root`.
```cpp
int n;
if (rank == 0) n = 1000;
MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);   // now all ranks have n
```

### `MPI_Scatter(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm)` (used)
Root splits `sendbuf` into `p` equal pieces; piece `i` goes to rank `i` (**including
root itself**).
- `sendcount` = elements sent **to each** process (not the total!).
- `sendbuf`, `sendcount`, `sendtype` are only meaningful on root.
- Needs equal-sized pieces → why V2 pads `N` up to a multiple of `p`.
```cpp
// root has a[N]; each rank gets N/p digits in mine[]
MPI_Scatter(a, N/p, MPI_UNSIGNED_CHAR,
            mine, N/p, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
```

### `MPI_Gather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm)` (used)
Inverse of scatter: every rank sends `sendcount` elements; root concatenates them
**in rank order** into `recvbuf`. `recvcount` = elements received **from each**
process. `recvbuf` only matters on root.
```cpp
MPI_Gather(mine, N/p, MPI_UNSIGNED_CHAR,
           result, N/p, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
```

### `MPI_Reduce(sendbuf, recvbuf, count, datatype, op, root, comm)` (used)
Combines each rank's value(s) with an operation; the result lands on **root only**.
Ops: `MPI_SUM`, `MPI_MAX`, `MPI_MIN`, `MPI_PROD`, `MPI_LAND`, `MPI_LOR`, …
```cpp
double local = /* my time */, worst;
MPI_Reduce(&local, &worst, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
```
- `MPI_Allreduce(sendbuf, recvbuf, count, type, op, comm)`: same but **everyone** gets
  the result (no root).

### Variants worth recognising
| Call | What it does |
|---|---|
| `MPI_Allgather` | Gather, but every rank receives the full result |
| `MPI_Scatterv` / `MPI_Gatherv` | Uneven pieces: extra arrays `counts[]` and `displs[]` (offsets) per rank |
| `MPI_Alltoall` | Every rank sends a distinct piece to every other rank |

### Collective rules
- All ranks must call the **same collectives in the same order**, or the program hangs.
- Don't put a collective inside `if (rank == 0)`.
- Root's `recvbuf` (gather) / `sendbuf` (scatter) must be large enough for all `p` pieces.

---

## 6. Deadlock

**Deadlock** = processes wait forever for events that can never happen.

### Classic example
```cpp
// both ranks:
MPI_Send(out, n, MPI_INT, other, 0, MPI_COMM_WORLD);
MPI_Recv(in,  n, MPI_INT, other, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
```
If `Send` doesn't buffer, both block in `Send` waiting for a receive nobody has
reached. It may *work* for small messages (buffered) and hang for big ones.

### Fixes
1. **Order the calls:** one side sends first, the other receives first (e.g. even
   ranks send then recv, odd ranks recv then send).
2. `MPI_Sendrecv`.
3. Non-blocking `Isend`/`Irecv` + `Waitall`.

### Why the lab's V1 is safe
Rank 0 sends chunks to workers in order; each worker first `Recv`s its chunk (from
0), **then** `Recv`s the carry from `id-1`. The dependency chain is acyclic:
rank 1 needs only rank 0; rank 2 needs rank 0 and 1; and so on, so each wait is
satisfied by someone that is not itself waiting on you. Swapping the receive order
(carry before chunk) can cycle and hang under `Ssend`.

**How to check by hand:** assume every `Send` is `Ssend`. Draw "A waits for B" edges
(a process in `Send` waits for the receiver to reach the matching `Recv`; a process
in `Recv` waits for the sender). A **cycle** means deadlock.

---

## 7. Cheat sheet

```cpp
MPI_Init(&argc, &argv);
MPI_Comm_rank(MPI_COMM_WORLD, &rank);
MPI_Comm_size(MPI_COMM_WORLD, &size);

MPI_Send (buf, n, TYPE, dest,   tag, MPI_COMM_WORLD);
MPI_Ssend(buf, n, TYPE, dest,   tag, MPI_COMM_WORLD);
MPI_Recv (buf, n, TYPE, source, tag, MPI_COMM_WORLD, &status);

MPI_Isend(buf, n, TYPE, dest,   tag, MPI_COMM_WORLD, &req);
MPI_Irecv(buf, n, TYPE, source, tag, MPI_COMM_WORLD, &req);
MPI_Wait(&req, &status);          MPI_Waitall(k, reqs, statuses);

MPI_Bcast  (buf, n, TYPE, root, comm);
MPI_Scatter(sbuf, n_each, TYPE, rbuf, n_each, TYPE, root, comm);
MPI_Gather (sbuf, n_each, TYPE, rbuf, n_each, TYPE, root, comm);
MPI_Reduce (sbuf, rbuf, n, TYPE, OP, root, comm);
MPI_Barrier(comm);

MPI_Finalize();
```

### Self-test (cover the page and answer)
1. What does `count` count: bytes or elements?
2. Does `MPI_Send` returning mean the receiver got the data?
3. What's the difference between `MPI_Send` and `MPI_Ssend`?
4. In `MPI_Scatter`, is `sendcount` the total or per-process? Does root get a piece?
5. Why must all ranks call a collective?
6. When may you read the buffer of an `MPI_Irecv`? When may you reuse an `MPI_Isend` buffer?
7. Why does V3 need `MPI_Irecv` rather than `MPI_Recv`?
8. Two ranks each do `Send` then `Recv` to each other. When does it deadlock? Give two fixes.
9. Who ends up with the result of `MPI_Reduce`? Of `MPI_Allreduce`?
10. What is `MPI_STATUS_IGNORE` for?

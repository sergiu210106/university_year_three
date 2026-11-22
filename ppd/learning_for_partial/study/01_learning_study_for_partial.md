# MPI partial exam: main study guide

Written exam, similar to Lab 3 (MPI large-number addition). You will have to **write MPI code on paper**, so
everything here is about knowing the functions precisely (argument order!) and being able to combine them.

## Map of the material

| File | What it is | Use it for |
|---|---|---|
| `../course/01_lectures.md` | Lectures 4 + 5 converted to markdown | Source of truth for what the professor taught |
| `../course/02_lab_task.md` | The Lab 3 task | The exam will look like this |
| **this file** | Function reference + examples + combinations + how to study | Learn every function |
| `02_cheat_sheet.md` | One page of signatures | Memorise the argument order |
| `03_templates.md` | Skeletons to write from memory | Paper practice |
| `04_lab_walkthrough.md` | Exam-style solutions to Lab 3 (V0, V1, V1.1, V2, V3) | The most likely exam content |
| `05_deadlock_and_safety.md` | Unsafe/safe patterns, how to analyse | "Is this code safe?" questions |
| `06_theory_qa.md` | Short-answer theory | Theory questions |
| `07_trace_exercises.md` | Predict output / values | Reading code |
| `08_common_mistakes.md` | Bugs people make on paper | Last check on your own answers |
| `09_practice_exercises.md` + `10_practice_solutions.md` | ~15 exam-style problems, answers kept separate | Self-testing |

Reading order = the file numbers: `01` this guide -> `02` cheat sheet -> `03` templates -> `04` lab walkthrough -> `05` deadlock & safety -> `06` theory Q&A -> `07` trace exercises -> `08` common mistakes -> `09` practice exercises (check with `10` solutions). Course material to consult: `../course/01_lectures.md`, `../course/02_lab_task.md`.

---

## 1. Mental model (what MPI is)

- MPI = a **library specification** (not a language, not one implementation). Implementations: MPICH, OpenMPI, MS-MPI, Intel MPI.
- You start **p processes** that all run the **same program** (**SPMD**). `mpirun -np 4 ./prog`.
- Processes have **separate address spaces**: no shared variables. Data moves only through messages (or collectives).
- Each process has a **rank** (0..p-1) inside a **communicator**. `MPI_COMM_WORLD` = all processes.
  Code branches on rank: `if (rank == 0) { master } else { worker }`.
- Point-to-point is **cooperative**: a Send in one process needs a matching Receive in another.
- Message passing is **non-deterministic**: the arrival order of messages from different senders is undefined;
  you must enforce any order you need (via tags, source selection, ordering of receives).
- Compile/run: `mpicc prog.c -o prog` (C), `mpic++ prog.cpp -o prog` (C++), `mpirun -np 4 ./prog` or `mpiexec -n 4 ./prog`.
- Every MPI function returns an `int` error code (`MPI_SUCCESS` = 0). By default an error aborts all processes, so on paper you usually ignore the return value.

Minimal program (this is the frame of every answer):

```cpp
#include <mpi.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    printf("Process %d of %d\n", rank, size);

    MPI_Finalize();
    return 0;
}
```

---

## 2. Arguments that appear everywhere

| Argument | Meaning |
|---|---|
| `buf` | Pointer to the first element (send: data to send; recv: where to store). For a single variable pass `&x`; for an array pass the array name. |
| `count` | Number of **elements** (not bytes). On a receive it is the **capacity**: receiving fewer is OK, more is an error. |
| `datatype` | MPI type constant of each element; must match the C type. |
| `dest` / `source` | Rank (in `comm`) of the other side. |
| `tag` | User integer label to tell messages apart (>= 0). |
| `comm` | Communicator, almost always `MPI_COMM_WORLD`. |
| `status` | Receive only: `MPI_Status*`; holds actual source, tag, size. `MPI_STATUS_IGNORE` if not needed. |
| `request` | Non-blocking only: `MPI_Request*`, handle to complete later. |
| `root` | Collectives: the rank that is the source (Bcast/Scatter) or destination (Gather/Reduce). |

A message is described by the triple **(address, count, datatype)**.

### Datatypes

| MPI constant | C type |
|---|---|
| `MPI_CHAR` | `char` |
| `MPI_SIGNED_CHAR` | `signed char` |
| `MPI_UNSIGNED_CHAR` | `unsigned char` (digit arrays in the lab) |
| `MPI_SHORT` / `MPI_UNSIGNED_SHORT` | `short` / `unsigned short` |
| `MPI_INT` / `MPI_UNSIGNED` | `int` / `unsigned int` |
| `MPI_LONG` / `MPI_UNSIGNED_LONG` | `long` / `unsigned long` |
| `MPI_LONG_LONG` (`MPI_LONG_LONG_INT`) / `MPI_UNSIGNED_LONG_LONG` | `long long` / `unsigned long long` |
| `MPI_FLOAT` | `float` |
| `MPI_DOUBLE` | `double` |
| `MPI_LONG_DOUBLE` | `long double` |

Why datatypes exist: heterogeneous machines (different representations) and fewer memory copies for strided/structured data.
Custom types (contiguous, vector/strided, indexed, struct) exist (`MPI_Type_contiguous`, `MPI_Type_vector`, `MPI_Type_create_struct`, `MPI_Type_commit`) but are not needed for the lab.

### Wildcards and special constants

- `MPI_ANY_SOURCE` (receive from anyone), `MPI_ANY_TAG` (any tag). After the receive, read `status.MPI_SOURCE` and `status.MPI_TAG`.
- `MPI_STATUS_IGNORE`, `MPI_STATUSES_IGNORE` (for arrays of statuses).
- `MPI_PROC_NULL`: as `dest` or `source` it makes the call a no-op that returns immediately. Handy at the ends of a chain (see section 9.3).
- `MPI_SUCCESS`, `MPI_COMM_WORLD`, `MPI_IN_PLACE` (collectives, send buffer = receive buffer on that rank).

### Matching rule

A receive matches a send when **communicator, source and tag** match (wildcards aside).
Messages from the **same sender to the same receiver** (same comm) are **non-overtaking**: they are matched in the order they were sent.
Messages from **different** senders have no defined order.

---

## 3. Environment functions

### `int MPI_Init(int *argc, char ***argv)`
Initialises MPI. **Must be the first MPI call**, once per process. Pass `&argc, &argv`.

### `int MPI_Finalize(void)`
Shuts MPI down. **Must be the last MPI call**; every process calls it. Pending communication must be completed before.

### `int MPI_Comm_size(MPI_Comm comm, int *size)`
Writes the number of processes in `comm` into `*size`. "How many are we?"

### `int MPI_Comm_rank(MPI_Comm comm, int *rank)`
Writes the caller's rank in `comm` (0..size-1) into `*rank`. "Who am I?"

### `int MPI_Abort(MPI_Comm comm, int errorcode)`
Terminates **all** processes of `comm`. Use for fatal errors (file not found). Plain `exit` in one process can leave the others hanging. (The lecture slide wrote `MPI_Abort()` without arguments: that is wrong.)

### `double MPI_Wtime(void)`
Wall-clock time in seconds. `t0 = MPI_Wtime(); ... ; elapsed = MPI_Wtime() - t0;`

```cpp
MPI_Barrier(MPI_COMM_WORLD);          // line everyone up
double t0 = MPI_Wtime();
// ... work ...
double dt = MPI_Wtime() - t0;
double maxdt;
MPI_Reduce(&dt, &maxdt, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
if (rank == 0) printf("%f s\n", maxdt);
```

Initialisation check variant (lecture):
```cpp
int rc = MPI_Init(&argc, &argv);
if (rc != MPI_SUCCESS) { printf("Error starting MPI\n"); MPI_Abort(MPI_COMM_WORLD, rc); }
```

---

## 4. Blocking point-to-point

### `int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm)`
Standard-mode blocking send. **Returns when `buf` can be reused.** This does *not* mean the receiver has the data.
MPI chooses: small messages are usually **buffered** (returns at once), large ones use **rendezvous** (waits for the matching receive).
**Never rely on buffering**: a portable program must still work if every `MPI_Send` is replaced by `MPI_Ssend`.

```cpp
int x = 42;
MPI_Send(&x, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);                  // one int to rank 1, tag 0
unsigned char digits[100];
MPI_Send(digits, 100, MPI_UNSIGNED_CHAR, 3, 7, MPI_COMM_WORLD);   // 100 digits to rank 3, tag 7
MPI_Send(digits + 50, 25, MPI_UNSIGNED_CHAR, 3, 8, MPI_COMM_WORLD); // a slice: elements 50..74
```

### `int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status)`
Blocking receive. **Returns when the message has fully arrived in `buf`.**
`count` is capacity. Truncation (message longer than `count`) is an error. Real length: `MPI_Get_count`.

```cpp
int y;
MPI_Status st;
MPI_Recv(&y, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &st);
printf("got %d from %d tag %d\n", y, st.MPI_SOURCE, st.MPI_TAG);
MPI_Recv(&y, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);  // from whoever
```

Ping (two ranks):
```cpp
int token = 5;
if (rank == 0) {
    MPI_Send(&token, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
    MPI_Recv(&token, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
} else if (rank == 1) {
    MPI_Recv(&token, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    token *= 2;
    MPI_Send(&token, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
}
```

### `int MPI_Ssend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm)`
Synchronous send: same arguments as `MPI_Send`, **completes only after the matching receive has been posted and has started receiving**.
Behaviour is predictable and safe -> recommended for **debugging**: if a program hangs when `Send` is replaced by `Ssend`, it was unsafe. The lab requires exactly this test.

### `int MPI_Bsend(...)` and `MPI_Buffer_attach(void *buffer, int size)` / `MPI_Buffer_detach(void *buffer_addr, int *size)`
Buffered send: copies the message into a **user-supplied buffer** and returns at once; local, never waits for the receiver.
You must attach enough space first (size = data + `MPI_BSEND_OVERHEAD` per message). Too little space = error (not deadlock).
```cpp
int size = 1000 * sizeof(int) + MPI_BSEND_OVERHEAD;
char *buf = (char*)malloc(size);
MPI_Buffer_attach(buf, size);
MPI_Bsend(data, 1000, MPI_INT, 1, 0, MPI_COMM_WORLD);
MPI_Buffer_detach(&buf, &size);   // blocks until buffered messages are delivered
```

### `int MPI_Rsend(...)`
Ready send: legal **only if the matching receive is already posted**; otherwise undefined behaviour. Rarely used; may be faster.

### Summary of send modes (blocking)

| Mode | Call | Completes when | Needs matching recv posted? |
|---|---|---|---|
| Standard | `MPI_Send` | buffer reusable (MPI decides: buffer or wait) | no guarantee either way |
| Synchronous | `MPI_Ssend` | receiver has started receiving | waits for it |
| Buffered | `MPI_Bsend` | message copied to attached buffer (local) | no |
| Ready | `MPI_Rsend` | buffer reusable | **must** already be posted |

All four are matched by the same `MPI_Recv`. Non-blocking versions: `MPI_Isend`, `MPI_Issend`, `MPI_Ibsend`, `MPI_Irsend`.
There is only one receive mode.

### `int MPI_Sendrecv(const void *sendbuf, int sendcount, MPI_Datatype sendtype, int dest, int sendtag, void *recvbuf, int recvcount, MPI_Datatype recvtype, int source, int recvtag, MPI_Comm comm, MPI_Status *status)`
Sends one message and receives another in a single call, **deadlock-free** even if sends do not buffer (the library posts the receive for you).
`sendbuf` and `recvbuf` must be **different** buffers (`MPI_Sendrecv_replace` uses a single buffer).
Mnemonic: first the whole send description (5), then the whole receive description (5), then comm and status.

```cpp
// ring shift: every rank sends right, receives from the left, no deadlock
int right = (rank + 1) % size, left = (rank - 1 + size) % size;
int out = rank, in;
MPI_Sendrecv(&out, 1, MPI_INT, right, 0,
             &in,  1, MPI_INT, left,  0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
```

---

## 5. Status, Get_count, Probe

### `MPI_Status`
A struct filled by receives/waits. Fields you need: `status.MPI_SOURCE`, `status.MPI_TAG` (and the error field `MPI_ERROR`). The length is not a field: use `MPI_Get_count`.

### `int MPI_Get_count(const MPI_Status *status, MPI_Datatype datatype, int *count)`
Number of elements actually received (of the given datatype).
```cpp
MPI_Recv(buf, 1000, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
int n; MPI_Get_count(&st, MPI_INT, &n);   // n <= 1000
```

### `int MPI_Probe(int source, int tag, MPI_Comm comm, MPI_Status *status)`
**Blocks** until a matching message is *available*, **without receiving it**. Use it to find the size/source first, allocate, then `Recv`.
Note the argument list: **no buffer, no count, no datatype**.
```cpp
MPI_Status st; int n;
MPI_Probe(0, 5, MPI_COMM_WORLD, &st);
MPI_Get_count(&st, MPI_CHAR, &n);
char *buf = (char*)malloc(n);
MPI_Recv(buf, n, MPI_CHAR, 0, 5, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
```

### `int MPI_Iprobe(int source, int tag, MPI_Comm comm, int *flag, MPI_Status *status)`
Non-blocking probe: returns immediately, `*flag = 1` if a matching message is waiting.
```cpp
int flag;
MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &st);
if (flag) { /* receive it */ } else { /* do other work */ }
```

Probe vs Test: `Probe` asks "is there a message I could receive?" (before posting a receive); `Test` asks "has my already-posted operation finished?".

---

## 6. Non-blocking point-to-point

Three phases: **(1) initiate, (2) do other work, (3) complete** (Wait/Test).
Advantages: separate initiation from completion, avoid deadlock, overlap computation and communication, post receives early.
Disadvantage: harder to write and debug.

### `int MPI_Isend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request)`
### `int MPI_Irecv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *request)`
Same as Send/Recv, but **no `status`** and an extra **`request` (out)** as the last argument. They return immediately.

**Golden rules**
1. Do **not modify** an `Isend` buffer until it has completed (Wait/Test succeeded).
2. Do **not read** an `Irecv` buffer until it has completed.
3. Every request must be completed (Wait/Test/Waitall...) before the program ends and before the buffer goes out of scope/freed.
4. Completion of a non-blocking **send** = sender may reuse the buffer. Completion of a non-blocking **receive** = buffer contains the data.
5. A blocking send can match a non-blocking receive and vice versa.
6. Synchronous mode (`Issend`) affects **completion** (Wait completes only after the receive started), not initiation.
7. Distinct operations in flight need **distinct request objects** (use an array) and **distinct buffers** (or disjoint slices).

### `int MPI_Wait(MPI_Request *request, MPI_Status *status)`
Blocks until that operation completes. For a receive, fills `status`. Sets the request to `MPI_REQUEST_NULL`.

### `int MPI_Test(MPI_Request *request, int *flag, MPI_Status *status)`
Returns immediately; `*flag = 1` if complete (then `status` is valid), else 0. Use in a polling loop while doing useful work.
```cpp
int done = 0;
while (!done) {
    MPI_Test(&req, &done, MPI_STATUS_IGNORE);
    do_some_other_work();
}
```

### `int MPI_Waitall(int count, MPI_Request requests[], MPI_Status statuses[])`
Blocks until **all** `count` requests complete. `statuses` is an array of `count` (or `MPI_STATUSES_IGNORE`).

### `int MPI_Waitany(int count, MPI_Request requests[], int *index, MPI_Status *status)`
Blocks until **one** completes; `*index` = its position in `requests`. Use for "whichever message arrives first".
Also exist: `MPI_Testall`, `MPI_Testany`, `MPI_Waitsome`, `MPI_Testsome`.

### Example from the lecture: exchange with Irecv first (safe)
```cpp
double s_buf[100], r_buf[100];
MPI_Request request; MPI_Status status;
if (rank == 0) {
    MPI_Irecv(r_buf, 100, MPI_DOUBLE, 1, 22, MPI_COMM_WORLD, &request);
    MPI_Send (s_buf, 100, MPI_DOUBLE, 1, 10, MPI_COMM_WORLD);
    MPI_Wait(&request, &status);
} else if (rank == 1) {
    MPI_Irecv(r_buf, 100, MPI_DOUBLE, 0, 10, MPI_COMM_WORLD, &request);
    MPI_Send (s_buf, 100, MPI_DOUBLE, 0, 22, MPI_COMM_WORLD);
    MPI_Wait(&request, &status);
}
```

### Receiving from two senders in whatever order (Waitany)
```cpp
int a, b; MPI_Request r[2];
MPI_Irecv(&a, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &r[0]);
MPI_Irecv(&b, 1, MPI_INT, 2, 2, MPI_COMM_WORLD, &r[1]);
for (int k = 0; k < 2; k++) {
    int which;
    MPI_Waitany(2, r, &which, MPI_STATUS_IGNORE);
    if (which == 0) { /* a arrived */ } else { /* b arrived */ }
}
```

---

## 7. Collective communication

Rules:
- **Every process of the communicator** must call the collective, with consistent `root`, `count`, `datatype`, `op`, `comm`.
- All processes must call collectives **in the same order**.
- No tags. Blocking in the sense that the local buffers can be used on return; most libraries also synchronise (do not assume it, do not rely on it).
- **Never put a collective inside `if (rank == 0)`** unless every other rank also calls it.
- A collective cannot be matched by Send/Recv.

### `int MPI_Barrier(MPI_Comm comm)`
No process leaves until all have entered. Used to line up timing and phases.

### `int MPI_Bcast(void *buf, int count, MPI_Datatype datatype, int root, MPI_Comm comm)`
`root`'s `buf` is copied to the `buf` of **every** process. Same call on root and non-roots.
```cpp
int n;
if (rank == 0) n = 1000;                         // only root knows it
MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);    // now everyone has n
```
Equivalent with point-to-point:
```cpp
if (rank == 0) for (int i = 1; i < size; i++) MPI_Send(buf, 1, MPI_CHAR, i, 100, MPI_COMM_WORLD);
else MPI_Recv(buf, 1, MPI_CHAR, 0, 100, MPI_COMM_WORLD, &status);
```

### `int MPI_Scatter(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm)`
Root splits `sendbuf` into `size` consecutive pieces of `sendcount` elements; piece `i` goes to rank `i`. **The root also receives its own piece** (piece number = its rank), like every other process.
- `sendcount` = elements sent **to each** process (NOT the total). Total needed in `sendbuf` = `sendcount * size`.
- `sendbuf/sendcount/sendtype` only matter on root; the others can pass anything (e.g. `NULL`).
- Pieces are equal. Uneven -> pad, or use `MPI_Scatterv`.
- `recvcount` is normally equal to `sendcount`.
```cpp
// N divisible by p: root holds a[N]
int per = N / size;
unsigned char mine[1000];            // must be able to hold `per` elements
MPI_Scatter(a, per, MPI_UNSIGNED_CHAR, mine, per, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
```
Lecture figure: root `sendbuf = [1,2,3,4]`, 4 tasks -> task0 gets 1, task1 gets 2, task2 gets 3, task3 gets 4.

### `int MPI_Gather(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm)`
Inverse of Scatter: every process (root included) sends `sendcount` elements; root stores them **in rank order** in `recvbuf`.
`recvcount` = elements received **from each** process (not total). `recvbuf` only matters on root and must hold `recvcount * size`.
```cpp
MPI_Gather(mine, per, MPI_UNSIGNED_CHAR, result, per, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
```

### `int MPI_Reduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm)`
Combines the `count` values of each process element-wise with `op`; the result lands only in **root's `recvbuf`**.
Operations: `MPI_SUM`, `MPI_PROD`, `MPI_MAX`, `MPI_MIN`, `MPI_LAND`, `MPI_LOR`, `MPI_BAND`, `MPI_BOR`, `MPI_MAXLOC`, `MPI_MINLOC`.
`sendbuf` and `recvbuf` must not alias (use `MPI_IN_PLACE` on root as `sendbuf` to reduce in place).
```cpp
double mypi = ..., pi;
MPI_Reduce(&mypi, &pi, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);   // pi valid only on rank 0
```
Lecture figure: values 1,2,3,4 with MPI_SUM -> 10 at the destination.

### Variants you should recognise
| Call | Effect |
|---|---|
| `MPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm)` | Reduce, but **every** process gets the result (no root). |
| `MPI_Allgather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm)` | Gather, but every process gets the full result (no root). |
| `MPI_Scatterv(sendbuf, int sendcounts[], int displs[], sendtype, recvbuf, recvcount, recvtype, root, comm)` | Uneven pieces: piece `i` has `sendcounts[i]` elements starting at `displs[i]`. |
| `MPI_Gatherv(sendbuf, sendcount, sendtype, recvbuf, int recvcounts[], int displs[], recvtype, root, comm)` | Uneven gather. |
| `MPI_Alltoall(...)` | Every process sends a distinct piece to every other process. |
| `MPI_Scan(sendbuf, recvbuf, count, datatype, op, comm)` | Prefix reduction: rank i gets op over ranks 0..i. |

Uneven split with Scatterv (typical when `N % p != 0`):
```cpp
int counts[MAXP], displs[MAXP];
for (int i = 0; i < size; i++) {
    counts[i] = N / size + (i < N % size ? 1 : 0);
    displs[i] = (i == 0) ? 0 : displs[i-1] + counts[i-1];
}
MPI_Scatterv(a, counts, displs, MPI_UNSIGNED_CHAR,
             mine, counts[rank], MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
```

### Collectives vs point-to-point
Collectives are simpler and usually faster than hand-written loops of Send/Recv. But: they need **equal participation**,
they do not let you express a **chain** (carry from rank i-1 to i), and the lab asks you explicitly for Send/Recv in V1/V3 and Scatter/Gather in V2.

---

## 8. Chunk arithmetic (needed in every lab variant)

Splitting `N` items among `w` workers (worker index `k = 0..w-1`):
```cpp
int base = N / w, rem = N % w;
int count_k  = base + (k < rem ? 1 : 0);            // first `rem` workers get one more
int offset_k = k * base + (k < rem ? k : rem);      // start index of worker k's slice
```
Check: `sum(count_k) == N`, `offset_{k+1} == offset_k + count_k`.
If the assignment says "N/(p-1) digits" and N is not divisible, the **last** worker takes the remainder:
`count = N/w; if (k == w-1) count = N - k*(N/w);`.
For Scatter/Gather: pad to a multiple of `p`: `padded = ((N + p - 1) / p) * p; per = padded / p;`.

Digit addition with carry (LSD at index 0):
```cpp
int add(const unsigned char *a, const unsigned char *b, unsigned char *r, int n, int carry) {
    for (int i = 0; i < n; i++) {
        int s = a[i] + b[i] + carry;
        r[i] = s % 10;
        carry = s / 10;
    }
    return carry;       // carry out of this slice (0 or 1)
}
```

---

## 9. Combining the functions (patterns)

### 9.1 Master / worker with point-to-point (Lab V1 shape)
Master reads and distributes; workers compute; workers return results; master collects **in order**.
```cpp
// sum of an array: master hands out chunks, collects partial sums
int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size; MPI_Comm_rank(MPI_COMM_WORLD, &rank); MPI_Comm_size(MPI_COMM_WORLD, &size);
    int workers = size - 1, N = 1000;
    if (rank == 0) {
        int a[1000]; for (int i = 0; i < N; i++) a[i] = i;
        int chunk = N / workers;
        for (int w = 1; w <= workers; w++) {
            int cnt = (w == workers) ? N - (w-1)*chunk : chunk;
            MPI_Send(&cnt, 1, MPI_INT, w, 0, MPI_COMM_WORLD);
            MPI_Send(a + (w-1)*chunk, cnt, MPI_INT, w, 1, MPI_COMM_WORLD);
        }
        long total = 0;
        for (int w = 1; w <= workers; w++) {
            long part;
            MPI_Recv(&part, 1, MPI_LONG, w, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            total += part;
        }
        printf("%ld\n", total);
    } else {
        int cnt; int mine[1000];
        MPI_Recv(&cnt, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(mine, cnt, MPI_INT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        long part = 0; for (int i = 0; i < cnt; i++) part += mine[i];
        MPI_Send(&part, 1, MPI_LONG, 0, 2, MPI_COMM_WORLD);
    }
    MPI_Finalize();
}
```
(The same job is `Scatter` + local sum + `Reduce`, see 9.4.)

### 9.2 Pipeline / carry chain (Lab V1 core)
Rank `i` needs a value from rank `i-1` before finishing, and passes one to `i+1`.
```cpp
int carry = 0;
if (rank > 1) MPI_Recv(&carry, 1, MPI_INT, rank-1, CARRY, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
carry = add(first, second, result, cnt, carry);
if (rank < workers) MPI_Send(&carry, 1, MPI_INT, rank+1, CARRY, MPI_COMM_WORLD);
```
The chain is inherently sequential. Parallelism comes only from receiving/reading data in parallel and from the speculative trick (V1.1).

### 9.3 `MPI_PROC_NULL` removes the end-of-chain special cases
```cpp
int prev = (rank == 1)       ? MPI_PROC_NULL : rank - 1;
int next = (rank == workers) ? MPI_PROC_NULL : rank + 1;
int carry = 0;                                   // stays 0 when prev is PROC_NULL (recv is a no-op)
MPI_Recv(&carry, 1, MPI_INT, prev, CARRY, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
...
MPI_Send(&carry, 1, MPI_INT, next, CARRY, MPI_COMM_WORLD);
```
On an exam, explicit `if` is clearer; this is a compact alternative.

### 9.4 Scatter - compute - Gather (+ chain) (Lab V2 shape)
```cpp
// root has a[], b[] padded to a multiple of size; every rank (root too) adds a slice
MPI_Bcast(&per, 1, MPI_INT, 0, MPI_COMM_WORLD);
MPI_Scatter(a, per, MPI_UNSIGNED_CHAR, la, per, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
MPI_Scatter(b, per, MPI_UNSIGNED_CHAR, lb, per, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
int carry = 0;
if (rank > 0) MPI_Recv(&carry, 1, MPI_INT, rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
carry = add(la, lb, lr, per, carry);
if (rank < size-1) MPI_Send(&carry, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD);
MPI_Gather(lr, per, MPI_UNSIGNED_CHAR, r, per, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
```
Point-to-point and collective calls can be mixed; they use separate matching, so a Recv never steals a collective's data.

Scatter -> local work -> Reduce (partial sums):
```cpp
int per = N / size, local = 0;
MPI_Scatter(a, per, MPI_INT, mine, per, MPI_INT, 0, MPI_COMM_WORLD);
for (int i = 0; i < per; i++) local += mine[i];
int total;
MPI_Reduce(&local, &total, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
```

### 9.5 Non-blocking master + workers (Lab V3 shape)
Master posts all sends, then waits; worker posts receives for data **and** carry (any arrival order), waits for all, computes, posts sends, waits.
```cpp
// worker
MPI_Request rq[3]; int nr = 2;
MPI_Irecv(first,  cnt, MPI_UNSIGNED_CHAR, 0, 1, MPI_COMM_WORLD, &rq[0]);
MPI_Irecv(second, cnt, MPI_UNSIGNED_CHAR, 0, 2, MPI_COMM_WORLD, &rq[1]);
if (rank > 1) { MPI_Irecv(&carry, 1, MPI_INT, rank-1, 3, MPI_COMM_WORLD, &rq[2]); nr = 3; }
MPI_Waitall(nr, rq, MPI_STATUSES_IGNORE);
carry = add(first, second, result, cnt, carry);
MPI_Request sq[2];
MPI_Isend(&carry, 1, MPI_INT, rank < workers ? rank+1 : 0, rank < workers ? 3 : 5, MPI_COMM_WORLD, &sq[0]);
MPI_Isend(result, cnt, MPI_UNSIGNED_CHAR, 0, 4, MPI_COMM_WORLD, &sq[1]);
MPI_Waitall(2, sq, MPI_STATUSES_IGNORE);
```

### 9.6 Bcast + local work + Reduce (Pi, lecture 5)
```cpp
MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);          // everyone learns n
h = 1.0 / n; sum = 0.0;
for (i = myid + 1; i <= n; i += numprocs) {            // cyclic distribution
    x = h * (i - 0.5);
    sum += 4.0 / (1.0 + x*x);
}
mypi = h * sum;
MPI_Reduce(&mypi, &pi, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
```
Block vs cyclic distribution: cyclic (`i = rank; i < n; i += size`) balances load when iteration cost varies; block gives contiguous slices (needed when order matters, like digits).

### 9.7 Safe exchange between two ranks
```cpp
// (a) order the calls
if (rank == 0) { MPI_Send(&x,1,MPI_INT,1,0,c); MPI_Recv(&y,1,MPI_INT,1,0,c,&st); }
else           { MPI_Recv(&y,1,MPI_INT,0,0,c,&st); MPI_Send(&x,1,MPI_INT,0,0,c); }
// (b) Sendrecv
MPI_Sendrecv(&x,1,MPI_INT,other,0, &y,1,MPI_INT,other,0, c,&st);
// (c) Irecv first
MPI_Irecv(&y,1,MPI_INT,other,0,c,&rq); MPI_Send(&x,1,MPI_INT,other,0,c); MPI_Wait(&rq,&st);
```

### 9.8 Variable-length message with Probe
Sender sends a message of unknown length; the receiver `Probe`s, reads the count, allocates, then `Recv`s (see section 5).

### 9.9 Timing the whole parallel run
`Barrier` -> `Wtime` -> work -> `Barrier` -> `Wtime` -> `Reduce(MAX)` to rank 0 (as in the lab, which includes file reading in the measured time).

---

## 10. Lab 3 in one page (the likely exam content)

Add two big numbers stored as digit arrays (index 0 = least significant). Carry dependency goes from low to high ranks.

| Variant | Communication | Key facts |
|---|---|---|
| V0 | none | sequential loop with carry |
| V1 | `Send`/`Recv` | rank 0 only does I/O; reads `N/(p-1)` digits per worker and `Send`s both chunks; worker receives **chunks first, then carry**; sends carry to `id+1`, sums to rank 0; rank 0 receives sums in order and writes immediately; must work with `MPI_Ssend` |
| V1.1 | `Send`/`Recv` | worker adds with carry 0 **before** the carry arrives; when the real carry (1) arrives, add 1 to the result (propagate through 9s) |
| V2 | `Scatter`/`Gather` + `Send`/`Recv` for the carry | rank 0 also computes; pad `N` to a multiple of `p`; final carry check |
| V3 | `Isend`/`Irecv`/`Waitall` | worker uses `Irecv` for digits and carry because their arrival order is unpredictable |

Full solutions: `04_lab_walkthrough.md`.

---

## 11. How to study (no schedule, just what works)

1. **Write on paper, not on screen.** The exam is on paper: reading code feels like knowing it, writing proves it. Do all code exercises by hand.
2. **Cover and recall.** Cover `02_cheat_sheet.md`, write the signature of each function, then check. Argument order is the single most common error.
3. **Blank-page templates.** Without looking, write: the program frame, master/worker, carry chain, scatter-compute-gather, Isend/Irecv/Waitall. Compare with `03_templates.md`; repeat only the parts you missed.
4. **Reconstruct, don't memorise.** For each call, ask: what does it need? A buffer (where), a count (how many), a type (what), a peer (who), a tag (which), a communicator (where in the world). Every signature follows from that; collectives drop dest/tag and add root.
5. **Trace by hand.** For small `p` (2-4), draw a timeline per rank and write which call each rank is blocked in. This is how you find deadlocks and predict outputs.
6. **The Ssend test in your head.** For every send you write, ask "if this blocked until the receive started, would the program still finish?" Draw "who waits for whom"; a cycle = deadlock.
7. **Explain aloud.** Say why each receive is where it is ("worker receives chunks first because rank 0 sends them before it can possibly send anything else").
8. **Hunt bugs.** `08_common_mistakes.md` and `05_deadlock_and_safety.md` contain broken code. Finding the bug trains the checklist you will run on your own answer.
9. **Time yourself on the lab variants.** Write V1 and V2 from scratch in the time you would have in the exam. Then compare against `04_lab_walkthrough.md`. Simplify your code until it is short enough to write quickly (fixed arrays, no helper classes, no error handling unless asked).
10. **Run real code if you can.** Install OpenMPI (`sudo apt install openmpi-bin libopenmpi-dev`), compile the repo's `labs/03-assignment` and the snippets here, and swap `MPI_Send` for `MPI_Ssend` to watch deadlocks happen. Seeing a hang teaches more than reading about it.
11. **Use the lecture wording.** If the exam has theory questions, the professor's phrases (SPMD, cooperative operations, non-deterministic, "safe" program, completion) are the expected vocabulary. Rehearse `06_theory_qa.md` answers in your own words.
12. **Prioritise.** Highest value: V1 structure (chunks then carry), the Ssend/deadlock reasoning, V2 padding + Scatter/Gather arguments, V3 Irecv/Waitall rules, plus the argument order of Send/Recv/Isend/Irecv/Bcast/Scatter/Gather/Reduce. Rare functions (Bsend, Rsend, Alltoall) need only a one-sentence definition.
13. **On exam day, on paper:** write the frame first (include, Init, rank, size, Finalize), mark the rank branches, then fill in the communication; before finishing, check that every Send has a matching Recv (same tag, count, datatype, opposite ranks) and every request is waited.

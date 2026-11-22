# Theory Q&A (short answers, use the lecture vocabulary)

## Models and basic concepts

**What is MPI?**
A message-passing library *specification* (standard) for parallel computers, clusters and heterogeneous networks. Not a language, not a compiler, not one implementation (MPICH, OpenMPI, MS-MPI, Intel MPI implement it). Goal: powerful, efficient, portable parallel programs.

**What is a process in the message-passing model?**
A program counter and an address space. A process may have several threads sharing that address space. MPI is for communication **between processes**, which have separate address spaces.

**What does inter-process communication consist of?**
Synchronization and movement of data from one process's address space to another's.

**SIMD vs MIMD vs SPMD vs MPMD?**
SIMD: data parallel (same instruction on different data). MIMD: task parallel (different instructions, different data). SPMD: one program, many data, each process runs the same code and branches on its rank; not synchronised at the individual operation level. MPMD: different programs on different data (master/worker with two executables, coupled analysis). SPMD is equivalent to MPMD (any MPMD program can be turned into an SPMD one with `if (rank == ...)`). Message passing/MPI targets **MIMD/SPMD**.

**Cooperative vs one-sided communication?**
Cooperative: sender and receiver both participate (`Send`/`Recv`); communication and synchronisation are combined; changes to the receiver's memory happen with its explicit participation. One-sided (MPI-2): `Put`/`Get` to remote memory with only one process participating; communication and synchronisation are decoupled.

**What is a communicator? A group? A context? A rank?**
A group of processes plus a context (messages are received in the same context they were sent in) = communicator. A process is identified by its rank (0..size-1) within the group of a communicator. `MPI_COMM_WORLD` is the default communicator with all initial processes.

**Why is message passing non-deterministic and what does the programmer do about it?**
The arrival order of messages from two processes A and B at a third process C is undefined. The programmer must enforce determinism where needed (specific source/tag in receives, ordering of receives, separate "channels" with one writer and one reader).

**What does a message consist of? How is it described?**
Data described by the triple (address, count, datatype), plus envelope: source, destination, tag, communicator.

**Why does MPI have its own datatypes?**
Heterogeneous communication (machines with different representations/lengths of basic types) and application-oriented descriptions of layout (strided, indexed, structs) which reduce copies and allow special hardware (scatter/gather).

**What are tags for? What are the wildcards?**
User integer label to identify/screen messages. `MPI_ANY_TAG` and `MPI_ANY_SOURCE` accept anything; then `status.MPI_TAG`/`status.MPI_SOURCE` tell what arrived.

**Which six functions suffice for many programs?**
`MPI_Init`, `MPI_Finalize`, `MPI_Comm_size`, `MPI_Comm_rank`, `MPI_Send`, `MPI_Recv`.

**What happens on an MPI error by default?**
All processes abort. The user can make routines return an error code instead (or throw exceptions in C++), or install custom error handlers.

**How do you run an MPI program?**
`mpirun -np p ./prog` / `mpiexec -n p ./prog` (implementation-dependent; `mpiexec` is the MPI-2 recommendation).

**When use / not use MPI?**
Use when you need portability and performance, building libraries/tools, per-processor memory management. Not appropriate when fault tolerance is required, or for distributed computing (wide-area).

## Point-to-point

**What does it mean that `MPI_Send` has completed?**
The send buffer can be reused. The message may not have been received (it may sit in a system buffer).

**What does `MPI_Recv` completion mean?**
The message has arrived and the data is in the receive buffer.

**Can the receive count be smaller/larger than the send count?**
Receive count is the buffer capacity: receiving fewer elements is fine (use `MPI_Get_count` for the real number), receiving more than `count` is an error (truncation).

**Send modes: what are they and how do they differ?**
- Standard (`MPI_Send`): MPI decides whether to buffer or wait for the receive. Completes when the buffer is reusable.
- Synchronous (`MPI_Ssend`): completes only after the matching receive has been posted and started. Predictable and safe; good for debugging.
- Buffered (`MPI_Bsend`): copies to a user-attached buffer (`MPI_Buffer_attach`), completes immediately, local, no synchronisation; error if the buffer is too small.
- Ready (`MPI_Rsend`): only legal if the receive is already posted, otherwise undefined; may be faster; rarely used.
Each has a non-blocking version (`I` prefix). There is only one receive.

**What is `MPI_Sendrecv` for?**
Send and receive in one call; posts the receive before blocking, so a pairwise exchange or a ring shift cannot deadlock. Blocks until the send buffer is free and the receive buffer holds the data.

**What does `MPI_Probe` do and when is it used?**
Blocking pre-receive query: waits until a matching message is available without receiving it. Used to find out the size/source of a message (with `MPI_Get_count`) to allocate the right buffer before `MPI_Recv`. `MPI_Iprobe` is the non-blocking version (flag). It differs from `MPI_Test`, which checks completion of an already posted operation.

**Is the order of messages guaranteed?**
Between a given pair (same sender, same receiver, same communicator) messages do not overtake each other. Between different senders: no order.

## Blocking vs non-blocking

**Blocking vs non-blocking send/receive?**
Blocking returns only when the buffer can be reused / the data has arrived. Non-blocking (`MPI_Isend`/`MPI_Irecv`) returns immediately and gives a `request`; the operation must be completed with `MPI_Wait`/`MPI_Test` (or the `all/any/some` versions).

**Phases of a non-blocking communication?**
1) initiate, 2) do other work, 3) wait for completion.

**Advantages and disadvantages of non-blocking communication?**
Advantages: separate initiation from completion, can avoid deadlock, can reduce latency by posting receives early, overlap computation and communication. Disadvantage: code is more complex to develop, maintain and debug.

**What may you do with the buffers?**
Do not overwrite the send buffer until the send completes; do not use the receive buffer until the receive completes.

**`MPI_Wait` vs `MPI_Test`?**
`Wait` blocks until complete. `Test` returns immediately with `flag = true` if complete.

**Can a blocking send match a non-blocking receive?**
Yes, and vice versa. Non-blocking sends can use any mode (synchronous, buffered, standard, ready). Synchronous mode affects completion, not initiation.

**Why does lab variant 3 need `MPI_Irecv`?**
A worker waits for two inputs (digits from rank 0, carry from rank `id-1`) whose arrival order is non-deterministic. Posting both receives and waiting for all lets either arrive first.

## Safety and deadlock

**What is a deadlock? Give an example.**
Processes wait forever for events that cannot happen. Example: two processes each `MPI_Recv` before `MPI_Send` (always); or each `MPI_Send` before `MPI_Recv` with synchronous behaviour.

**What is a safe MPI program?**
One that does not rely on system buffering. All standard sends can be replaced by synchronous sends and it still works. A portable program must be safe.

**Why can a program work with small messages and hang with large ones?**
Small messages are buffered by the implementation (the send returns early); large ones need the receiver, so the unsafe send/send pattern blocks.

**Ways to fix an unsafe exchange?**
Order the operations (one side sends first, the other receives first), `MPI_Sendrecv`, `MPI_Isend`/`MPI_Irecv` + wait, or buffered sends with an attached buffer.

**What does buffered mode do on insufficient buffer space?**
Not a deadlock but a buffer-overflow error.

**What is (un)fairness in MPI?**
MPI does not guarantee fairness: if task 0 and task 1 both send to task 2 and task 2 has one matching receive, only one send completes (the other may starve).

## Collectives

**What is a collective operation?**
An operation called by **all** processes of a communicator (implicit synchronisation in most libraries). Examples: `Bcast`, `Scatter`, `Gather`, `Reduce`, `Barrier`.

**Bcast, Scatter, Gather, Reduce in one sentence each.**
`Bcast`: root's buffer is copied to all. `Scatter`: root's array is split into equal pieces, piece `i` to rank `i` (root keeps one). `Gather`: every rank's piece is concatenated in rank order at the root. `Reduce`: values of all ranks are combined with an operation (SUM, MAX, ...) and the result is stored at the root.

**Differences Reduce / Allreduce, Gather / Allgather?**
The `All` versions deliver the result to every process and have no `root`.

**Why use collectives instead of Send/Recv loops?**
Simpler code, usually more efficient implementations (tree algorithms), less deadlock risk. Limits: all processes must participate; cannot express a pipeline like the carry chain.

**What is the incorrect-code example for collectives?**
Rank 1 does `Recv` from 0 before `Bcast`, rank 0 does `Bcast` before `Send` to 1: deadlock, because collectives imply synchronisation.

**What does `MPI_Barrier` do?**
No process continues until all processes of the communicator have reached it. Used for synchronisation points and clean timing.

**Block vs cyclic distribution?**
Block: contiguous slices per process. Cyclic: process `r` handles `r, r+p, r+2p, ...` (Pi example). Cyclic balances irregular load; block is needed when positions matter (digits).

## Lab 3 questions

**Why can't the addition be trivially parallelised?**
Digit `i` depends on the carry from digit `i-1`; the dependency crosses slice boundaries, so slice `k` needs the carry out of slice `k-1`.

**Why is V1 deadlock-free even with `MPI_Ssend`?**
Rank 0 distributes before collecting and never waits for workers while distributing; workers receive chunks before the carry, send the carry before the result; the dependencies form a chain without cycle.

**What does V1.1 change and what does it cost?**
Workers add with carry 0 before the carry arrives and then fix up (increment the result, propagating through 9s) when the real carry is 1. Overlaps computation with waiting; costs an extra pass in the worst case; measured gain small because I/O and communication dominate.

**Why does V2 pad and how much?**
`Scatter`/`Gather` need equal pieces, so `N` is padded with zeros to a multiple of `p` (at least; padding to more than `N` also gives room for the final carry).

**Why is the number of workers `p-1` in V1/V3?**
Rank 0 only does I/O; the digits are divided among the `p-1` other processes.

**Why measure time including file reading?**
The task says all variants must include reading the numbers in the measured time (reading dominates for cheap additions).

**How do you check correctness?**
Compare the output file of every parallel run with the sequential output, on all test cases; test deadlock-freedom by replacing `MPI_Send` with `MPI_Ssend`.

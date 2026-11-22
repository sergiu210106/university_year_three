# PPD Lectures 4 and 5: Message Passing Interface

Converted from `Lecture 4.pdf` (MPI I, 44 slides) and `Lecture 5.pdf` (MPI II, 37 slides).
Slide figures are described in text where they carry meaning. Slide numbers are given as `(slide N)`.
Typos in the original slides (e.g. `1oo`, missing `MPI_COMM_WORLD` in `MPI_Abort()`) are kept in the code
where they matter and flagged with a `NOTE`.

---

# Part 1: Lecture 4 - Message Passing Interface (I)

## The message-passing model (slide 2)

- A process is (traditionally and simply viewed) a **program counter** and an **address space**.
- Processes may have multiple threads (program counters and associated stacks) sharing a single address space.
- MPI is for communication **among processes**, which have **separate address spaces**.
- Interprocess communication consists of:
  - synchronization
  - movement of data from one process's address space to another's.

## Types of parallel programming computations (slide 3)

- **SIMD** - data parallel
- **MIMD** - task parallel
- **MPMD** - Multiple Program Multiple Data
- **SPMD** - Single Program Multiple Data; not synchronized at the individual-operation level
- SPMD is equivalent to MPMD, since every MPMD program can be transformed into an SPMD program.
- **Message passing (and so also MPI) is for MIMD/SPMD parallelism.**

## SPMD / MPMD (slide 4)

Figure with three panels, each showing nodes running programs on separate data:

- (a) **SPMD**: one executable `a.out` runs on Node 1, Node 2, Node 3.
- (b) **MPMD: Master/Worker**: different executables (`a.out`, `b.out`) with arrows between the nodes.
- (c) **MPMD: Coupled Analysis**: `a.out`, `b.out`, `c.out`, one per node.

## The SPMD model (slide 5)

The same program is run by every process (here 3 processes):

1. Read array `a[]` from input
2. Get my rank
3. If rank==0 then is=1, ie=2; if rank==1 then is=3, ie=4; if rank==2 then is=5, ie=6
4. Process from `a(is)` to `a(ie)`
5. Gather the results to process 0
6. If rank==0 then write array `a()` to the output file

Each process works on its own slice (is..ie) of the array, then the pieces are gathered on process 0.

## What is MPI? (slide 6)

- A message-passing **library specification**
  - extended message-passing model
  - not a language or compiler specification
  - not a specific implementation or product
- For parallel computers, clusters, and heterogeneous networks
- Full-featured
- Designed to provide access to advanced parallel hardware for end users, library writers, tool developers

## Why use MPI? (slide 7)

MPI provides a powerful, efficient, and portable way to express parallel programs.

## Cooperative operations for communication (slide 8)

- The message-passing approach makes the exchange of data **cooperative**.
- Data is explicitly sent by one process and received by another.
- Any change in the receiving process's memory is made with the receiver's explicit participation.
- Communication and synchronization are combined.

```text
Process 0          Process 1
Send(data)
                   Receive(data)
```

## One-sided operations for communication (slide 9)

- One-sided operations include remote memory reads and writes.
- Only one process needs to explicitly participate.
- Advantage: communication and synchronization are decoupled.
- One-sided operations were included in MPI-2.

```text
Process 0          Process 1
Put(data)  ----->  (memory)
(memory)   <-----  Get(data)
```

## Architecture compatibility (slide 10)

Initially just for distributed memory (DM), later also for shared memory (SM).
Platforms: distributed memory, shared memory, hybrid.

## C, C++ and Fortran bindings (slide 11)

- C and Fortran bindings correspond closely.
- In C: `mpi.h` must be `#include`d; MPI functions return error codes or `MPI_SUCCESS`.
- In Fortran: `mpif.h` must be included, or use the MPI module (MPI-2); all MPI calls are subroutines, with a place for the return code in the last argument.
- C++ bindings and Fortran-90 issues are part of MPI-2.

## Implementations (slide 12)

MPICH, OpenMPI, IBM MPI, Microsoft MPI, IntelMPI (not free), others.

## A minimal MPI program, C (slide 13)

```c
#include "mpi.h"
#include <stdio.h>
int main( int argc, char *argv[] )
{
    MPI_Init( &argc, &argv );
    printf( "Hello, world!\n" );
    MPI_Finalize();
    return 0;
}
```

## Setting the MPI environment (slide 14)

```c
int MPI_Init(int* argc_ptr, char** argv_ptr[])
```
- Allows the system to do any setup needed to handle further calls to the MPI library.
- **Must be called before any other MPI function.**
- Requires passing along the command-line arguments.

```c
int MPI_Finalize(void)
```
- Companion of `MPI_Init`. Allows the system to free resources allocated to MPI.
- **Must be the last MPI function call.** (Other C/C++ statements after it are possible, but not MPI functions.)

## Hello World in MPI (slide 15)

```c
#include "mpi.h"
#include <stdio.h>
int main(int argc, char *argv[])
{
    int namelen, myid, numprocs;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    printf("Process %d / %d  : Hello world\n", myid, numprocs);
    MPI_Finalize();
    return 0;
}
```

Compilation and execution:
```console
$ mpicc hello.c -o hello
$ mpirun -np 4 hello
Process 0 / 4 : Hello world
Process 2 / 4 : Hello world
Process 1 / 4 : Hello world
Process 3 / 4 : Hello world
```
(The order of the output lines is not deterministic.)

## API: MPI functions (slide 16)

General format: `rc = MPI_Xxxxx(parameter, ...)`
Example: `rc = MPI_Bsend(&buf, count, type, dest, tag, comm)`
Error code `rc`: `MPI_SUCCESS` on success.

## Variant: verifying the correct initialization (slide 17)

```c
#include "mpi.h"
#include <stdio.h>
int main(int argc, char *argv[])
{
    int numtasks, rank, rc;
    rc = MPI_Init(&argc, &argv);
    if (rc != MPI_SUCCESS) {
        printf("Error starting MPI program. Terminating.\n");
        MPI_Abort(MPI_COMM_WORLD, rc);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    printf("Number of tasks= %d My rank= %d\n", numtasks, rank);
    /******* do some work *******/
    MPI_Finalize();
}
```

## Error handling (slide 18)

- By default, an error causes **all processes to abort**.
- The user can cause routines to return (with an error code) instead.
  - In C++, exceptions are thrown (MPI-2).
- A user can also write and install custom error handlers.
- Libraries might want to handle errors differently from applications.

## Running MPI programs (slide 19)

- The MPI-1 standard does not specify how to run an MPI program. It depends on the implementation and may require scripts, program arguments and/or environment variables.
- `mpiexec <args>` is part of MPI-2 as a recommendation, not a requirement. In practice: `mpiexec`, `mpirun`.

## Finding out about the environment (slide 20)

Two important questions early in a parallel program:
- How many processes are participating in this computation? -> `MPI_Comm_size`
- Which one am I? -> `MPI_Comm_rank` (a number between 0 and size-1)

## MPI basic send/receive (slide 21)

Point-to-point communication: process 0 does `Send(data)`, process 1 does `Receive(data)`.
Things that need specifying:
- How will "data" be described?
- How will processes be identified?
- How will the receiver recognize/screen messages?
- What will it mean for these operations to complete?

## What is message passing? (slide 22)

Data transfer **plus synchronization**. (Figure: process 0 asks "May I send?", process 1 answers "Yes", then the data flows over time.)
- Requires cooperation of sender and receiver.
- Cooperation is not always apparent in code.

## Determinism vs non-determinism (slide 23)

- Message-passing programming models are inherently **non-deterministic**: the order in which messages sent from two processes A and B arrive at a third process C is undefined.
- It is the programmer's responsibility to ensure deterministic execution, if required.
- In communication-channel-based models, determinism is guaranteed by defining separate channels for different communications and ensuring each channel has a single "writer" and a single "reader".

## Some basic concepts (slide 24)

- Processes can be collected into **groups**.
- Each message is sent in a **context** and must be received in the same context.
- A group and a context together form a **communicator**.
- A process is identified by its **rank** in the group associated with a communicator.
- There is a default communicator whose group contains all initial processes: `MPI_COMM_WORLD`.

## MPI datatypes (slide 25)

The data in a message to send or receive is described by a triple **(address, count, datatype)**.
An MPI datatype is recursively defined as:
- predefined, corresponding to a data type from the language (e.g. `MPI_INT`, `MPI_DOUBLE_PRECISION`)
- a contiguous array of MPI datatypes
- a strided block of datatypes
- an indexed array of blocks of datatypes
- an arbitrary structure of datatypes

There are MPI functions to construct custom datatypes, such as an array of (int, float) pairs, or a row of a matrix stored columnwise.

## MPI basic datatypes (slide 26)

| MPI datatype | C type |
|---|---|
| `MPI_CHAR` | signed char |
| `MPI_SHORT` | signed short int |
| `MPI_INT` | signed int |
| `MPI_LONG` | signed long int |
| `MPI_LONG_LONG_INT`, `MPI_LONG_LONG` | signed long long int |
| `MPI_SIGNED_CHAR` | signed char |
| `MPI_UNSIGNED_CHAR` | unsigned char |
| `MPI_UNSIGNED_SHORT` | unsigned short int |
| `MPI_UNSIGNED` | unsigned int |
| `MPI_UNSIGNED_LONG` | unsigned long int |
| `MPI_UNSIGNED_LONG_LONG` | unsigned long long int |
| `MPI_FLOAT` | float |
| `MPI_DOUBLE` | double |
| `MPI_LONG_DOUBLE` | long double |
| ... | |

## Why datatypes? (slide 27)

- Since all data is labeled by type, an MPI implementation can support communication between machines with very different memory representations and elementary type lengths (heterogeneous communication).
- Specifying application-oriented layout of data in memory reduces memory-to-memory copies in the implementation, and allows the use of special hardware (scatter/gather) when available.

## MPI tags (slide 28)

- Messages are sent with an accompanying user-defined integer **tag**, to help the receiver identify the message.
- Messages can be screened at the receiving end by specifying a specific tag, or not screened by specifying `MPI_ANY_TAG` in a receive.

## MPI basic (blocking) send (slide 29)

```c
MPI_SEND(start, count, datatype, dest, tag, comm)
```
- The message buffer is described by `(start, count, datatype)`; `start` is a pointer to the beginning of the buffer.
- The target process is specified by `dest`, the rank of the target in the communicator `comm`.
- When this function returns, the data has been delivered to the system and the buffer can be reused.
  - **The message may not have been received by the target process!**

## MPI basic (blocking) receive (slide 30)

```c
MPI_RECV(start, count, datatype, source, tag, comm, status)
```
- Waits until a matching (on source and tag) message is received from the system, and the buffer can be used.
- `source` is a rank in the communicator `comm`, or `MPI_ANY_SOURCE`.
- `status` contains further information.
- Receiving **fewer** than `count` occurrences of datatype is OK, but receiving **more** is an error.

## Retrieving further information (slide 31)

`status` is a data structure allocated in the user's program and provides information about the transfer:

```c
int recvd_tag, recvd_from, recvd_count;
MPI_Status status;
MPI_Recv(..., MPI_ANY_SOURCE, MPI_ANY_TAG, ..., &status);
recvd_tag  = status.MPI_TAG;
recvd_from = status.MPI_SOURCE;
MPI_Get_count(&status, datatype, &recvd_count);
```

## Example: two processes, 0 and 1 (slides 32-33)

```c
#include "mpi.h"
#include <stdio.h>
int main(argc, argv)
int argc;
char *argv[];
{
    int numtasks, rank, dest, source, rc, count, tag = 1;
    char inmsg, outmsg = 'x';
    MPI_Status Stat;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0) {
        dest = source = 1;
        rc = MPI_Send(&outmsg, 1, MPI_CHAR, dest, tag, MPI_COMM_WORLD);
        rc = MPI_Recv(&inmsg, 1, MPI_CHAR, source, tag, MPI_COMM_WORLD, &Stat);
    }
    else if (rank == 1) {
        dest = source = 0;
        rc = MPI_Recv(&inmsg, 1, MPI_CHAR, source, tag, MPI_COMM_WORLD, &Stat);
        rc = MPI_Send(&outmsg, 1, MPI_CHAR, dest, tag, MPI_COMM_WORLD);
    }
    rc = MPI_Get_count(&Stat, MPI_CHAR, &count);
    printf("Task %d: Received %d char(s) from task %d with tag %d \n",
           rank, count, Stat.MPI_SOURCE, Stat.MPI_TAG);
    MPI_Finalize();
}
```
Rank 0 sends first then receives; rank 1 receives first then sends (so it is safe).

## MPI is simple (slide 34)

Many parallel programs can be written using just six functions, only two of which are non-trivial:
`MPI_INIT`, `MPI_FINALIZE`, `MPI_COMM_SIZE`, `MPI_COMM_RANK`, `MPI_SEND`, `MPI_RECV`.
- Point-to-point (send/recv) is not the only way...
- Other, more efficient variants are possible.

## Collective operations in MPI (slide 35)

- Collective operations are called by **all processes in a communicator** (implicit synchronization).
- `MPI_BCAST` distributes data from one process (the root) to all others in a communicator.
- `MPI_REDUCE` combines data from all processes in the communicator and returns it to one process.
- In many numerical algorithms, SEND/RECEIVE can be replaced by BCAST/REDUCE, improving both simplicity and efficiency.

## MPI_Bcast (slide 36)

"Broadcasts a message to all other processes of that group."
Figure: `count = 1; source = 1;` broadcast originates in task 1:
`MPI_Bcast(&msg, count, MPI_INT, source, MPI_COMM_WORLD);`
Before: task 1 holds `7`, the others are empty. After: tasks 0-3 all hold `7`.

## Broadcast implemented using MPI_Send and MPI_Recv (slide 37)

```c
if (id == 0) {
    for (int i = 1; i < nprocs; i++)
        MPI_Send(buf, 1, MPI_CHAR, i, 100, MPI_COMM_WORLD);
}
else // id != 0
    MPI_Recv(buf, 1, MPI_CHAR, 0, 100, MPI_COMM_WORLD, &status);

// semantically equivalent to
MPI_Bcast(buf, 1, MPI_CHAR, 0, MPI_COMM_WORLD);
```

## MPI_Barrier (slide 38)

`MPI_Barrier(comm)`: no process proceeds past the barrier until every process in the communicator has called it.

```c
#include <stdio.h>
#include <mpi.h>
int main(int argc, char** argv) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    printf("Process %d: Starting task phase 1...\n", rank);
    // different workloads
    // ...
    printf("Process %d: Reached the barrier, waiting for others...\n", rank);
    // Synchronize all processes here. No process will proceed past this
    // point until every process in MPI_COMM_WORLD has called MPI_Barrier.
    MPI_Barrier(MPI_COMM_WORLD);
    // All processes resume simultaneously after the barrier
    printf("Process %d: Passed the barrier! Proceeding to phase 2.\n", rank);
    MPI_Finalize();
    return 0;
}
```

## MPI_Scatter (slide 39)

"Sends data from one task to all other tasks in a group."
Figure: `sendcnt = 1; recvcnt = 1; src = 1;`
`MPI_Scatter(sendbuf, sendcnt, MPI_INT, recvbuf, recvcnt, MPI_INT, src, MPI_COMM_WORLD);`
Task 1 contains the message to be scattered: `sendbuf = [1, 2, 3, 4]`. After the call task 0 has `1`, task 1 has `2`, task 2 has `3`, task 3 has `4` in `recvbuf`.

## MPI_Gather (slide 40)

"Gathers together values from a group of processes."
Figure: `sendcnt = 1; recvcnt = 1; src = 1;` messages will be gathered in task 1:
`MPI_Gather(sendbuf, sendcnt, MPI_INT, recvbuf, recvcnt, MPI_INT, src, MPI_COMM_WORLD);`
Before: task 0..3 hold `1,2,3,4` in `sendbuf`. After: task 1's `recvbuf` is `[1, 2, 3, 4]`.

## Reduction, all-to-one (slide 41)

```c
int MPI_Reduce(
    void *sendbuf /* in */, void *recvbuf /* out */, int count /* in */,
    MPI_Datatype datatype /* in */, MPI_Op op /* in */, int root /* in */,
    MPI_Comm comm /* in */
)
```
- Combines values in `sendbuf` on all processes to a single value using the specified operation `op`.
- The combined value is put in `recvbuf` of the process with rank `root`.
- The routine is called by all group members using the **same arguments** for `count`, `datatype`, `op`, `root` and `comm`.

## MPI_Reduce figure (slide 42)

"Perform an associative reduction operation across all tasks in the group and place the result in one task."
`count = 1; dest = 1;` result will be placed in task 1:
`MPI_Reduce(sendbuf, recvbuf, count, MPI_INT, MPI_SUM, dest, MPI_COMM_WORLD);`
Before: tasks 0..3 hold `1,2,3,4`. After: task 1's `recvbuf` is `10`.

## Collectives synchronization (slide 43)

- In most libraries, collectives imply a synchronization. An implementation without synchronization is costly.
- **Incorrect code (high risk of deadlock):**

```c
if (my_rank == 1)
    MPI_Recv(0);
MPI_Bcast(...);
if (my_rank == 0)
    MPI_Send(1);
```
Rank 1 blocks in the Recv waiting for rank 0, but rank 0 is blocked in the Bcast (which needs rank 1) before it reaches its Send.

## References and MPI sources (slide 44)

- William Gropp, Ewing Lusk. *An Introduction to MPI Parallel Programming with the Message Passing Interface.* Argonne National Laboratory.
- The standard itself: <http://www.mpi-forum.org>. Books: <http://mpitutorial.com/recommended-books/>. <http://www.mcs.anl.gov/mpi>

---

# Part 2: Lecture 5 - Message Passing Interface (II): Non-blocking operations

## Example: PI approximation (slides 2-4)

- PI is approximated by evaluating the integral of `4/(1+x*x)` between 0 and 1. The integral is approximated by a sum of rectangle areas.
- The master process (rank 0) asks the user for the number of intervals and **broadcasts** it to all other processes.
- Each process adds up a subset of the rectangle areas.
- Finally, the sums computed by each process are added together using a **reduction**.

```c
#include "mpi.h"
#include <math.h>
int main(int argc, char *argv[])
{
    int done = 0, n, myid, numprocs, i, rc;
    double PI25DT = 3.141592653589793238462643;
    double mypi, pi, h, sum, x, a;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    while (!done) {
        if (myid == 0) {
            printf("Enter the number of intervals: (0 quits) ");
            scanf("%d", &n);
        }
        MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (n == 0) break;

        h   = 1.0 / (double) n;
        sum = 0.0;
        for (i = myid + 1; i <= n; i += numprocs) {   // cyclic distribution
            x = h * ((double)i - 0.5);
            sum += 4.0 / (1.0 + x*x);
        }
        mypi = h * sum;
        MPI_Reduce(&mypi, &pi, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        if (myid == 0)
            printf("pi is approximately %.16f, Error is %.16f\n",
                   pi, fabs(pi - PI25DT));
    }
    MPI_Finalize();
    return 0;
}
```
(The original slide layout scrambles the closing braces; the structure above is the intended one.)

## Deadlock, safety, fairness (slide 5)

Section headings: Deadlock, Safety, Fairness.

## Deadlock example (slide 6)

Process 0 receives a number from and sends a number to process 1; process 1 receives from and sends to process 0.

```c
#include <stdio.h>
#include "mpi.h"
int main(int argc, char** argv)
{
    int my_rank, numbertoreceive, numbertosend = -16;
    MPI_Status status;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    if (my_rank == 0) {
        MPI_Recv(&numbertoreceive, 1, MPI_INT, 1, 20, MPI_COMM_WORLD, &status);
        MPI_Send(&numbertosend,    1, MPI_INT, 1, 10, MPI_COMM_WORLD);
    }
    else if (my_rank == 1) {
        MPI_Recv(&numbertoreceive, 1, MPI_INT, 0, 10, MPI_COMM_WORLD, &status);
        MPI_Send(&numbertosend,    1, MPI_INT, 0, 20, MPI_COMM_WORLD);
    }
    MPI_Finalize();
    return 0;
}
```
Both processes block in `MPI_Recv` first: **always deadlocks**, regardless of buffering.

## Sources of deadlocks (slide 7)

```cpp
Process 0                              Process 1
MPI_Send(&x,1,MPI_INT,1,10,comm);      MPI_Send(&x,1,MPI_INT,0,20,comm);
MPI_Recv(&y,1,MPI_INT,1,20,comm,&st);  MPI_Recv(&y,1,MPI_INT,0,10,comm,&st);
```
- When the Send returns, the sending buffer (`&x`) can be reused. The message may not have been received by the target process; it could be stored in a system buffer.
- This code is called **"unsafe"** because it depends on the availability of system buffers.

## Buffers usage (slide 8, decision of the specific MPI implementation)

- Completion of `MPI_Recv` is trivial: the data can now be used.
- Completion of `MPI_Send` is trickier: it implies the data has been stored away so the program is free to overwrite the send buffer.
  - **Non-local:** the data can be sent directly to the receive buffer.
  - **Local (buffering):** the data can be stored in a local buffer (system or user provided), in which case the send could return before the receive is initiated.

## Possible deadlock: buffering-dependent code (slide 9)

Process 0 sends to 1 then receives from 1; process 1 sends to 0 then receives from 0.

```c
if (my_rank == 0) {
    MPI_Send(&numbertosend,    1, MPI_INT, 1, 10, MPI_COMM_WORLD);
    MPI_Recv(&numbertoreceive, 1, MPI_INT, 1, 20, MPI_COMM_WORLD, &status);
}
else if (my_rank == 1) {
    MPI_Send(&numbertosend,    1, MPI_INT, 0, 20, MPI_COMM_WORLD);
    MPI_Recv(&numbertoreceive, 1, MPI_INT, 0, 10, MPI_COMM_WORLD, &status);
}
```
Success depends on buffering: **one of the sends must buffer and return**, otherwise deadlock occurs.

## Write safe code (slide 10)

- A safe MPI program should not rely on system buffering for success.
- Any system will eventually run out of buffer space as message sizes grow.
- The user should design proper send/receive orders to avoid deadlock.

## Solution to the "unsafe" problem (slides 11-12)

Order the operations more carefully:

```text
Process 0       Process 1
Send(1)         Recv(0)
Recv(1)         Send(0)
```
```c
if (my_rank == 0) {
    MPI_Send(&numbertosend,    1, MPI_INT, 1, 10, MPI_COMM_WORLD);
    MPI_Recv(&numbertoreceive, 1, MPI_INT, 1, 20, MPI_COMM_WORLD, &status);
}
else if (my_rank == 1) {
    MPI_Recv(&numbertoreceive, 1, MPI_INT, 0, 10, MPI_COMM_WORLD, &status);
    MPI_Send(&numbertosend,    1, MPI_INT, 0, 20, MPI_COMM_WORLD);
}
```

## Safety (slide 13)

- If standard-mode sends are used, a deadlock **may** occur when both processes are blocked because there is not enough buffer space.
- The same will **certainly** happen if the synchronous mode is used.
- If the buffered mode is used and not enough buffer space is available, the program will not complete either; but instead of a deadlock we get a buffer-overflow error.
- **A portable program using standard-mode sends should not rely on message buffering to complete without deadlock.**
  - All sends in such a portable program can be replaced with synchronous-mode sends and the program will still run correctly.
  - The buffered send mode can be used for programs that require buffering.

## MPI_Sendrecv (slide 14)

Send and receive in one call: sends a message and posts a receive before blocking. Blocks until the sending application buffer is free for reuse **and** the receiving application buffer contains the received message.

```c
MPI_Sendrecv(
    &sendbuf, sendcount, sendtype, dest, sendtag,
    &recvbuf, recvcount, recvtype, source, recvtag, comm, &status
)
```

## Circular deadlock (slide 15)

Figure: a communicator with processes 0-5 arranged in a ring and messages between them: if every process does a blocking send to its neighbour before receiving, nobody can proceed (a cycle of waits).

## Fairness (slide 16)

MPI does not guarantee fairness; it is up to the programmer to prevent "operation starvation".
Example: task 0 sends a message to task 2; task 2 has a corresponding receive; task 1 sends a competing message that matches task 2's receive. **Only one of the sends will complete!**

## Overview of blocking, probe and non-blocking calls (slide 17)

| Operation | Call |
|---|---|
| Blocking send | `MPI_Send(buffer,count,type,dest,tag,comm)` |
| Blocking receive | `MPI_Recv(buffer,count,type,source,tag,comm,status)` |
| Blocking probe | `MPI_Probe(source,tag,comm,&status)` - allows checking of incoming messages without actually receiving them. If a send matching the probe has been initiated by some process, the call to `MPI_Probe` will return. |
| Non-blocking send | `MPI_Isend(buffer,count,type,dest,tag,comm,request)` |
| Non-blocking receive | `MPI_Irecv(buffer,count,type,source,tag,comm,request)` |
| Wait | `MPI_Wait(&request,&status)` - returns when the operation identified by `request` is complete. |
| Test | `MPI_Test(&request,&flag,&status)` - returns `flag = true` if the operation identified by `request` is complete; then `status` is set to contain information on the completed operation. |
| Non-blocking probe | `MPI_Iprobe(source,tag,comm,&flag,&status)` - like `MPI_Probe` but without blocking. |

## A quick overview of MPI's send modes (slide 18)

Send modes represent different choices of buffering (where is the data kept until it is received) and synchronization (when does a send complete).

- **`MPI_Send`**: will not return until you can use the send buffer. It may or may not block (it is allowed to buffer, either on the sender or receiver side, or to wait for the matching receive).
- **`MPI_Bsend`**: may buffer; returns immediately and you can use the send buffer. A late add-on to the MPI specification; should be used only when absolutely necessary.
- **`MPI_Ssend`**: will not return until the matching receive is posted.
- **`MPI_Rsend`**: may be used ONLY if the matching receive is already posted. The user is responsible for writing a correct program.
- **`MPI_Isend`**: a nonblocking send call indicates that the system may start copying data out of the send buffer. The sender must not modify any part of the send buffer after a nonblocking send is called, until the send completes. (A nonblocking receive call indicates that the system may start writing data into the receive buffer; the receiver should not access any part of the receive buffer after a nonblocking receive is called, until the receive completes.)
- **`MPI_Ibsend`**: buffered nonblocking.
- **`MPI_Issend`**: synchronous nonblocking. A Wait/Test will complete only when the matching receive is posted.
- **`MPI_Irsend`**: as with `MPI_Rsend`, but nonblocking.

These calls allocate a communication request object and associate it with the request handle (the argument `request`). The request can be used later to query the status of the communication or wait for its completion.

## Non-blocking communication (slides 19-20)

Advantages:
- allows the separation between the initiation of the communication and the completion
- can avoid deadlock
- can reduce latency by posting receive calls early

Disadvantages:
- complex to develop, maintain and debug code

## Non-blocking communication phases (slide 21)

1. Initiate non-blocking communication.
2. Do some work (perhaps involving other communications?).
3. Wait for non-blocking communication to complete.

| Non-blocking operation | MPI call |
|---|---|
| Standard send | `MPI_ISEND` |
| Synchronous send | `MPI_ISSEND` |
| Buffered send | `MPI_IBSEND` |
| Ready send | `MPI_IRSEND` |
| Receive | `MPI_IRECV` |

## Non-blocking send/recv syntax (slide 22)

```c
int MPI_Isend(void* message /* in */, int count /* in */, MPI_Datatype datatype /* in */,
              int dest /* in */, int tag /* in */, MPI_Comm comm /* in */,
              MPI_Request* request /* out */)

int MPI_Irecv(void* message /* out */, int count /* in */, MPI_Datatype datatype /* in */,
              int source /* in */, int tag /* in */, MPI_Comm comm /* in */,
              MPI_Request* request /* out */)
```

## Non-blocking send/recv details (slide 23)

- A non-blocking operation requires a minimum of two function calls: one to start the operation and one to complete it.
- The `request` is used to query the status of the communication or to wait for its completion.
- The user must NOT overwrite the send buffer until the send (data transfer) is complete.
- The user can not use the receive buffer before the receive is complete.

## Completion (slide 24)

- Completion of a non-blocking **send** means the sender is now free to update the send buffer.
- Completion of a non-blocking **receive** means the receive buffer contains the received data.

```c
int MPI_Wait(MPI_Request* request /* in-out */, MPI_Status* status /* out */)
int MPI_Test(MPI_Request* request /* out */, int* flag /* out */, MPI_Status* status /* out */)
```

## Details of Wait/Test (slide 25)

- `request` identifies a previously posted send/receive.
- `MPI_Wait()` returns when the operation is complete, and the status is updated for a receive.
- `MPI_Test()` returns immediately, with `flag = true` if the posted operation corresponding to the `request` handle is complete.

## Non-blocking send/recv example (slide 26)

```c
#include <stdio.h>
#include "mpi.h"
int main(int argc, char** argv)
{
    int my_rank, nprocs, recv_count;
    MPI_Request request;
    MPI_Status status;
    double s_buf[100], r_buf[100];
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    if (my_rank == 0) {
        MPI_Irecv(r_buf, 100, MPI_DOUBLE, 1, 22, MPI_COMM_WORLD, &request);
        MPI_Send (s_buf, 100, MPI_DOUBLE, 1, 10, MPI_COMM_WORLD);
        MPI_Wait(&request, &status);
    }
    else if (my_rank == 1) {
        MPI_Irecv(r_buf, 100, MPI_DOUBLE, 0, 10, MPI_COMM_WORLD, &request);
        MPI_Send (s_buf, 100, MPI_DOUBLE, 0, 22, MPI_COMM_WORLD);
        MPI_Wait(&request, &status);
    }
    MPI_Get_count(&status, MPI_DOUBLE, &recv_count);
    printf("proc %d, source %d, tag %d, count %d\n",
           my_rank, status.MPI_SOURCE, status.MPI_TAG, recv_count);
    MPI_Finalize();
}
```
NOTE: the slide has typos (`1oo`, an extra `(` in `MPI_Get_count`); corrected above. Both ranks post the Irecv first, so the symmetric exchange cannot deadlock.

## Other communication modes (slide 27)

| Mode | Functions | Completion condition |
|---|---|---|
| Synchronous send | `MPI_Ssend`, `MPI_Issend` | A send will not complete until a matching receive has been posted and the matching receive has begun reception of the data. |
| Buffered send (has additional associated functions; the send operation is **local**) | `MPI_Bsend`, `MPI_Ibsend` | Always completes (unless an error occurs), irrespective of the receiver. |
| Standard send | `MPI_Send`, `MPI_Isend` | Message sent (no guarantee that the receive has started). It is up to MPI to decide what to do. |
| Ready send | `MPI_Rsend`, `MPI_Irsend` | May be used only when a matching receive has already been posted. |

## Other communication modes: notes (slide 28)

- `MPI_Ssend()`: synchronization of source and destination; the behavior is predictable and safe. Recommended for debugging purposes.
- `MPI_Bsend()`: only copies the message to a buffer; completes immediately; predictable behavior and no synchronization. The user must allocate extra buffer space with `MPI_Buffer_attach()`.
- `MPI_Rsend()`: rarely used; completes immediately; will succeed only if a matching receive is already posted; if the receiving process is not ready, the action is undefined; may improve performance.

## Waiting versus testing (slide 29)

```c
int MPI_Wait(MPI_Request *request, MPI_Status *status)
int MPI_Test(MPI_Request *request, int *flag, MPI_Status *status)
```

## Blocking and non-blocking (slide 30)

Send and receive can be blocking or non-blocking.
- A blocking send can be used with a non-blocking receive, and vice versa.
- Non-blocking sends can use any mode: synchronous, buffered, standard, or ready.
- Synchronous mode affects **completion**, not initiation.

## MPI_Probe (slide 31)

- `MPI_Probe` is a **pre-receive** query about a message. Typically needed when a message is expected from a certain source with a certain tag, but the treatment of the message depends on its contents, which might vary. E.g. it can help you choose whether to allocate a larger buffer for a big message, or whether to execute `MPI_Cancel` to save memory.
- It serves a different purpose than `MPI_Test`: it gives an earlier determination about the status of a message having a given source and tag, letting you take action **before** posting a receive for it.
- Blocking and nonblocking versions exist:
  `MPI_Iprobe(int source, int tag, MPI_Comm comm, int *flag, MPI_Status *status);`
- `MPI_Probe` simply blocks until a matching message arrives.

## MPI_Probe example (slide 32)

Rank 0 sends 3 messages of different lengths (tags 0, 1, 2); rank 1 probes for each, queries the length, allocates exactly that much, then receives.

```c
int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 2) {
        fprintf(stderr, "Need at least 2 processes\n");
        MPI_Abort(MPI_COMM_WORLD, 1);   // NOTE: slide has MPI_Abort() with no arguments
        return 1;
    }
    if (rank == 0) {
        // Sender: send messages of different lengths
        const char *messages[] = {
            "Hi", "Hello, World!", "This is a longer message from rank 0"
        };
        int num_messages = 3;
        for (int i = 0; i < num_messages; i++) {
            int len = strlen(messages[i]) + 1;   // include null terminator
            MPI_Send(messages[i], len, MPI_CHAR, 1, i, MPI_COMM_WORLD);
        }
    } else if (rank == 1) {
        // Receiver: use MPI_Probe to check message size before receiving
        int num_messages = 3;
        for (int i = 0; i < num_messages; i++) {
            MPI_Status status;
            // 1. Probe for the next message (blocks until a message is available)
            MPI_Probe(0, i, MPI_COMM_WORLD, &status);
            // 2. Query how many elements are in the incoming message
            int count;
            MPI_Get_count(&status, MPI_CHAR, &count);
            // 3. Allocate exactly the right amount of memory
            char *buf = (char *)malloc(count * sizeof(char));
            if (!buf) { fprintf(stderr, "malloc failed\n"); MPI_Abort(MPI_COMM_WORLD, 1); }
            // 4. Now receive the message into the correctly-sized buffer
            MPI_Recv(buf, count, MPI_CHAR, 0, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            printf("Rank 1 received (tag %d): \"%s\"\n", i, buf);
            free(buf);
        }
    }
    MPI_Finalize();
    return 0;
}
```

## Measure time (slide 33)

```c
double MPI_Wtime(void);   // time is measured in seconds
```
**Ping-pong exercise:** write a program in which two processes repeatedly pass a message back and forth. Insert timing calls to measure the time taken for one message. Investigate how the time taken varies with the size of the message.

## Message-Passing Interface: extended capabilities (slide 34)

- Dynamic process management (dynamic process startup, dynamic establishment of connections)
- One-sided communication (put/get, other operations)
- Parallel I/O
- Other MPI-2 features: generalized requests, bindings for C++/Fortran-90, interlanguage issues

## When to use MPI (slide 35-36)

Appropriate when we need: portability and performance; building tools for others (libraries); managing memory on a per-processor basis.
**Not** appropriate when we require fault tolerance, or for distributed computing.

# One-page cheat sheet (memorise the argument ORDER)

Rule of thumb: **buffer, count, type, peer, tag, comm** (+ status / request last).
Collectives drop the tag and add `root` before `comm`.

## Environment
```cpp
MPI_Init(&argc, &argv);                MPI_Finalize();
MPI_Comm_rank(MPI_COMM_WORLD, &rank);  MPI_Comm_size(MPI_COMM_WORLD, &size);
MPI_Abort(MPI_COMM_WORLD, code);       double t = MPI_Wtime();
```

## Point-to-point (blocking)
```cpp
MPI_Send (buf, count, type, dest,   tag, comm)
MPI_Ssend(buf, count, type, dest,   tag, comm)     // sync: waits for recv to start
MPI_Bsend(buf, count, type, dest,   tag, comm)     // needs MPI_Buffer_attach
MPI_Rsend(buf, count, type, dest,   tag, comm)     // recv must already be posted
MPI_Recv (buf, count, type, source, tag, comm, &status)
MPI_Sendrecv(sbuf,scount,stype,dest,stag,  rbuf,rcount,rtype,source,rtag, comm,&status)
MPI_Probe(source, tag, comm, &status)              // no buffer!
MPI_Iprobe(source, tag, comm, &flag, &status)
MPI_Get_count(&status, type, &n)
```

## Point-to-point (non-blocking)
```cpp
MPI_Isend(buf, count, type, dest,   tag, comm, &request)
MPI_Irecv(buf, count, type, source, tag, comm, &request)
MPI_Wait   (&request, &status)
MPI_Test   (&request, &flag, &status)
MPI_Waitall(n, requests, statuses)                 // MPI_STATUSES_IGNORE
MPI_Waitany(n, requests, &index, &status)
```

## Collectives (everyone calls)
```cpp
MPI_Barrier(comm)
MPI_Bcast  (buf, count, type, root, comm)
MPI_Scatter(sbuf, scount_per_proc, stype, rbuf, rcount, rtype, root, comm)
MPI_Gather (sbuf, scount,          stype, rbuf, rcount_per_proc, rtype, root, comm)
MPI_Reduce (sbuf, rbuf, count, type, op, root, comm)
MPI_Allreduce(sbuf, rbuf, count, type, op, comm)
MPI_Allgather(sbuf, scount, stype, rbuf, rcount, rtype, comm)
MPI_Scatterv(sbuf, counts[], displs[], stype, rbuf, rcount, rtype, root, comm)
MPI_Gatherv (sbuf, scount, stype, rbuf, counts[], displs[], rtype, root, comm)
```

## Constants
`MPI_COMM_WORLD`, `MPI_ANY_SOURCE`, `MPI_ANY_TAG`, `MPI_STATUS_IGNORE`, `MPI_STATUSES_IGNORE`, `MPI_PROC_NULL`, `MPI_IN_PLACE`, `MPI_SUCCESS`
Ops: `MPI_SUM MPI_PROD MPI_MAX MPI_MIN MPI_LAND MPI_LOR MPI_MAXLOC MPI_MINLOC`
Types: `MPI_CHAR MPI_UNSIGNED_CHAR MPI_SHORT MPI_INT MPI_UNSIGNED MPI_LONG MPI_LONG_LONG MPI_FLOAT MPI_DOUBLE MPI_LONG_DOUBLE`

## Facts to know cold
| Fact | |
|---|---|
| `count` | elements, not bytes; on Recv it is a capacity |
| Send returning | buffer reusable; NOT "received" |
| Recv returning | data is in the buffer |
| Ssend returning | receive has started |
| Scatter/Gather count | **per process**, not total; root takes part too |
| Reduce result | only on root (Allreduce: everyone) |
| Collective | all ranks, same order, same root/count/type/op |
| Isend buffer | do not modify until Wait completes |
| Irecv buffer | do not read until Wait completes |
| Request | one per outstanding operation; always complete it |
| Safe program | still works if every Send is an Ssend |
| Same sender->receiver | messages do not overtake each other |
| Different senders | arrival order undefined |

## Compile / run
```bash
mpicc  a.c   -o a        mpic++ a.cpp -o a
mpirun -np 4 ./a         mpiexec -n 4 ./a
```

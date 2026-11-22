# Common mistakes (check your paper answer against this list)

## Structure
1. **Forgetting `MPI_Init` / `MPI_Finalize`**, or calling an MPI function before `Init` / after `Finalize`.
2. **Not passing `&argc, &argv`** to `MPI_Init`.
3. **Forgetting that all processes run the same code.** Anything outside an `if (rank == ...)` runs on every process (e.g. a `printf`, a file open, a loop over workers).
4. **Using `size` where `size - 1` is needed.** If rank 0 only does I/O, there are `size - 1` workers; slices are `N / (size-1)`.
5. **Reading the input file in every process** when only rank 0 should (or opening the output in a worker).
6. **Using a value only rank 0 knows** (e.g. `N`) on the workers without sending it (Bcast, or Send).
7. **Assuming a printed order.** Output from different processes interleaves arbitrarily.

## Arguments
8. **Mixing up the argument order.** Always: buffer, count, type, peer, tag, comm (status/request last). `MPI_Send` has `dest` where `MPI_Recv` has `source`.
9. **`count` in bytes instead of elements.** `MPI_Send(a, n * sizeof(int), MPI_INT, ...)` is wrong; it is `n`.
10. **Passing the variable instead of its address** (`MPI_Recv(x, ...)` where `x` is an `int`; correct: `&x`). Arrays and pointers are passed without `&`.
11. **Type mismatch** between the C type and the MPI datatype (`unsigned char` array with `MPI_INT`, `double` with `MPI_FLOAT`).
12. **Forgetting the `status` argument of `Recv`** (use `MPI_STATUS_IGNORE`), or adding one to `Isend`/`Irecv` (they take `request` instead).
13. **Tag/rank mismatch:** the receiver waits on tag 2 / source 1, but the sender used tag 3 / dest 2. Draw the send-receive pairs next to each other and check each field.
14. **Receive count smaller than the message** (truncation error). It may be larger.
15. **Passing `MPI_Status` by value** (`status` instead of `&status`).

## Collectives
16. **Collective inside `if (rank == 0)`** (or inside a branch not executed by all).
17. **Different `root`, `count`, `datatype` or `op` on different ranks.**
18. **`sendcount` = total** in Scatter/Gather. It is the amount **per process**.
19. **Expecting Scatter's remainder to be handled automatically.** Pieces are equal; pad or use `Scatterv`.
20. **Reading the result of `Reduce`/`Gather` on non-root ranks.** It is valid only on `root` (use `Allreduce`/`Allgather` for everyone).
21. **Thinking Scatter excludes the root.** Root receives a piece too (so the root also needs a receive buffer).
22. **Using `Send`/`Recv` to match a collective** or the reverse.
23. **Expecting a non-root's buffer to be sent by `Bcast`.** Only the root's buffer is the source; on every other rank the buffer is *overwritten*.

## Blocking semantics and deadlock
24. **Both ranks `Send` first, then `Recv`** (works for small messages, hangs for big ones). Reorder, use `Sendrecv`, or `Isend`/`Irecv`.
25. **Both ranks `Recv` first.** Always deadlocks.
26. **Relying on `MPI_Send` buffering.** Test mentally with `MPI_Ssend`.
27. **Receiving in the wrong order**, e.g. a worker waits for the carry from `id-1` before the chunks that rank 0 sends -> serialises or blocks.
28. **Master collecting before distributing everything** (with synchronous sends this deadlocks the chain).
29. **Sending the result before the carry** in the carry chain (can delay or block the next worker).
30. **Assuming messages from two different senders arrive in a fixed order.** Only messages from the same sender to the same receiver are non-overtaking.
31. **Using `MPI_ANY_SOURCE` and assuming a particular sender**, or forgetting to read `status.MPI_SOURCE`.

## Non-blocking
32. **Reading the `Irecv` buffer before `Wait`** or modifying the `Isend` buffer before `Wait`.
33. **Reusing one `MPI_Request` for several outstanding operations.** Use an array.
34. **Never waiting** on a request (leak; `Finalize` with pending operations).
35. **Buffer going out of scope** (local variable in a block / function) while the request is outstanding.
36. **Posting the `Isend` of a slice and then overwriting that slice** (e.g. re-reading the next chunk into the same array). Use different slices or wait first.
37. **Waiting on a request that was never started** or on the wrong `count` in `Waitall` (array larger than the number actually posted: uninitialised requests!). Pass exactly the number posted (`nr` in the worker example).
38. **`MPI_Test` flag ignored** (treating the buffer as ready when `flag == 0`).

## Lab-specific (number addition)
39. **Digit order:** index 0 is the least significant digit; carry flows from low index to high index, from rank `k` to rank `k+1`.
40. **Forgetting the final carry** (sum has one more digit, e.g. 999 + 1) or forgetting to pass the last worker's carry back to rank 0.
41. **Not padding the shorter number with zeros**, or padding but still sending the unpadded length.
42. **Chunk remainder lost:** `N / w` truncates, so the last `N % w` digits are never processed. Give the remainder to the last worker (or spread it).
43. **Worker 1 waiting for a carry that nobody sends.** Worker 1 starts with carry 0; the last worker sends no carry to a next worker.
44. **Gather with unpadded array** or `Scatter` with `N` not a multiple of `p`.
45. **Speculative addition (V1.1):** forgetting to keep the speculative carry-out when fixing up, or not propagating through nines (`9 + 1`) in the fix-up.
46. **Measured time not including file reading** when the task asks for it.

## Quick self-check before you hand in
- [ ] Init first, Finalize last
- [ ] every rank branch is complete (all roles have code)
- [ ] every Send has a Recv (count/type/tag/rank) and vice versa
- [ ] collectives called by all, same order
- [ ] swap Send -> Ssend in your head: still finishes?
- [ ] non-blocking: waited, buffers untouched
- [ ] count = elements, address-of used correctly

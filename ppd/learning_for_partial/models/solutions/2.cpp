/*
 * MODEL 2: transform with h(v, w), ring sum, gather back.
 *
 * input.txt:  w (real), then n (positive int), then n reals, one per line
 *
 *   h(v, w):  case 1: v <  w  ->  v = v + w   (count in A)
 *             case 2: v >= w  ->  v = v - w   (count in B)
 *
 * ---------------------------------------------------------------------------
 * PLAN: one MPI construct per stage
 *   Stage 1  read on rank 0           -> MPI_Bcast(w), MPI_Bcast(n)
 *            "as equally as possible" -> 2 x MPI_Scatter (explained below)
 *   Stage 2  pure local loop, no communication
 *   Stage 3  cascade 0 -> 1 -> ... -> P-1 -> 0 -> MPI_Send / MPI_Recv in a ring
 *   Stage 4  blocks to p0 in index order -> 2 x MPI_Gather (mirror of stage 1)
 *            pairs (A,B) to p1             -> MPI_Gather with root = 1
 *
 * ---------------------------------------------------------------------------
 * "AS EQUALLY AS POSSIBLE" WITH PLAIN MPI_Scatter
 *   q = n / P   (everybody gets at least q numbers)
 *   r = n % P   (r numbers are left over, r < P)
 *   Block sizes may differ by at most 1, so ranks 0 .. r-1 get ONE extra number.
 *
 *   Scatter #1: data[0 .. q*P - 1], q numbers to each rank -> local[0 .. q-1]
 *   Scatter #2: the leftovers data[q*P .. n-1], 1 number to each rank
 *               -> local[q]. Only ranks 0 .. r-1 get a real leftover. The
 *               others get an unused slot and ignore it.
 *   So rank i owns cnt = q + (i < r ? 1 : 0) numbers.
 *
 *   Example: n = 10, P = 4 -> q = 2, r = 2
 *     scatter #1: rank0 {d0,d1}  rank1 {d2,d3}  rank2 {d4,d5}  rank3 {d6,d7}
 *     scatter #2: rank0 {d8}     rank1 {d9}     (rank2, rank3: nothing real)
 *     sizes 3,3,2,2  -> differ by at most 1
 *
 *   Why not just pad with 0 (like Model 1)? h would be applied to the padding.
 *   That changes A/B and the sum, and the sizes would no longer be "as equal
 *   as possible" (n = 5, P = 4 with ceil blocks gives 2,2,1,0).
 *
 *   Index order on the way back: Gather #1 puts the q-blocks back at
 *   data[0 .. q*P-1]. Gather #2 puts each leftover back at position q*P + i.
 *   Every number returns to its original index.
 * ---------------------------------------------------------------------------
 *
 * Assumptions: P >= 2 (stage 4 sends to p1, and the ring needs a neighbour),
 *              n <= MAX, P <= MAXP.
 *
 * Compile / run:  mpic++ 2.cpp -o 2  &&  mpirun -np 4 ./2
 */
#include <mpi.h>
#include <fstream>
#include <iostream>
using namespace std;

const int MAX  = 1000;   // max numbers
const int MAXP = 100;    // max processes

int main(int argc, char *argv[]) {
    int rank, P;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &P);

    // Optional on paper, but it shows you thought about the edge case.
    if (P < 2) {
        cout << "Run with at least 2 processes.\n";
        MPI_Finalize();
        return 0;
    }

    /* ======================= STAGE 1: read + distribute ======================= */
    double w;
    int n;
    double data[MAX];                    // all n numbers, only filled on rank 0
    if (rank == 0) {
        ifstream in("input.txt");
        in >> w >> n;
        for (int i = 0; i < n; i++)
            in >> data[i];
        in.close();
    }
    // The statement only asks for w to be broadcast. We also broadcast n, because
    // every rank needs it to compute q, r and its own block size.
    MPI_Bcast(&w, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(&n, 1, MPI_INT,    0, MPI_COMM_WORLD);

    int q = n / P;                       // numbers everyone gets
    int r = n % P;                       // leftovers: ranks 0..r-1 get one more
    int cnt = q + (rank < r ? 1 : 0);    // size of MY block

    double local[MAX + 1];               // q numbers + room for 1 leftover at local[q]

    // Scatter #1: q numbers to each rank (rank 0 keeps the first q itself,
    // so it takes part in the computation as required).
    MPI_Scatter(data, q, MPI_DOUBLE, local, q, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // Scatter #2: the leftovers, 1 per rank. Plain Scatter reads P numbers from
    // the root, but only r are real. So rank 0 copies the r leftovers into a
    // P-sized buffer (the slots after r stay unused).
    // `r` is the same on every rank, so either ALL ranks enter this `if` or
    // none do. That keeps the collective safe.
    double extra[MAXP];
    if (r > 0) {
        if (rank == 0)
            for (int i = 0; i < r; i++) extra[i] = data[q * P + i];
        MPI_Scatter(extra, 1, MPI_DOUBLE, &local[q], 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    }

    /* ======================= STAGE 2: apply h locally ======================= */
    int A = 0, B = 0;                    // how many times case 1 / case 2 was applied
    double localSum = 0.0;               // sum of the TRANSFORMED values
    for (int i = 0; i < cnt; i++) {      // cnt, not q: include my leftover if I have one
        if (local[i] < w) {              // case 1
            local[i] = local[i] + w;
            A++;
        } else {                         // case 2 (v >= w)
            local[i] = local[i] - w;
            B++;
        }
        localSum += local[i];            // add AFTER transforming
    }

    /* ======================= STAGE 3: cascade (ring) sum ======================= */
    // p0 starts: it sends its own localSum to p1.
    // p_i (i >= 1): receive the partial sum from p_{i-1}, add my localSum,
    //               pass it on to p_{i+1}. The last one, p_{P-1}, sends to p0
    //               ((P-1)+1) % P = 0.
    // p0 then receives the global sum from p_{P-1} and prints it.
    //
    // Deadlock check: p0 SENDS first and everyone else RECEIVES first, so the
    // chain always moves forward. It still works if every Send were an Ssend.
    // (If p0 did Recv before Send, all P ranks would wait in Recv: deadlock.)
    if (rank == 0) {
        double globalSum;
        MPI_Send(&localSum, 1, MPI_DOUBLE, 1, 0, MPI_COMM_WORLD);
        MPI_Recv(&globalSum, 1, MPI_DOUBLE, P - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        cout << "Global sum = " << globalSum << "\n";
    } else {
        double partial;
        MPI_Recv(&partial, 1, MPI_DOUBLE, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        partial += localSum;
        MPI_Send(&partial, 1, MPI_DOUBLE, (rank + 1) % P, 0, MPI_COMM_WORLD);
    }

    /* ======================= STAGE 4: collect results ======================= */
    // (a) transformed blocks -> p0, in the original index order.
    //     This is the exact mirror of stage 1, so we can write straight into
    //     `data` on rank 0 (the original values are not needed any more).
    MPI_Gather(local, q, MPI_DOUBLE, data, q, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    if (r > 0) {
        // Every rank sends local[q]. Rank 0 receives them in extra[0..P-1],
        // but only extra[0..r-1] are real leftovers (from ranks 0..r-1).
        MPI_Gather(&local[q], 1, MPI_DOUBLE, extra, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        if (rank == 0)
            for (int i = 0; i < r; i++) data[q * P + i] = extra[i];  // back to its index
    }

    // (b) pair (A, B) -> p1. This is a Gather with root = 1 (the root does not
    //     have to be 0). p1 receives 2 ints from each rank, in rank order:
    //     allPairs = {A0,B0, A1,B1, A2,B2, ...}
    int pair[2] = {A, B};
    int allPairs[2 * MAXP];              // only meaningful on rank 1
    MPI_Gather(pair, 2, MPI_INT, allPairs, 2, MPI_INT, 1, MPI_COMM_WORLD);
    // Shorter alternative for (b), the same result in one call:
    //   int tot[2]; MPI_Reduce(pair, tot, 2, MPI_INT, MPI_SUM, 1, MPI_COMM_WORLD);

    if (rank == 0) {                     // p0 writes the transformed array
        ofstream out("output.txt");
        for (int i = 0; i < n; i++)
            out << data[i] << "\n";
        out.close();
    }
    if (rank == 1) {                     // p1 adds up the pairs
        int totalA = 0, totalB = 0;
        for (int i = 0; i < P; i++) {
            totalA += allPairs[2 * i];       // even positions: A of rank i
            totalB += allPairs[2 * i + 1];   // odd positions:  B of rank i
        }
        cout << "Total A (case 1) = " << totalA << ", Total B (case 2) = " << totalB << "\n";
    }

    MPI_Finalize();
    return 0;
}

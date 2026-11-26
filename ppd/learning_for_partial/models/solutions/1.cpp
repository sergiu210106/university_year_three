/*
 * MODEL 1: split A into X (positives) and Y (negatives), keeping their order.
 *
 * input.txt :  n, then a_0 ... a_{n-1}, one per line
 * outputX.txt: the positive numbers, in the order they appear in A
 * outputY.txt: the negative numbers, in the order they appear in A
 * Only rank 0 touches files. Every rank (rank 0 included) does part of the work.
 *
 * ---------------------------------------------------------------------------
 * PLAN (write this out in 4 steps before you code)
 *   1. rank 0 reads n and A    -> MPI_Bcast(n), so every rank knows the block size
 *   2.                         -> MPI_Scatter(A), each rank gets `per` numbers
 *   3. each rank splits its block into a local X and a local Y
 *      (going left to right, so the order inside the block stays the same)
 *   4. MPI_Gather the local counts and blocks to rank 0. Rank 0 joins them in
 *      rank order (rank order = order in A) and writes the files.
 *
 * Why the order is preserved:
 *   Scatter hands out A in rank order: rank 0 gets A[0..per-1], rank 1 gets
 *   A[per..2per-1], and so on. Gather puts the blocks back in rank order. Inside
 *   a block we copy left to right. So the final X and Y follow the order of A.
 *
 * ---------------------------------------------------------------------------
 * 8 POINTS: p divides n
 *   per = n / p. Scatter sends exactly per * p = n numbers.
 *
 * 2 POINTS: p does NOT divide n. What breaks?
 *   per = n / p rounds down, so per * p < n. The last n % p numbers are never
 *   scattered and never processed.
 *   Example: n = 10, p = 4 gives per = 2. Only 8 numbers get sent and
 *   A[8], A[9] are lost.
 *
 *   Fix (only 2 lines change, marked with  <-- V2  below):
 *     - per = ceil(n / p) = (n + p - 1) / p    (now per * p >= n)
 *     - put 0 in the extra slots A[n .. per*p - 1] (padding)
 *   Why padding with 0 is safe: 0 is neither positive nor negative, so the
 *   padding never ends up in X or Y. Nothing else in the program changes.
 *   When p does divide n there are no extra slots, so this gives the same
 *   result as the 8-point version.
 * ---------------------------------------------------------------------------
 *
 * Array sizes: static arrays of MAX elements. We assume n + p <= MAX
 * (the padding adds at most p - 1 slots). Counts arrays are indexed by rank,
 * so we assume p <= MAXP.
 *
 * Compile / run:  mpic++ 1.cpp -o 1  &&  mpirun -np 4 ./1
 */
#include <mpi.h>
#include <fstream>
using namespace std;

const int MAX  = 1000;   // max numbers (n + padding)
const int MAXP = 100;    // max processes

int main(int argc, char *argv[]) {
    int rank, p;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &p);

    /* ---------- Step 1: rank 0 reads everything ---------- */
    int n = 0;
    int A[MAX];                          // the full array, only filled on rank 0
    if (rank == 0) {
        ifstream in("input.txt");
        in >> n;
        for (int i = 0; i < n; i++)
            in >> A[i];
        in.close();
    }
    // Bcast is a collective, so ALL ranks call it (outside any `if`).
    // On rank 0, n is the source. On the others, n is overwritten with that value.
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Every rank computes the block size itself (it only depends on n and p).
    // V1 (8p):  int per = n / p;
    int per = (n + p - 1) / p;                                      // <-- V2

    // V2: fill the padding slots with 0 (V1 has no padding, so skip this line).
    if (rank == 0)
        for (int i = n; i < per * p; i++) A[i] = 0;                 // <-- V2

    /* ---------- Step 2: scatter A ---------- */
    int local[MAX];                      // my block of A
    // sendcount and recvcount are PER PROCESS (per), not the total n.
    // The send buffer (A) is only read on the root.
    MPI_Scatter(A, per, MPI_INT, local, per, MPI_INT, 0, MPI_COMM_WORLD);

    /* ---------- Step 3: local work, split my block ---------- */
    // At most `per` positives and at most `per` negatives fit in my block.
    int lx[MAX], ly[MAX];
    int cx = 0, cy = 0;                  // how many I actually put in lx / ly
    for (int i = 0; i < per; i++) {
        if (local[i] > 0)
            lx[cx++] = local[i];
        else if (local[i] < 0)
            ly[cy++] = local[i];
        // local[i] == 0 -> goes nowhere (this is also why 0-padding is harmless)
    }

    /* ---------- Step 4: collect everything on rank 0 ---------- */
    // Plain MPI_Gather needs the SAME count from every rank, but cx differs
    // from rank to rank. Trick:
    //   - gather the counts (1 int per rank)
    //   - gather the full fixed-size blocks (per ints per rank)
    //   - rank 0 keeps only the first cntX[r] values of block r
    // The rest of each block (after cx) is garbage, but it is never read.
    int cntX[MAXP], cntY[MAXP];          // cntX[r] = how many positives rank r found
    int allX[MAX], allY[MAX];            // block r is at allX[r*per ... r*per+per-1]
    MPI_Gather(&cx, 1,   MPI_INT, cntX, 1,   MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Gather(&cy, 1,   MPI_INT, cntY, 1,   MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Gather(lx,  per, MPI_INT, allX, per, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Gather(ly,  per, MPI_INT, allY, per, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        ofstream fx("outputX.txt");
        ofstream fy("outputY.txt");
        for (int r = 0; r < p; r++) {             // rank order = original order
            for (int j = 0; j < cntX[r]; j++)     // only the useful part of block r
                fx << allX[r * per + j] << "\n";  // block r starts at r*per
            for (int j = 0; j < cntY[r]; j++)
                fy << allY[r * per + j] << "\n";
        }
        fx.close();
        fy.close();
    }

    MPI_Finalize();
    return 0;
}

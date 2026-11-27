/*
 * MODEL 3: matrix-vector multiplication y = A * x.
 * input.txt: n, the matrix dimension. Rank 0 generates A and x and prints y.
 *
 * PLAN
 *   1. Broadcast n and all n entries of x. Each dot product needs all of x.
 *   2. Scatter consecutive rows of A, with rows * n integers per rank.
 *   3. Compute one dot product per local row.
 *   4. Gather rows results per rank. Rank order restores the original row order.
 *
 * V1 (8p): p divides n, so use rows = n / p and skip the padding loop.
 * V2 (2p, below): n / p would leave the last n % p rows undistributed.
 * Round rows up and append zero rows. The extra results come last, so rank 0
 * prints the first n entries as before. Each row still has n columns.
 *
 * Compile / run:  mpic++ 3.cpp -o 3  &&  mpirun -np 4 ./3
 */
#include <mpi.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
using namespace std;

const int MAX = 1000;  // maximum dimension and padded row count

int main(int argc, char *argv[]) {
    int rank, p;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &p);

    /* ---------- Step 1: rank 0 reads n and initializes A and x ---------- */
    int n = 0;
    if (rank == 0) {
        ifstream in("input.txt");
        in >> n; 
    }
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int rows = (n + p - 1) / p;  // V2: round up for an uneven split
    int paddedRows = rows * p;

    static int A[MAX * MAX], localA[MAX * MAX];
    int x[MAX], localY[MAX], y[MAX];
    if (rank == 0) {
        for (int i = 0; i < n * n; i++)
            A[i] = rand() % 100;
        for (int i = 0; i < n; i++)
            x[i] = rand() % 100;
        for (int i = n * n; i < paddedRows * n; i++)
            A[i] = 0;  // V2: append zero rows
    }
    MPI_Bcast(x, n, MPI_INT, 0, MPI_COMM_WORLD);

    /* ---------- Step 2: scatter whole rows of A ---------- */
    // A[i * n + j] is at row i, column j. Each block has rows * n entries.
    int entries = rows * n;
    MPI_Scatter(A, entries, MPI_INT, localA, entries, MPI_INT, 0, MPI_COMM_WORLD);

    /* ---------- Step 3: compute one result per local row ---------- */
    for (int i = 0; i < rows; i++) {
        int sum = 0;
        for (int j = 0; j < n; j++)
            sum += localA[i * n + j] * x[j];
        localY[i] = sum;
    }

    /* ---------- Step 4: gather y in row order and print it ---------- */
    MPI_Gather(localY, rows, MPI_INT, y, rows, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank == 0)
        for (int i = 0; i < n; i++)
            cout << y[i] << "\n";

    MPI_Finalize();
    return 0;
}

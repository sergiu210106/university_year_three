/*
 * MODEL 4: two-dimensional convolution, B = A * K.
 * input.txt: n (matrix size), then k (odd kernel size).
 * Assumes 3 <= p, p divides n, and 1 <= k <= n <= 1000.
 * Rank 0 generates A and K and prints the n-by-n result. Values outside A
 * are zero, and the kernel is flipped in both directions for convolution.
 *
 * PLAN
 *   1. Share n, k and K. Give each rank a disjoint block of n / p rows of A.
 *   2. Exchange halo rows with immediate neighbors using MPI_Sendrecv.
 *   3. Compute one output for each owned matrix entry.
 *   4. Collect the output blocks on rank 0 and print them in row order.
 *
 * Variant A uses only point-to-point communication. Variant B replaces
 * steps 1 and 4 with Bcast, Scatter and Gather. Both use the same halo exchange.
 *
 * Compile:    mpic++ 4.cpp -o 4
 * Variant A:  mpirun -np 4 ./4
 * Variant B:  mpirun -np 4 ./4 B
 */
#include <mpi.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
using namespace std;

const int MAX = 1000;
const int MAX_ROWS = MAX / 3;  // p >= 3

int main(int argc, char *argv[]) {
    int rank, p;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &p);
    bool useCollectives = argc > 1 && string(argv[1]) == "B";

    /* ---------- Step 1: share the kernel and distribute A ---------- */
    int sizes[2] = {};
    if (rank == 0) {
        ifstream in("input.txt");
        if (!(in >> sizes[0] >> sizes[1]))
            sizes[0] = sizes[1] = 0;
    }

    if (useCollectives) {
        MPI_Bcast(sizes, 2, MPI_INT, 0, MPI_COMM_WORLD);
    } else if (rank == 0) {
        for (int r = 1; r < p; r++)
            MPI_Send(sizes, 2, MPI_INT, r, 0, MPI_COMM_WORLD);
    } else {
        MPI_Recv(sizes, 2, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    int n = sizes[0], k = sizes[1];

    int rows = n / p;
    int entries = rows * n;
    int halo = k / 2;
    static double A[MAX * MAX], B[MAX * MAX], K[MAX * MAX];
    // The owned rows start after the top halo. Static storage zeroes the halos.
    static double localA[(MAX_ROWS + MAX) * MAX], localB[MAX_ROWS * MAX];
    double *owned = localA + halo * n;

    if (rank == 0) {
        for (int i = 0; i < n * n; i++)
            A[i] = (rand() % 1000) / 10.0;
        for (int i = 0; i < k * k; i++)
            K[i] = (rand() % 1000) / 10.0;
    }

    if (useCollectives) {
        MPI_Bcast(K, k * k, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        MPI_Scatter(A, entries, MPI_DOUBLE, owned, entries, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    } else if (rank == 0) {
        copy(A, A + entries, owned);
        for (int r = 1; r < p; r++) {
            MPI_Send(K, k * k, MPI_DOUBLE, r, 1, MPI_COMM_WORLD);
            MPI_Send(A + r * entries, entries, MPI_DOUBLE, r, 2, MPI_COMM_WORLD);
        }
    } else {
        MPI_Recv(K, k * k, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(owned, entries, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    /* ---------- Step 2: fill the top and bottom halos ---------- */
    int up = rank == 0 ? MPI_PROC_NULL : rank - 1;
    int down = rank == p - 1 ? MPI_PROC_NULL : rank + 1;
    // Fill one row per side per round, from nearest to farthest.
    // After rows rounds, the send buffers refer to previously received rows.
    // MPI_PROC_NULL leaves the zero-filled outer rows in place.
    for (int d = 0; d < halo; d++) {
        MPI_Sendrecv(owned + (rows - 1 - d) * n, n, MPI_DOUBLE, down, 3,
                     owned - (d + 1) * n, n, MPI_DOUBLE, up, 3,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Sendrecv(owned + d * n, n, MPI_DOUBLE, up, 4,
                     owned + (rows + d) * n, n, MPI_DOUBLE, down, 4,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    /* ---------- Step 3: convolve each owned row ---------- */
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int u = -halo; u <= halo; u++) {
                for (int v = -halo; v <= halo; v++) {
                    int col = j - v;
                    if (col >= 0 && col < n)
                        sum += owned[(i - u) * n + col] * K[(u + halo) * k + v + halo];
                }
            }
            localB[i * n + j] = sum;
        }
    }

    /* ---------- Step 4: collect and print B ---------- */
    if (useCollectives) {
        MPI_Gather(localB, entries, MPI_DOUBLE, B, entries, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    } else if (rank == 0) {
        copy(localB, localB + entries, B);
        for (int r = 1; r < p; r++)
            MPI_Recv(B + r * entries, entries, MPI_DOUBLE, r, 5, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    } else {
        MPI_Send(localB, entries, MPI_DOUBLE, 0, 5, MPI_COMM_WORLD);
    }

    if (rank == 0) {
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++)
                cout << B[i * n + j] << (j == n - 1 ? '\n' : ' ');
        }
    }

    MPI_Finalize();
    return 0;
}

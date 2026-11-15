#include <mpi.h>

#include <stdio.h>

#include <stdlib.h>
 
void generateRandom(int n, int* v) {

    for (int i = 0; i < n; i++) {
        v[i] = rand() % 100;
    }
}
 
int dotProd(int n, int* a, int* b) {
    int c=0;
    for (int i = 0; i < n; i++) {
        c += a[i] * b[i];
    }
    return c;
}
 
int compare(int a, int b) {
    return a == b;
}
 
int main(int argc, char** argv) {
    int n = 100;
    int* a = (int*)malloc(n * sizeof(int));
    int* b = (int*)malloc(n * sizeof(int));

    int c = 0;
    int cp = 0;
 
    MPI_Init(&argc, &argv);
 
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
 
    int chunkSize = n / size;
    int* chunkA = (int*)malloc(chunkSize * sizeof(int));
    int* chunkB = (int*)malloc(chunkSize * sizeof(int));
    int chunkC = 0;

    double startTime2;
 
    if (rank == 0) {
        generateRandom(n, a);
        generateRandom(n, b);
 
        double startTimeSeq = MPI_Wtime();
 
        c = dotProd(n, a, b);
 
        double endTimeSeq = MPI_Wtime();
        double duration = endTimeSeq - startTimeSeq;

        printf("%f\n", duration);
 
        startTime2 = MPI_Wtime();
    }
 
    MPI_Scatter(a, chunkSize, MPI_INT, chunkA, chunkSize, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Scatter(b, chunkSize, MPI_INT, chunkB, chunkSize, MPI_INT, 0, MPI_COMM_WORLD);
 
    for (int i = 0; i < chunkSize; i++) {
        chunkC += chunkA[i] * chunkB[i];
    }

    if (rank != 0) {
        MPI_Send(&chunkC, chunkSize, MPI_INT, 0, 3, MPI_COMM_WORLD);
    }
  
    if (rank == 0) {
        MPI_Request arrRequests[size - 1];
        int arrChunk[size-1];
        int flags[size-1];
        

        cp += chunkC;
        for (int i = 1; i < size; i ++) {
            MPI_Irecv(&arrChunk[i-1], 1, MPI_INT, i, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        int numRequests = size - 1;
        while (numRequests > 0) {
            for (int i = 1; i < size; i ++) {
                if(!flags[i-1]) {
                    MPI_Test(&arrRequests[i-1], &flags[i-1], MPI_STATUS_IGNORE);
                    if(flags[i-1]) {
                        numRequests --;
                        cp += arrChunk[i-1];
                    }
                }

            }
        }

        
        double endTime2 = MPI_Wtime();
        double duration = endTime2 - startTime2;
        printf("%f\n", duration);
        printf("%d\n", compare(c, cp));
    }
 
    free(a);
    free(b);
    free(chunkA);
    free(chunkB);

    MPI_Finalize();
    return 0;

}
 
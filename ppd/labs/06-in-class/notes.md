var 1

```c
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

int* generateRandom(int n) {
    int *v = (int*) malloc(n * sizeof(int));

    for (int i = 0; i < n; i ++) {
        v[i] = rand() % 100;
    }

    return v;
}

int* elementWiseProd(int n, int* a, int* b) {
    int* c = (int*) malloc(n * sizeof(int));
    for (int i = 0; i < n; i ++) {
        c[i] = a[i]*b[i];
    }
    return c;
}

int compare(int n, int* a, int* b) {
    for (int i = 0; i < n; i ++) {
        if(a[i]!=b[i]) {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv) {
    int* a = NULL;
    int* b = NULL;
    int* c = NULL;
    int * cp = NULL;
    
    int n = 1000;
    
    int rank, process_count, chunkSize;


    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &process_count);

    chunkSize = n / (process_count - 1);

    if (rank == 0) {
        a = generateRandom(n);
        b = generateRandom(n);
        double startTimeSeq = MPI_Wtime();
        
        c = elementWiseProd(n, a, b);

        double endTimeSeq = MPI_Wtime();
        double duration = endTimeSeq - startTimeSeq;
        printf("%f\n", duration);

        cp = (int*) malloc(n * sizeof(int));

        double startTimePar = MPI_Wtime();
        
        for (int i = 1; i < process_count; i ++) {
            MPI_Send(a + chunkSize*(i-1), chunkSize, MPI_INT, i, 1, MPI_COMM_WORLD);
        }

        for (int i = 1; i < process_count; i ++) {
            MPI_Send(b + chunkSize*(i-1), chunkSize, MPI_INT, i, 2, MPI_COMM_WORLD);
        }

        for (int i = 1; i < process_count; i ++) {
            MPI_Recv(cp + chunkSize*(i-1), chunkSize, MPI_INT, i, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        double endTimePar = MPI_Wtime();
        double durationPar = endTimePar - startTimePar;
        printf("%f\n", durationPar);
        printf("%d\n", compare(n, c, cp));

        free(a);
        free(b);
        free(c);
    } else {
        int* chunkA = (int*) malloc(chunkSize*sizeof(int));
        int* chunkB = (int*) malloc(chunkSize*sizeof(int));
        int* chunkC = (int*) malloc(chunkSize*sizeof(int));

        MPI_Recv(chunkA, chunkSize, MPI_INT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(chunkB, chunkSize, MPI_INT, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        for (int i = 0; i < chunkSize; i ++) {
            chunkC[i] = chunkA[i] * chunkB[i];
        }

        MPI_Send(chunkC, chunkSize, MPI_INT, 0, 3, MPI_COMM_WORLD);
        free(chunkA);
        free(chunkB);
        free(chunkC);
    }


    MPI_Finalize();
    return 0;
}


```


var 2 
```c
#include <mpi.h>

#include <stdio.h>

#include <stdlib.h>
 
void generateRandom(int n, int* v) {

    for (int i = 0; i < n; i++) {

        v[i] = rand() % 100;

    }

}
 
void elementwiseProd(int n, int* a, int* b, int* c) {

    for (int i = 0; i < n; i++) {

        c[i] = a[i] * b[i];

    }

}
 
int compare(int n, int* a, int* b) {

    for (int i = 0; i < n; i++) {

        if (a[i] != b[i]) {

            return 0;

        }

    }

    return 1;

}
 
int main(int argc, char** argv) {

    int n = 100;

    int* a = (int*)malloc(n * sizeof(int));

    int* b = (int*)malloc(n * sizeof(int));

    int* c = (int*)malloc(n * sizeof(int));

    int* cp = (int*)malloc(n * sizeof(int));
 
    MPI_Init(&argc, &argv);
 
    int rank, size;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    MPI_Comm_size(MPI_COMM_WORLD, &size);
 
    int chunkSize = n / (size - 1);
 
    int* chunkA = (int*)malloc(chunkSize * sizeof(int));

    int* chunkB = (int*)malloc(chunkSize * sizeof(int));

    int* chunkC = (int*)malloc(chunkSize * sizeof(int));
 
    double startTime2;
 
    if (rank == 0) {

        generateRandom(n, a);

        generateRandom(n, b);
 
        double startTimeSeq = MPI_Wtime();
 
        elementwiseProd(n, a, b, c);
 
        double endTimeSeq = MPI_Wtime();

        double duration = endTimeSeq - startTimeSeq;

        printf("%f\n", duration);
 
        startTime2 = MPI_Wtime();

    }
 
    MPI_Scatter(a, chunkSize, MPI_INT, chunkA, chunkSize, MPI_INT, 0, MPI_COMM_WORLD);
 
    MPI_Scatter(b, chunkSize, MPI_INT, chunkB, chunkSize, MPI_INT, 0, MPI_COMM_WORLD);
 
    for (int i = 0; i < chunkSize; i++) {

        chunkC[i] = chunkA[i] * chunkB[i];

    }
 
    MPI_Gather(chunkC, chunkSize, MPI_INT, cp, chunkSize, MPI_INT, 0, MPI_COMM_WORLD);
 
    if (rank == 0) {

        double endTime2 = MPI_Wtime();

        double duration = endTime2 - startTime2;

        printf("%f\n", duration);
 
        printf("%d\n", compare(n, c, cp));

    }
 
    free(a);

    free(b);

    free(c);

    free(chunkA);

    free(chunkB);

    free(chunkC);
 
    MPI_Finalize();

    return 0;

}
```

var 3 
```c

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
        cp += chunkC;
        for (int i = 1; i < size; i ++) {
            MPI_Recv(&chunkC, 1, MPI_INT, 0, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            cp += chunkC;
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
 

```

var 4

```c
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
```

var 5
```c
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
    if (a!=b) return 0;
    return 1;
}

int main(int argc, char** argv) {
    int n = 10000000;
    int* a = (int*)malloc(n * sizeof(int));
    int* b = (int*)malloc(n * sizeof(int));
    int c=0,cp=0;

    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int chunkSize = n / size;

    int* chunkA = (int*)malloc(chunkSize * sizeof(int));
    int* chunkB = (int*)malloc(chunkSize * sizeof(int));
    int chunkC=0;

    double startTime2;

    if (rank == 0) {
        generateRandom(n, a);
        generateRandom(n, b);

        double startTimeSeq = MPI_Wtime();

        c=dotProd(n, a, b);

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

    MPI_Reduce(&chunkC, &cp, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);


    if (rank == 0) {
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
```
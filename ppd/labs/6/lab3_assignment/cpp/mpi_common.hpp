#ifndef LAB3_MPI_COMMON_HPP
#define LAB3_MPI_COMMON_HPP

#include "common.hpp"
#include <mpi.h>

enum MessageTag { FIRST = 1, SECOND, CARRY, RESULT, FINAL_CARRY };

inline void send_blocking(const void *buffer, int count, MPI_Datatype type,
                          int destination, int tag) {
#ifdef SYNCHRONOUS_SEND
    MPI_Ssend(buffer, count, type, destination, tag, MPI_COMM_WORLD);
#else
    MPI_Send(buffer, count, type, destination, tag, MPI_COMM_WORLD);
#endif
}

typedef void (*MpiAddition)(const Paths &, int, int);

inline int run_mpi(int argc, char **argv, MpiAddition addition, int minimum_processes) {
    MPI_Init(&argc, &argv);
    int rank = 0, processes = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &processes);
    try {
        Paths paths = parse_paths(argc, argv);
        if (processes < minimum_processes || processes > MAX_PROCESSES)
            throw std::runtime_error("process count outside supported limits");
        MPI_Barrier(MPI_COMM_WORLD);
        double start = MPI_Wtime();
        addition(paths, rank, processes);
        MPI_Barrier(MPI_COMM_WORLD);
        double elapsed = (MPI_Wtime() - start) * 1000.0;
        double total = 0.0;
        MPI_Reduce(&elapsed, &total, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        if (rank == 0) print_time(total);
    } catch (const std::exception &error) {
        std::cerr << "rank " << rank << ": " << error.what() << '\n';
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }
    MPI_Finalize();
    return 0;
}

#endif

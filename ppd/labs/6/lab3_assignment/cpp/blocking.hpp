#ifndef LAB3_BLOCKING_HPP
#define LAB3_BLOCKING_HPP

#include "mpi_common.hpp"

inline void distribute_blocking(std::ifstream &input1, std::ifstream &input2,
                                int length1, int length2, int length, int workers) {
    unsigned char first[CAPACITY], second[CAPACITY];
    for (int worker = 1; worker <= workers; ++worker) {
        int count = chunk_count(length, workers, worker - 1);
        int offset = chunk_offset(length, workers, worker - 1);
        read_digits(input1, length1, offset, first, count);
        read_digits(input2, length2, offset, second, count);
        send_blocking(first, count, MPI_UNSIGNED_CHAR, worker, FIRST);
        send_blocking(second, count, MPI_UNSIGNED_CHAR, worker, SECOND);
    }
}

inline void collect_blocking(const char *path, int length, int workers) {
    unsigned char result[CAPACITY];
    std::ofstream output;
    open_output(output, path, length);
    for (int worker = 1; worker <= workers; ++worker) {
        int count = chunk_count(length, workers, worker - 1);
        MPI_Recv(result, count, MPI_UNSIGNED_CHAR, worker, RESULT,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        write_segment(output, result, count);
    }
    int carry = 0;
    MPI_Recv(&carry, 1, MPI_INT, workers, FINAL_CARRY, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if (carry) {
        unsigned char digit = static_cast<unsigned char>(carry);
        write_segment(output, &digit, 1);
    }
    finish_output(output, length + carry);
}

inline void add_worker(int rank, int workers, int count, bool speculative) {
    unsigned char first[CAPACITY], second[CAPACITY], result[CAPACITY];
    MPI_Recv(first, count, MPI_UNSIGNED_CHAR, 0, FIRST, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(second, count, MPI_UNSIGNED_CHAR, 0, SECOND, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    int outgoing = speculative ? add_digits(first, second, result, count, 0) : 0;
    int incoming = 0;
    if (rank > 1)
        MPI_Recv(&incoming, 1, MPI_INT, rank - 1, CARRY, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if (speculative) {
        if (incoming) outgoing |= increment_digits(result, count);
    } else {
        outgoing = add_digits(first, second, result, count, incoming);
    }
    if (rank < workers) send_blocking(&outgoing, 1, MPI_INT, rank + 1, CARRY);
    send_blocking(result, count, MPI_UNSIGNED_CHAR, 0, RESULT);
    if (rank == workers) send_blocking(&outgoing, 1, MPI_INT, 0, FINAL_CARRY);
}

inline void add_blocking(const Paths &paths, int rank, int processes, bool speculative) {
    std::ifstream input1, input2;
    int length1 = 0, length2 = 0, length = 0;
    if (rank == 0) {
        length1 = open_input(input1, paths.first);
        length2 = open_input(input2, paths.second);
        length = std::max(length1, length2);
    }
    MPI_Bcast(&length, 1, MPI_INT, 0, MPI_COMM_WORLD);
    int workers = processes - 1;
    if (rank == 0) {
        distribute_blocking(input1, input2, length1, length2, length, workers);
        collect_blocking(paths.output, length, workers);
    } else {
        add_worker(rank, workers, chunk_count(length, workers, rank - 1), speculative);
    }
}

#endif

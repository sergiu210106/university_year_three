#include "mpi_common.hpp"

void master(const Paths &paths, int processes, int length, int length1, int length2,
            std::ifstream &input1, std::ifstream &input2) {
    unsigned char first[CAPACITY], second[CAPACITY], result[CAPACITY];
    MPI_Request sends[2 * (MAX_PROCESSES - 1)];
    MPI_Request receives[MAX_PROCESSES];
    int workers = processes - 1;
    int carry = 0;
    MPI_Irecv(&carry, 1, MPI_INT, workers, FINAL_CARRY, MPI_COMM_WORLD, &receives[workers]);
    for (int worker = 1; worker <= workers; ++worker) {
        int count = chunk_count(length, workers, worker - 1);
        int offset = chunk_offset(length, workers, worker - 1);
        read_digits(input1, length1, offset, first + offset, count);
        read_digits(input2, length2, offset, second + offset, count);
        MPI_Irecv(result + offset, count, MPI_UNSIGNED_CHAR, worker, RESULT,
                  MPI_COMM_WORLD, &receives[worker - 1]);
        MPI_Isend(first + offset, count, MPI_UNSIGNED_CHAR, worker, FIRST,
                  MPI_COMM_WORLD, &sends[2 * (worker - 1)]);
        MPI_Isend(second + offset, count, MPI_UNSIGNED_CHAR, worker, SECOND,
                  MPI_COMM_WORLD, &sends[2 * (worker - 1) + 1]);
    }
    MPI_Waitall(2 * workers, sends, MPI_STATUSES_IGNORE);
    MPI_Waitall(workers + 1, receives, MPI_STATUSES_IGNORE);
    if (carry) result[length++] = static_cast<unsigned char>(carry);
    write_output(paths.output, result, length);
}

void worker(int rank, int workers, int count) {
    unsigned char first[CAPACITY], second[CAPACITY], result[CAPACITY];
    MPI_Request receives[3], sends[2];
    int incoming = 0;
    MPI_Irecv(first, count, MPI_UNSIGNED_CHAR, 0, FIRST, MPI_COMM_WORLD, &receives[0]);
    MPI_Irecv(second, count, MPI_UNSIGNED_CHAR, 0, SECOND, MPI_COMM_WORLD, &receives[1]);
    int receive_count = 2;
    if (rank > 1) {
        MPI_Irecv(&incoming, 1, MPI_INT, rank - 1, CARRY, MPI_COMM_WORLD, &receives[2]);
        receive_count = 3;
    }
    MPI_Waitall(receive_count, receives, MPI_STATUSES_IGNORE);
    int outgoing = add_digits(first, second, result, count, incoming);
    int destination = rank < workers ? rank + 1 : 0;
    int tag = rank < workers ? CARRY : FINAL_CARRY;
    MPI_Isend(&outgoing, 1, MPI_INT, destination, tag, MPI_COMM_WORLD, &sends[0]);
    MPI_Isend(result, count, MPI_UNSIGNED_CHAR, 0, RESULT, MPI_COMM_WORLD, &sends[1]);
    MPI_Waitall(2, sends, MPI_STATUSES_IGNORE);
}

void addition(const Paths &paths, int rank, int processes) {
    std::ifstream input1, input2;
    int length1 = 0, length2 = 0, length = 0;
    if (rank == 0) {
        length1 = open_input(input1, paths.first);
        length2 = open_input(input2, paths.second);
        length = std::max(length1, length2);
    }
    MPI_Bcast(&length, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank == 0)
        master(paths, processes, length, length1, length2, input1, input2);
    else
        worker(rank, processes - 1, chunk_count(length, processes - 1, rank - 1));
}

int main(int argc, char **argv) {
    return run_mpi(argc, argv, addition, 2);
}

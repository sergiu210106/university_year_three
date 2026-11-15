#include "mpi_common.hpp"

void addition(const Paths &paths, int rank, int processes) {
    unsigned char first[CAPACITY], second[CAPACITY], result[CAPACITY];
    unsigned char local_first[CAPACITY], local_second[CAPACITY], local_result[CAPACITY];
    int length = 0;
    if (rank == 0) {
        std::ifstream input1, input2;
        int length1 = open_input(input1, paths.first);
        int length2 = open_input(input2, paths.second);
        length = std::max(length1, length2);
        int padded = ((length + processes - 1) / processes) * processes;
        if (padded + 1 > CAPACITY) throw std::runtime_error("collective padding exceeds capacity");
        read_digits(input1, length1, 0, first, padded);
        read_digits(input2, length2, 0, second, padded);
    }
    MPI_Bcast(&length, 1, MPI_INT, 0, MPI_COMM_WORLD);
    int count = (length + processes - 1) / processes;
    int padded = count * processes;
    MPI_Scatter(first, count, MPI_UNSIGNED_CHAR, local_first, count,
                MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
    MPI_Scatter(second, count, MPI_UNSIGNED_CHAR, local_second, count,
                MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
    int carry = 0;
    if (rank > 0)
        MPI_Recv(&carry, 1, MPI_INT, rank - 1, CARRY, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    carry = add_digits(local_first, local_second, local_result, count, carry);
    if (rank + 1 < processes) send_blocking(&carry, 1, MPI_INT, rank + 1, CARRY);
    MPI_Gather(local_result, count, MPI_UNSIGNED_CHAR, result, count,
               MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
    MPI_Bcast(&carry, 1, MPI_INT, processes - 1, MPI_COMM_WORLD);
    if (rank == 0) {
        // A carry into distribution padding is already stored at the true width.
        int final_carry = padded > length ? result[length] : carry;
        if (final_carry) result[length++] = static_cast<unsigned char>(final_carry);
        write_output(paths.output, result, length);
    }
}

int main(int argc, char **argv) {
    return run_mpi(argc, argv, addition, 1);
}

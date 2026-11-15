#include "blocking.hpp"

void addition(const Paths &paths, int rank, int processes) {
    add_blocking(paths, rank, processes, false);
}

int main(int argc, char **argv) {
    return run_mpi(argc, argv, addition, 2);
}

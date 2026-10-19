#include "conv.hpp"

// usage: seq <matrix_f_file> <convolution_file> <output_file>
// prints "<compute_ms> <total_ms>" to stdout
int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s <matrix_f_file> <convolution_file> <output_file>\n", argv[0]);
        return 1;
    }
    auto program_start = Clock::now();

    read_input(argv[1], argv[2]);

    auto compute_start = Clock::now();
    compute_rect(0, n, 0, m);
    auto compute_end = Clock::now();

    write_output(argv[3]);
    free_matrices();

    auto program_end = Clock::now();
    print_times(program_start, compute_start, compute_end, program_end);
    return 0;
}

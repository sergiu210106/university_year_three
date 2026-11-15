#include "conv.hpp"

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }
    try {
        auto program_start = Clock::now();
        read_input(argv[1]);
        auto compute_start = Clock::now();
        RowBuffers buffers(0, rows, false);
        compute_range(0, rows, buffers);
        auto compute_end = Clock::now();
        write_output(argv[2]);
        print_times(program_start, compute_start, compute_end);
    } catch (const exception &error) {
        fprintf(stderr, "error: %s\n", error.what());
        return 1;
    }
}

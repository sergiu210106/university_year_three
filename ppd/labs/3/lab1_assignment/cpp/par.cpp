#include <thread>
#include <vector>

#include "conv.hpp"

// usage: par <matrix_f_file> <convolution_file> <output_file> <threads> <variant>
//   variant: horizontal | vertical | block | linear | cyclic
// prints "<compute_ms> <total_ms>" to stdout
int main(int argc, char **argv) {
    if (argc < 6) {
        fprintf(stderr, "usage: %s <matrix_f_file> <convolution_file> <output_file> <threads> "
                        "<horizontal|vertical|block|linear|cyclic>\n", argv[0]);
        return 1;
    }
    auto program_start = Clock::now();
    int p = stoi(argv[4]);
    if (p < 1) fail("threads must be >= 1");
    Variant variant = parse_variant(argv[5]);

    read_input(argv[1], argv[2]);

    auto compute_start = Clock::now();

    vector<thread> threads;
    for (int t = 0; t < p; t ++)
        threads.emplace_back(thread_work, variant, t, p);
    for (auto &th : threads) th.join();

    auto compute_end = Clock::now();

    write_output(argv[3]);
    free_matrices();

    auto program_end = Clock::now();
    print_times(program_start, compute_start, compute_end, program_end);
    return 0;
}

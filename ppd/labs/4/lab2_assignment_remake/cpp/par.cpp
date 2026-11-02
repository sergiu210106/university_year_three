#include <barrier>
#include <thread>
#include "conv.hpp"

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <input_file> <output_file> <threads>\n", argv[0]);
        return 1;
    }
    try {
        auto program_start = Clock::now();
        int requested_threads = parse_threads(argv[3]);
        read_input(argv[1]);
        auto compute_start = Clock::now();
        int count = min(requested_threads, rows);
        barrier<> snapshots_ready(count);
        vector<thread> workers;
        workers.reserve(count);
        try {
            for (int t = 0; t < count; ++t) {
                int start = static_cast<long long>(t) * rows / count;
                int end = static_cast<long long>(t + 1) * rows / count;
                workers.emplace_back([start, end, &snapshots_ready] {
                    // Variant 1b: each worker saves its original neighbours first.
                    RowBuffers buffers(start, end, true);
                    // Nobody may overwrite the image until ALL snapshots are ready.
                    snapshots_ready.arrive_and_wait();
                    compute_range(start, end, buffers);
                });
            }
        } catch (...) {
            // A worker that could not start must not leave others waiting forever.
            for (size_t missing = workers.size(); missing < static_cast<size_t>(count); ++missing)
                snapshots_ready.arrive_and_drop();
            for (auto &worker : workers) worker.join();
            throw;
        }
        for (auto &worker : workers) worker.join();
        auto compute_end = Clock::now();
        write_output(argv[2]);
        print_times(program_start, compute_start, compute_end);
    } catch (const exception &error) {
        fprintf(stderr, "error: %s\n", error.what());
        return 1;
    }
}

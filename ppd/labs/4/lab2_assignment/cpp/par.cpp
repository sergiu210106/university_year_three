#include <functional>
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
        vector<RowBuffers> buffers;
        buffers.reserve(count);
        // Complete ALL snapshots before starting ANY worker: no barrier is needed.
        for (int t = 0; t < count; ++t) {
            int start = static_cast<long long>(t) * rows / count;
            int end = static_cast<long long>(t + 1) * rows / count;
            buffers.emplace_back(start, end, true);
        }
        vector<thread> workers;
        workers.reserve(count);
        try {
            for (int t = 0; t < count; ++t) {
                int start = static_cast<long long>(t) * rows / count;
                int end = static_cast<long long>(t + 1) * rows / count;
                workers.emplace_back(compute_range, start, end, ref(buffers[t]));
            }
        } catch (...) {
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

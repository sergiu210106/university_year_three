#include "common.hpp"
#include <chrono>

int main(int argc, char **argv) {
    try {
        Paths paths = parse_paths(argc, argv);
        auto start = std::chrono::high_resolution_clock::now();
        unsigned char first[CAPACITY], second[CAPACITY], result[CAPACITY];
        std::ifstream input1, input2;
        int length1 = open_input(input1, paths.first);
        int length2 = open_input(input2, paths.second);
        int length = std::max(length1, length2);
        read_digits(input1, length1, 0, first, length);
        read_digits(input2, length2, 0, second, length);
        int carry = add_digits(first, second, result, length, 0);
        if (carry) result[length++] = static_cast<unsigned char>(carry);
        write_output(paths.output, result, length);
        auto end = std::chrono::high_resolution_clock::now();
        print_time(std::chrono::duration<double, std::milli>(end - start).count());
    } catch (const std::exception &error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}

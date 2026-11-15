#ifndef LAB3_COMMON_HPP
#define LAB3_COMMON_HPP

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

const int MAX_DIGITS = 100000;
const int MAX_PROCESSES = 17;
const int CAPACITY = MAX_DIGITS + MAX_PROCESSES;
const int HEADER_WIDTH = 6;

struct Paths {
    const char *first;
    const char *second;
    const char *output;
};

inline Paths parse_paths(int argc, char **argv) {
    if (argc > 4) throw std::runtime_error("usage: program [input1 [input2 [output]]]");
    return {argc > 1 ? argv[1] : "Numar1.txt",
            argc > 2 ? argv[2] : "Numar2.txt",
            argc > 3 ? argv[3] : "Numar3.txt"};
}

inline int open_input(std::ifstream &input, const char *path) {
    input.open(path);
    int length = 0;
    if (!(input >> length) || length < 1 || length > MAX_DIGITS)
        throw std::runtime_error("cannot read input length (expected 1..100000)");
    return length;
}

// Read a slice in LSD order, padding beyond this operand's declared length.
inline void read_digits(std::istream &input, int length, int offset,
                        unsigned char digits[], int count) {
    for (int i = 0; i < count; ++i) {
        int digit = 0;
        if (offset + i < length && (!(input >> digit) || digit < 0 || digit > 9))
            throw std::runtime_error("expected a decimal digit");
        digits[i] = static_cast<unsigned char>(digit);
    }
}

inline int add_digits(const unsigned char first[], const unsigned char second[],
                      unsigned char result[], int count, int carry) {
    for (int i = 0; i < count; ++i) {
        int sum = first[i] + second[i] + carry;
        result[i] = static_cast<unsigned char>(sum % 10);
        carry = sum / 10;
    }
    return carry;
}

// Return the overflow from adding one to an already computed slice.
inline int increment_digits(unsigned char digits[], int count) {
    for (int i = 0; i < count; ++i) {
        if (digits[i] < 9) {
            ++digits[i];
            return 0;
        }
        digits[i] = 0;
    }
    return 1;
}

inline int chunk_count(int length, int parts, int index) {
    return length / parts + (index < length % parts ? 1 : 0);
}

inline int chunk_offset(int length, int parts, int index) {
    return index * (length / parts) + std::min(index, length % parts);
}

inline void open_output(std::ofstream &output, const char *path, int length) {
    output.open(path, std::ios::out | std::ios::trunc);
    if (!output) throw std::runtime_error("cannot open output file");
    output << std::setw(HEADER_WIDTH) << length << '\n';
}

inline void write_segment(std::ostream &output, const unsigned char digits[], int count) {
    for (int i = 0; i < count; ++i) output << static_cast<int>(digits[i]) << ' ';
    if (!output) throw std::runtime_error("cannot write output digits");
}

inline void finish_output(std::ofstream &output, int length) {
    output << '\n';
    output.seekp(0);
    output << std::setw(HEADER_WIDTH) << length << '\n';
    output.close();
    if (!output) throw std::runtime_error("cannot finish output file");
}

inline void write_output(const char *path, const unsigned char digits[], int count) {
    std::ofstream output;
    open_output(output, path, count);
    write_segment(output, digits, count);
    finish_output(output, count);
}

inline void print_time(double milliseconds) {
    std::cout << std::fixed << std::setprecision(6) << milliseconds << '\n';
}

#endif

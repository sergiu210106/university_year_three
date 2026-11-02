// Shared image, buffered I/O and in-place convolution for Seq and Par.
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;
using Clock = chrono::high_resolution_clock;

int rows, cols;
vector<vector<int>> image; // The only image-sized matrix; vector storage is dynamic.
int kernel[3][3];

[[noreturn]] void fail(const string &message) {
    throw runtime_error(message);
}

// Buffered reader for whitespace-separated integers (much faster than ifstream >>,
// which matters for the 10^8 values of a 10000 x 10000 matrix).
class Reader {
    FILE *fp;
    char buf[1 << 16];
    size_t len = 0, pos = 0;

    int get() {
        if (pos == len) {
            len = fread(buf, 1, sizeof buf, fp);
            pos = 0;
            if (len == 0) return EOF;
        }
        return buf[pos ++];
    }

public:
    explicit Reader(const char *path) : fp(fopen(path, "rb")) {
        if (!fp) fail(string("cannot open ") + path);
    }
    ~Reader() { fclose(fp); }

    int next_int() {
        int ch = get();
        while (ch != EOF && ch <= ' ') ch = get();
        if (ch == EOF) fail("unexpected end of input file");
        bool negative = ch == '-';
        if (negative) ch = get();
        if (ch < '0' || ch > '9') fail("expected an integer");
        long long value = 0;
        while (ch >= '0' && ch <= '9') {
            value = value * 10 + ch - '0';
            if (value > static_cast<long long>(INT_MAX) + negative)
                fail("integer outside 32-bit range");
            ch = get();
        }
        if (ch != EOF && ch > ' ') fail("invalid integer");
        return static_cast<int>(negative ? -value : value);
    }
};

// Buffered writer for integers.
class Writer {
    FILE *fp;
    char buf[1 << 16];
    size_t len = 0;

public:
    explicit Writer(const char *path) : fp(fopen(path, "wb")) {
        if (!fp) fail(string("cannot open ") + path);
    }
    ~Writer() { fclose(fp); }

    void flush() {
        if (fwrite(buf, 1, len, fp) != len || fflush(fp) != 0)
            fail("cannot write output file");
        len = 0;
    }

    void put_char(char ch) {
        if (len == sizeof buf) flush();
        buf[len ++] = ch;
    }

    void put_int(int x) {
        if (len + 12 > sizeof buf) flush();
        unsigned u = x;
        if (x < 0) {
            buf[len ++] = '-';
            u = 0u - u;
        }
        char tmp[10];
        int d = 0;
        do {
            tmp[d ++] = char('0' + u % 10);
            u /= 10;
        } while (u);
        while (d) buf[len ++] = tmp[-- d];
    }
};

// date.txt: rows cols, image values, kernel size (3), kernel values.
void read_input(const char *path) {
    Reader input(path);
    rows = input.next_int();
    cols = input.next_int();
    if (rows < 1 || cols < 1) fail("rows and columns must be positive");
    image.resize(rows);
    for (auto &row : image) {
        row.resize(cols);
        for (int &value : row) value = input.next_int();
    }
    if (input.next_int() != 3) fail("kernel size must be 3");
    for (auto &row : kernel)
        for (int &value : row) value = input.next_int();
}

void write_output(const char *path) {
    Writer output(path);
    output.put_int(rows);
    output.put_char(' ');
    output.put_int(cols);
    output.put_char('\n');
    for (const auto &row : image) {
        for (int j = 0; j < cols; ++j) {
            if (j) output.put_char(' ');
            output.put_int(row[j]);
        }
        output.put_char('\n');
    }
    output.flush();
}

struct RowBuffers {
    vector<int> previous, current, following;

    RowBuffers(int start, int end, bool parallel)
        : previous(image[max(0, start - 1)]), current(image[start]) {
        // Preserve the neighbouring row; every worker copies before the barrier opens.
        if (parallel) following = image[min(end, rows - 1)];
    }
};

// Read only original values from the supplied rows; write directly into the image.
void compute_row(int i, const vector<int> &previous, const vector<int> &current,
                 const vector<int> &next) {
    for (int j = 0; j < cols; ++j) {
        int sum = 0;
        for (int offset = -1; offset <= 1; ++offset) {
            int column = max(0, min(cols - 1, j + offset));
            sum += previous[column] * kernel[0][offset + 1];
            sum += current[column]  * kernel[1][offset + 1];
            sum += next[column]     * kernel[2][offset + 1];
        }
        image[i][j] = sum;
    }
}

// One worker owns [start, end). There is no shared write inside this range.
void compute_range(int start, int end, RowBuffers &buffers) {
    for (int i = start; i < end; ++i) {
        const vector<int> *next;
        if (i == rows - 1) next = &buffers.current;
        else if (i == end - 1 && !buffers.following.empty()) next = &buffers.following;
        else next = &image[i + 1];
        compute_row(i, buffers.previous, buffers.current, *next);
        if (i + 1 < end) {
            buffers.previous.swap(buffers.current);
            // This row is still original: only this worker can overwrite it.
            copy(image[i + 1].begin(), image[i + 1].end(), buffers.current.begin());
        }
    }
}

int parse_threads(const string &text) {
    size_t consumed;
    int count = stoi(text, &consumed);
    if (consumed != text.size() || count < 1) fail("threads must be a positive integer");
    return count;
}

void print_times(Clock::time_point program_start, Clock::time_point compute_start,
                 Clock::time_point compute_end) {
    double compute_ms = chrono::duration<double, milli>(compute_end - compute_start).count();
    double total_ms = chrono::duration<double, milli>(Clock::now() - program_start).count();
    printf("%.6f %.6f\n", compute_ms, total_ms);
}

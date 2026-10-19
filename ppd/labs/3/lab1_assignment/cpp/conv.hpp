// Shared code for seq.cpp and par.cpp: matrices, fast file I/O, the convolution kernel
// and the work done by one thread for every distribution variant.
//
// Compiled with -DSTATIC_ALLOC: the matrices are global arrays  int f[MAX][MAX].
// Compiled without it:          the matrices are allocated with new (int** + one new per row).
// Both are indexed as f[i][j], so the rest of the code is the same for the two variants.
#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace std;
using Clock = chrono::steady_clock;

const int KMAX = 15;   // largest supported convolution matrix

int n, m, k;           // F and V are n x m, C is k x k

#ifdef STATIC_ALLOC
const int MAX = 10000; // largest supported n and m
int f[MAX][MAX], v[MAX][MAX], c[KMAX][KMAX];
#else
int **f, **v, **c;

int **alloc_matrix(int rows, int cols) {
    int **a = new int *[rows];
    for (int i = 0; i < rows; i ++) a[i] = new int[cols];
    return a;
}

void free_matrix(int **a, int rows) {
    for (int i = 0; i < rows; i ++) delete[] a[i];
    delete[] a;
}
#endif

[[noreturn]] void fail(const string &msg) {
    fprintf(stderr, "error: %s\n", msg.c_str());
    exit(1);
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
        while (ch != '-' && (ch < '0' || ch > '9')) {
            if (ch == EOF) fail("unexpected end of input file");
            ch = get();
        }
        bool neg = ch == '-';
        if (neg) ch = get();
        int x = 0;
        while (ch >= '0' && ch <= '9') {
            x = x * 10 + (ch - '0');
            ch = get();
        }
        return neg ? -x : x;
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
    ~Writer() {
        flush();
        fclose(fp);
    }

    void flush() {
        fwrite(buf, 1, len, fp);
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

// MatrixF file: "n m", then n rows of m integers.
// ConvolutionM file: "k", then k rows of k integers (k odd).
void read_input(const char *f_path, const char *c_path) {
    Reader rf(f_path);
    n = rf.next_int();
    m = rf.next_int();
    if (n < 1 || m < 1) fail("n and m must be >= 1");
#ifdef STATIC_ALLOC
    if (n > MAX || m > MAX) fail("n and m must be <= " + to_string(MAX) + " (static allocation)");
#else
    f = alloc_matrix(n, m);
    v = alloc_matrix(n, m);
#endif
    for (int i = 0; i < n; i ++)
        for (int j = 0; j < m; j ++)
            f[i][j] = rf.next_int();

    Reader rc(c_path);
    k = rc.next_int();
    if (k < 1 || k % 2 == 0 || k > KMAX) fail("k must be odd and between 1 and " + to_string(KMAX));
#ifndef STATIC_ALLOC
    c = alloc_matrix(k, k);
#endif
    for (int i = 0; i < k; i ++)
        for (int j = 0; j < k; j ++)
            c[i][j] = rc.next_int();
}

// Output file: "n m", then the n rows of V, values separated by one space.
void write_output(const char *path) {
    Writer w(path);
    w.put_int(n);
    w.put_char(' ');
    w.put_int(m);
    w.put_char('\n');
    for (int i = 0; i < n; i ++) {
        for (int j = 0; j < m; j ++) {
            if (j) w.put_char(' ');
            w.put_int(v[i][j]);
        }
        w.put_char('\n');
    }
}

void free_matrices() {
#ifndef STATIC_ALLOC
    free_matrix(f, n);
    free_matrix(v, n);
    free_matrix(c, k);
#endif
}

// v[i][j] = sum over a, b in [0, k) of c[a][b] * f[i + a - r][j + b - r],  r = k / 2.
// F is virtually bordered with its boundary elements: an index outside the matrix is
// replaced by the nearest valid one (f[-1][j] = f[0][j], f[n][m] = f[n-1][m-1], ...).
inline int compute_cell(int i, int j) {
    int r = k / 2, s = 0;
    for (int a = 0; a < k; a ++) {
        const int *row = f[clamp(i + a - r, 0, n - 1)];
        for (int b = 0; b < k; b ++)
            s += c[a][b] * row[clamp(j + b - r, 0, m - 1)];
    }
    return s;
}

// computes V on the submatrix rows [r0, r1) x columns [c0, c1)
void compute_rect(int r0, int r1, int c0, int c1) {
    for (int i = r0; i < r1; i ++)
        for (int j = c0; j < c1; j ++)
            v[i][j] = compute_cell(i, j);
}

// ---- Distribution of the work among p threads (used by par.cpp) ----

enum class Variant { Horizontal, Vertical, Block, Linear, Cyclic };

Variant parse_variant(const string &s) {
    if (s == "horizontal") return Variant::Horizontal;
    if (s == "vertical") return Variant::Vertical;
    if (s == "block") return Variant::Block;
    if (s == "linear") return Variant::Linear;
    if (s == "cyclic") return Variant::Cyclic;
    fail("unknown variant '" + s + "' (horizontal, vertical, block, linear, cyclic)");
}

// Block variant: grid of pr x pc blocks with pr * pc = p, chosen so that the blocks are
// as close to square as possible. On a tie, the grid with more rows of blocks wins.
void block_grid(int p, int &pr, int &pc) {
    double best = -1;
    for (int r = p; r >= 1; r --) {
        if (p % r) continue;
        double diff = fabs((double) n / r - (double) m / (p / r));
        if (best < 0 || diff < best) {
            best = diff;
            pr = r;
            pc = p / r;
        }
    }
}

// Delta distribution functions: the thread that owns element (i, j), with the elements
// numbered row by row (idx = i * m + j, N = n * m).
//   linear: thread t owns the contiguous indices [t * N / p, (t + 1) * N / p)
//   cyclic: thread t owns the indices t, t + p, t + 2p, ...
int delta_linear(int i, int j, int p) {
    long long N = (long long) n * m, idx = (long long) i * m + j;
    // largest t with t * N / p <= idx
    int t = (int) ((idx * p + p) / N);
    while ((long long) t * N / p > idx) t --;
    return t;
}

int delta_cyclic(int i, int j, int p) {
    return (int) (((long long) i * m + j) % p);
}

// computes V for the indices from, from + step, from + 2 * step, ... < to (row by row)
void compute_indices(long long from, long long to, int step, Variant variant, int t, int p) {
    if (from >= to) return;
    int i = (int) (from / m), j = (int) (from % m);
    for (long long idx = from; idx < to; idx += step) {
#ifdef CHECK_DELTA
        int owner = variant == Variant::Linear ? delta_linear(i, j, p) : delta_cyclic(i, j, p);
        if (owner != t) fail("delta mismatch at (" + to_string(i) + ", " + to_string(j) + ")");
#else
        (void) variant, (void) t, (void) p;
#endif
        v[i][j] = compute_cell(i, j);
        j += step;
        while (j >= m) {
            j -= m;
            i ++;
        }
    }
}

// the work of thread t out of p
void thread_work(Variant variant, int t, int p) {
    long long N = (long long) n * m;
    switch (variant) {
    case Variant::Horizontal:
        compute_rect(t * n / p, (t + 1) * n / p, 0, m);
        break;
    case Variant::Vertical:
        compute_rect(0, n, t * m / p, (t + 1) * m / p);
        break;
    case Variant::Block: {
        int pr = 1, pc = 1;
        block_grid(p, pr, pc);
        int br = t / pc, bc = t % pc;
        compute_rect(br * n / pr, (br + 1) * n / pr, bc * m / pc, (bc + 1) * m / pc);
        break;
    }
    case Variant::Linear:
        compute_indices(t * N / p, (t + 1) * N / p, 1, variant, t, p);
        break;
    case Variant::Cyclic:
        compute_indices(t, N, p, variant, t, p);
        break;
    }
}

void print_times(Clock::time_point program_start, Clock::time_point compute_start,
                 Clock::time_point compute_end, Clock::time_point program_end) {
    chrono::duration<double, milli> compute = compute_end - compute_start;
    chrono::duration<double, milli> total = program_end - program_start;
    printf("%.6f %.6f\n", compute.count(), total.count());
}

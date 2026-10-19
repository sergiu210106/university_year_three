import java.io.*;
import java.util.Locale;

// Shared code for Seq and Par: matrices, fast file I/O, the convolution kernel and the
// work done by one thread for every distribution variant.
public class Conv {
    static int n, m, k;    // F and V are n x m, C is k x k
    static int[][] f, v, c;

    // Buffered reader for whitespace-separated integers (Scanner / split would be too slow
    // or use too much memory for the 10^8 values of a 10000 x 10000 matrix).
    static class Reader implements Closeable {
        private final InputStream in;
        private final byte[] buf = new byte[1 << 16];
        private int len = 0, pos = 0;

        Reader(String path) throws IOException {
            in = new FileInputStream(path);
        }

        private int get() throws IOException {
            if (pos == len) {
                len = in.read(buf);
                pos = 0;
                if (len <= 0) {
                    len = 0;
                    return -1;
                }
            }
            return buf[pos++];
        }

        int nextInt() throws IOException {
            int ch = get();
            while (ch != '-' && (ch < '0' || ch > '9')) {
                if (ch == -1) throw new EOFException("unexpected end of input file");
                ch = get();
            }
            boolean neg = ch == '-';
            if (neg) ch = get();
            int x = 0;
            while (ch >= '0' && ch <= '9') {
                x = x * 10 + (ch - '0');
                ch = get();
            }
            return neg ? -x : x;
        }

        @Override
        public void close() throws IOException {
            in.close();
        }
    }

    // Buffered writer for integers.
    static class Writer implements Closeable {
        private final OutputStream out;
        private final byte[] buf = new byte[1 << 16];
        private final byte[] tmp = new byte[11];
        private int len = 0;

        Writer(String path) throws IOException {
            out = new FileOutputStream(path);
        }

        void flush() throws IOException {
            out.write(buf, 0, len);
            len = 0;
        }

        void putChar(char ch) throws IOException {
            if (len == buf.length) flush();
            buf[len++] = (byte) ch;
        }

        void putInt(int x) throws IOException {
            if (len + 12 > buf.length) flush();
            long u = x;
            if (u < 0) {
                buf[len++] = '-';
                u = -u;
            }
            int d = 0;
            do {
                tmp[d++] = (byte) ('0' + u % 10);
                u /= 10;
            } while (u != 0);
            while (d > 0) buf[len++] = tmp[--d];
        }

        @Override
        public void close() throws IOException {
            flush();
            out.close();
        }
    }

    // MatrixF file: "n m", then n rows of m integers.
    // ConvolutionM file: "k", then k rows of k integers (k odd).
    static void readInput(String fPath, String cPath) throws IOException {
        try (Reader rf = new Reader(fPath)) {
            n = rf.nextInt();
            m = rf.nextInt();
            if (n < 1 || m < 1) fail("n and m must be >= 1");
            f = new int[n][m];
            v = new int[n][m];
            for (int i = 0; i < n; i++)
                for (int j = 0; j < m; j++)
                    f[i][j] = rf.nextInt();
        }
        try (Reader rc = new Reader(cPath)) {
            k = rc.nextInt();
            if (k < 1 || k % 2 == 0) fail("k must be odd and >= 1");
            c = new int[k][k];
            for (int i = 0; i < k; i++)
                for (int j = 0; j < k; j++)
                    c[i][j] = rc.nextInt();
        }
    }

    // Output file: "n m", then the n rows of V, values separated by one space.
    static void writeOutput(String path) throws IOException {
        try (Writer w = new Writer(path)) {
            w.putInt(n);
            w.putChar(' ');
            w.putInt(m);
            w.putChar('\n');
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < m; j++) {
                    if (j > 0) w.putChar(' ');
                    w.putInt(v[i][j]);
                }
                w.putChar('\n');
            }
        }
    }

    static void fail(String msg) {
        System.err.println("error: " + msg);
        System.exit(1);
    }

    static int clamp(int x, int lo, int hi) {
        return x < lo ? lo : (x > hi ? hi : x);
    }

    // v[i][j] = sum over a, b in [0, k) of c[a][b] * f[i + a - r][j + b - r],  r = k / 2.
    // F is virtually bordered with its boundary elements: an index outside the matrix is
    // replaced by the nearest valid one (f[-1][j] = f[0][j], f[n][m] = f[n-1][m-1], ...).
    static int computeCell(int i, int j) {
        int r = k / 2, s = 0;
        for (int a = 0; a < k; a++) {
            int[] row = f[clamp(i + a - r, 0, n - 1)];
            int[] ca = c[a];
            for (int b = 0; b < k; b++)
                s += ca[b] * row[clamp(j + b - r, 0, m - 1)];
        }
        return s;
    }

    // computes V on the submatrix rows [r0, r1) x columns [c0, c1)
    static void computeRect(int r0, int r1, int c0, int c1) {
        for (int i = r0; i < r1; i++)
            for (int j = c0; j < c1; j++)
                v[i][j] = computeCell(i, j);
    }

    // ---- Distribution of the work among p threads (used by Par) ----

    enum Variant { HORIZONTAL, VERTICAL, BLOCK, LINEAR, CYCLIC }

    static Variant parseVariant(String s) {
        for (Variant variant : Variant.values())
            if (variant.name().equalsIgnoreCase(s)) return variant;
        fail("unknown variant '" + s + "' (horizontal, vertical, block, linear, cyclic)");
        return null;
    }

    // Block variant: grid of pr x pc blocks with pr * pc = p, chosen so that the blocks are
    // as close to square as possible. On a tie, the grid with more rows of blocks wins.
    // Returns {pr, pc}.
    static int[] blockGrid(int p) {
        int pr = 1;
        double best = -1;
        for (int r = p; r >= 1; r--) {
            if (p % r != 0) continue;
            double diff = Math.abs((double) n / r - (double) m / (p / r));
            if (best < 0 || diff < best) {
                best = diff;
                pr = r;
            }
        }
        return new int[] {pr, p / pr};
    }

    // Delta distribution functions: the thread that owns element (i, j), with the elements
    // numbered row by row (idx = i * m + j, N = n * m).
    //   linear: thread t owns the contiguous indices [t * N / p, (t + 1) * N / p)
    //   cyclic: thread t owns the indices t, t + p, t + 2p, ...
    // The threads enumerate their indices directly (computeIndices); these functions
    // state the distribution and are checked in the tests (Par with -Dconv.checkDelta=true).
    static int deltaLinear(int i, int j, int p) {
        long N = (long) n * m, idx = (long) i * m + j;
        int t = (int) ((idx * p + p) / N);   // largest t with t * N / p <= idx
        while ((long) t * N / p > idx) t--;
        return t;
    }

    static int deltaCyclic(int i, int j, int p) {
        return (int) (((long) i * m + j) % p);
    }

    static final boolean CHECK_DELTA = Boolean.getBoolean("conv.checkDelta");

    // computes V for the indices from, from + step, from + 2 * step, ... < to (row by row)
    static void computeIndices(long from, long to, int step, Variant variant, int t, int p) {
        if (from >= to) return;
        int i = (int) (from / m), j = (int) (from % m);
        for (long idx = from; idx < to; idx += step) {
            if (CHECK_DELTA) {
                int owner = variant == Variant.LINEAR ? deltaLinear(i, j, p) : deltaCyclic(i, j, p);
                if (owner != t) fail("delta mismatch at (" + i + ", " + j + ")");
            }
            v[i][j] = computeCell(i, j);
            j += step;
            while (j >= m) {
                j -= m;
                i++;
            }
        }
    }

    // the work of thread t out of p
    static void threadWork(Variant variant, int t, int p) {
        long N = (long) n * m;
        switch (variant) {
            case HORIZONTAL:
                computeRect(t * n / p, (t + 1) * n / p, 0, m);
                break;
            case VERTICAL:
                computeRect(0, n, t * m / p, (t + 1) * m / p);
                break;
            case BLOCK: {
                int[] grid = blockGrid(p);
                int pr = grid[0], pc = grid[1];
                int br = t / pc, bc = t % pc;
                computeRect(br * n / pr, (br + 1) * n / pr, bc * m / pc, (bc + 1) * m / pc);
                break;
            }
            case LINEAR:
                computeIndices(t * N / p, (t + 1) * N / p, 1, variant, t, p);
                break;
            case CYCLIC:
                computeIndices(t, N, p, variant, t, p);
                break;
        }
    }

    static void printTimes(long programStart, long computeStart, long computeEnd, long programEnd) {
        double computeMs = (computeEnd - computeStart) / 1e6;
        double totalMs = (programEnd - programStart) / 1e6;
        System.out.printf(Locale.ROOT, "%.6f %.6f%n", computeMs, totalMs);
    }
}

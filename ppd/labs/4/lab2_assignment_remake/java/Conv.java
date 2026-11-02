import java.io.*;
import java.util.Locale;

// Shared image, buffered I/O and in-place convolution for Seq and Par.
public class Conv {
    static int rows, cols;
    static int[][] image; // The only image-sized matrix.
    static int[][] kernel = new int[3][3];

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
            while (ch != -1 && ch <= ' ') ch = get();
            if (ch == -1) throw new EOFException("unexpected end of input file");
            boolean negative = ch == '-';
            if (negative) ch = get();
            if (ch < '0' || ch > '9') throw new IOException("expected an integer");
            long value = 0;
            while (ch >= '0' && ch <= '9') {
                value = value * 10 + ch - '0';
                if (value > (long) Integer.MAX_VALUE + (negative ? 1 : 0))
                    throw new IOException("integer outside 32-bit range");
                ch = get();
            }
            if (ch != -1 && ch > ' ') throw new IOException("invalid integer");
            return (int) (negative ? -value : value);
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

    // date.txt: rows cols, image values, kernel size (3), kernel values.
    static void readInput(String path) throws IOException {
        try (Reader input = new Reader(path)) {
            rows = input.nextInt();
            cols = input.nextInt();
            if (rows < 1 || cols < 1)
                throw new IllegalArgumentException("rows and columns must be positive");
            image = new int[rows][cols];
            for (int[] row : image)
                for (int j = 0; j < cols; j++) row[j] = input.nextInt();
            if (input.nextInt() != 3)
                throw new IllegalArgumentException("kernel size must be 3");
            for (int[] row : kernel)
                for (int j = 0; j < 3; j++) row[j] = input.nextInt();
        }
    }

    static void writeOutput(String path) throws IOException {
        try (Writer output = new Writer(path)) {
            output.putInt(rows);
            output.putChar(' ');
            output.putInt(cols);
            output.putChar('\n');
            for (int[] row : image) {
                for (int j = 0; j < cols; j++) {
                    if (j > 0) output.putChar(' ');
                    output.putInt(row[j]);
                }
                output.putChar('\n');
            }
        }
    }

    static class RowBuffers {
        int[] previous, current;
        final int[] following;

        RowBuffers(int start, int end, boolean parallel) {
            previous = image[Math.max(0, start - 1)].clone();
            current = image[start].clone();
            // Preserve the neighbouring row; every worker copies before the barrier opens.
            following = parallel ? image[Math.min(end, rows - 1)].clone() : null;
        }
    }

    static void computeRow(int i, int[] previous, int[] current, int[] next) {
        for (int j = 0; j < cols; j++) {
            int sum = 0;
            for (int offset = -1; offset <= 1; offset++) {
                int column = Math.max(0, Math.min(cols - 1, j + offset));
                sum += previous[column] * kernel[0][offset + 1];
                sum += current[column]  * kernel[1][offset + 1];
                sum += next[column]     * kernel[2][offset + 1];
            }
            image[i][j] = sum;
        }
    }

    // One worker owns [start, end). No other worker writes inside this range.
    static void computeRange(int start, int end, RowBuffers buffers) {
        for (int i = start; i < end; i++) {
            int[] next;
            if (i == rows - 1) next = buffers.current;
            else if (i == end - 1 && buffers.following != null) next = buffers.following;
            else next = image[i + 1];
            computeRow(i, buffers.previous, buffers.current, next);
            if (i + 1 < end) {
                int[] oldPrevious = buffers.previous;
                buffers.previous = buffers.current;
                buffers.current = oldPrevious;
                // Still original: only this worker can overwrite the next row.
                System.arraycopy(image[i + 1], 0, buffers.current, 0, cols);
            }
        }
    }

    static void printTimes(long programStart, long computeStart, long computeEnd) {
        double computeMs = (computeEnd - computeStart) / 1e6;
        double totalMs = (System.nanoTime() - programStart) / 1e6;
        System.out.printf(Locale.ROOT, "%.6f %.6f%n", computeMs, totalMs);
    }
}

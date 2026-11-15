import java.io.*;
import java.util.Locale;

public class Par {

    static class Worker extends Thread {
        private final double[] a, b, c;
        private final int from, to;

        Worker(double[] a, double[] b, double[] c, int from, int to) {
            this.a = a;
            this.b = b;
            this.c = c;
            this.from = from;
            this.to = to;
        }

        @Override
        public void run() {
            for (int i = from; i < to; i++) {
                c[i] = a[i] + b[i];
            }
        }
    }

    public static void main(String[] args) throws IOException, InterruptedException {
        if (args.length < 3) {
            System.err.println("usage: java Par <input_file> <output_file> <threads>");
            System.exit(1);
        }
        long programStart = System.nanoTime();
        int p = Integer.parseInt(args[2]);

        BufferedReader in = new BufferedReader(new FileReader(args[0]));
        int n = Integer.parseInt(in.readLine().trim());
        String[] lineA = in.readLine().trim().split(" ");
        String[] lineB = in.readLine().trim().split(" ");
        in.close();

        double[] a = new double[n], b = new double[n], c = new double[n];
        for (int i = 0; i < n; i++) {
            a[i] = Double.parseDouble(lineA[i]);
            b[i] = Double.parseDouble(lineB[i]);
        }

        long computeStart = System.nanoTime();

        Worker[] workers = new Worker[p];
        for (int t = 0; t < p; t++) {
            workers[t] = new Worker(a, b, c, t * n / p, (t + 1) * n / p);
            workers[t].start();
        }
        for (Worker w : workers) {
            w.join();
        }

        long computeEnd = System.nanoTime();

        PrintWriter out = new PrintWriter(args[1]);
        out.println(n);
        for (int i = 0; i < n; i++) {
            out.print(c[i]);
            out.print(' ');
        }
        out.println();
        out.close();

        long programEnd = System.nanoTime();

        double computeMs = (computeEnd - computeStart) / 1e6;
        double totalMs = (programEnd - programStart) / 1e6;
        System.out.printf(Locale.ROOT, "%.6f %.6f%n", computeMs, totalMs);
    }
}

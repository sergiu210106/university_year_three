import java.io.*;
import java.util.Locale;

public class Seq {
    public static void main(String[] args) throws IOException {
        if (args.length < 2) {
            System.err.println("usage: java Seq <input_file> <output_file>");
            System.exit(1);
        }
        long programStart = System.nanoTime();

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
        for (int i = 0; i < n; i++) {
            c[i] = a[i] + b[i];
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

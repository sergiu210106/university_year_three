import java.io.IOException;

public class Par {

    // A class and not a lambda: the first lambda created by a JVM costs ~15 ms of
    // invokedynamic setup, which would land inside the timed region (see lab 0).
    static class Worker extends Thread {
        private final Conv.Variant variant;
        private final int t, p;

        Worker(Conv.Variant variant, int t, int p) {
            this.variant = variant;
            this.t = t;
            this.p = p;
        }

        @Override
        public void run() {
            Conv.threadWork(variant, t, p);
        }
    }

    public static void main(String[] args) throws IOException, InterruptedException {
        if (args.length < 5) {
            System.err.println("usage: java Par <matrix_f_file> <convolution_file> <output_file> <threads> "
                    + "<horizontal|vertical|block|linear|cyclic>");
            System.exit(1);
        }
        long programStart = System.nanoTime();
        int p = Integer.parseInt(args[3]);
        if (p < 1) Conv.fail("threads must be >= 1");
        Conv.Variant variant = Conv.parseVariant(args[4]);

        Conv.readInput(args[0], args[1]);

        long computeStart = System.nanoTime();

        Worker[] workers = new Worker[p];
        for (int t = 0; t < p; t++) {
            workers[t] = new Worker(variant, t, p);
            workers[t].start();
        }
        for (Worker w : workers) {
            w.join();
        }

        long computeEnd = System.nanoTime();

        Conv.writeOutput(args[2]);

        long programEnd = System.nanoTime();
        Conv.printTimes(programStart, computeStart, computeEnd, programEnd);
    }
}

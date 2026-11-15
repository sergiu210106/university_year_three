public class Par {
    static class Worker extends Thread {
        final int start, end;
        final Conv.RowBuffers buffers;

        Worker(int start, int end) {
            this.start = start;
            this.end = end;
            buffers = new Conv.RowBuffers(start, end, true);
        }

        @Override
        public void run() {
            Conv.computeRange(start, end, buffers);
        }
    }

    public static void main(String[] args) {
        if (args.length != 3) {
            System.err.println("usage: java Par <input_file> <output_file> <threads>");
            System.exit(1);
        }
        try {
            long programStart = System.nanoTime();
            int requestedThreads = Integer.parseInt(args[2]);
            if (requestedThreads < 1)
                throw new IllegalArgumentException("threads must be positive");
            Conv.readInput(args[0]);
            long computeStart = System.nanoTime();
            int count = Math.min(requestedThreads, Conv.rows);
            Worker[] workers = new Worker[count];
            // Complete ALL snapshots before starting ANY worker: no barrier is needed.
            for (int t = 0; t < count; t++) {
                int start = (int) ((long) t * Conv.rows / count);
                int end = (int) ((long) (t + 1) * Conv.rows / count);
                workers[t] = new Worker(start, end);
            }
            for (Worker worker : workers) worker.start();
            for (Worker worker : workers) worker.join();
            long computeEnd = System.nanoTime();
            Conv.writeOutput(args[1]);
            Conv.printTimes(programStart, computeStart, computeEnd);
        } catch (Exception error) {
            System.err.println("error: " + error.getMessage());
            System.exit(1);
        }
    }
}

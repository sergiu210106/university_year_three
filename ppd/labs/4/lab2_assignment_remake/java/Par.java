import java.util.concurrent.CyclicBarrier;

public class Par {
    static class Worker extends Thread {
        final int start, end;
        final CyclicBarrier snapshotsReady;

        Worker(int start, int end, CyclicBarrier snapshotsReady) {
            this.start = start;
            this.end = end;
            this.snapshotsReady = snapshotsReady;
        }

        @Override
        public void run() {
            try {
                // Variant 1b: each worker saves its original neighbours first.
                Conv.RowBuffers buffers = new Conv.RowBuffers(start, end, true);
                // Nobody may overwrite the image until ALL snapshots are ready.
                snapshotsReady.await();
                Conv.computeRange(start, end, buffers);
            } catch (Throwable error) {
                // This command-line program aborts on worker failure so peers cannot hang.
                System.err.println("worker error: " + error);
                System.exit(1);
            }
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
            CyclicBarrier snapshotsReady = new CyclicBarrier(count);
            Worker[] workers = new Worker[count];
            for (int t = 0; t < count; t++) {
                int start = (int) ((long) t * Conv.rows / count);
                int end = (int) ((long) (t + 1) * Conv.rows / count);
                workers[t] = new Worker(start, end, snapshotsReady);
            }
            for (Worker worker : workers) worker.start();
            for (Worker worker : workers) worker.join();
            long computeEnd = System.nanoTime();
            Conv.writeOutput(args[1]);
            Conv.printTimes(programStart, computeStart, computeEnd);
        } catch (Throwable error) {
            // Also abort on startup failure rather than leaving a partial barrier group.
            System.err.println("error: " + error);
            System.exit(1);
        }
    }
}

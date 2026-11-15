public class Seq {
    public static void main(String[] args) {
        if (args.length != 2) {
            System.err.println("usage: java Seq <input_file> <output_file>");
            System.exit(1);
        }
        try {
            long programStart = System.nanoTime();
            Conv.readInput(args[0]);
            long computeStart = System.nanoTime();
            Conv.RowBuffers buffers = new Conv.RowBuffers(0, Conv.rows, false);
            Conv.computeRange(0, Conv.rows, buffers);
            long computeEnd = System.nanoTime();
            Conv.writeOutput(args[1]);
            Conv.printTimes(programStart, computeStart, computeEnd);
        } catch (Exception error) {
            System.err.println("error: " + error.getMessage());
            System.exit(1);
        }
    }
}

import java.io.IOException;

public class Seq {
    public static void main(String[] args) throws IOException {
        if (args.length < 3) {
            System.err.println("usage: java Seq <matrix_f_file> <convolution_file> <output_file>");
            System.exit(1);
        }
        long programStart = System.nanoTime();

        Conv.readInput(args[0], args[1]);

        long computeStart = System.nanoTime();
        Conv.computeRect(0, Conv.n, 0, Conv.m);
        long computeEnd = System.nanoTime();

        Conv.writeOutput(args[2]);

        long programEnd = System.nanoTime();
        Conv.printTimes(programStart, computeStart, computeEnd, programEnd);
    }
}

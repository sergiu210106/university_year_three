 public class Main {
    public static double[] generate(int n){
        double[] array = new double[n];
        for(int i = 0; i < n; i++){
            array[i] = Math.random();
        }
        return array;
    }
   
    public static double[] vectorSumSerial(int n, double[] a, double[] b){
        double[] c = new double[n];
        for (int i=0; i<n; i++){
            c[i] = a[i] + b[i];
        }
        return c;
    }
 
    public static double[] vectorSumParallel(int n, double[] a, double[] b, int p){
        int chunkSize = (int) Math.ceil((double)n/p);
        double[] c = new double[n];
        Thread[] threads = new Thread[p];
        for (int i=0; i<p; i++){
            int start = i * chunkSize;
            int end = Math.min(start + chunkSize, n);
            threads[i] = new Thread(() -> {
                for (int j=start; j<end; j++){
                    c[j] = a[j] + b[j];
                }
            });
            threads[i].start();
        }
        for (Thread thread : threads) {
            try {
                thread.join();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        return c;
    }
 
    public static double[] vectorSumParallelCycle(int n, double[] a, double[] b, int p){
        double[] c = new double[n];
        Thread[] threads = new Thread[p];
        for (int j=0; j<p; j++){
            int index = j;
            threads[index] = new Thread(() -> {
                for (int i=index; i<n; i+=p){
                    c[i] = a[i] + b[i];
                }
            });
        }
        for (Thread thread : threads){
            thread.start();
        }
        for (Thread thread : threads) {
            try {
                thread.join();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        return c;
    }
    public static void print(double[] array){
        for (int i=0; i<array.length; i++){
            System.out.print(array[i] + " ");
        }
        System.out.println();
    }
 
    public static boolean correct(double[] a, double[] b, double[] c){
        for (int i=0; i<a.length; i++){
            if (Math.abs(a[i] + b[i] - c[i]) > 1e-6){
                return false;
            }
        }
        return true;
    }
 
    public static void main(String[] args) {
        int n = Integer.parseInt(args[0]);
        int tp = Integer.parseInt(args[1]);
        int p = Integer.parseInt(args[2]);
 
        double[] a = generate(n);
        double[] b = generate(n);
        double[] c = null;
        double start = System.nanoTime();
        if (tp==0) c = vectorSumSerial(n, a, b);
        else if (tp==1) c = vectorSumParallel(n, a, b, p);
        else if (tp==2) c= vectorSumParallelCycle(n, a, b, p);
 
        double finish = System.nanoTime();
        System.out.println((finish-start) / 1e6);
        // print(a);
        // print(b);
        // print(c);
        System.out.println(correct(a, b, c));
    }
   
}
 
 
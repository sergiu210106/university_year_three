#include <fstream>
#include <iostream>
#include <random>
#include <string>

using namespace std;

// usage: generate_input <n> <m> <k> <matrix_f_file> <convolution_file> <seed>
// writes F (n x m, random integers in [0, 255]) and C (k x k, random integers in [-5, 5]):
//   matrix_f_file:    "n m", then n rows of m values
//   convolution_file: "k",   then k rows of k values
int main(int argc, char **argv) {
    if (argc < 7) {
        cerr << "usage: " << argv[0] << " <n> <m> <k> <matrix_f_file> <convolution_file> <seed>\n";
        return 1;
    }

    int n = stoi(argv[1]), m = stoi(argv[2]), k = stoi(argv[3]);
    mt19937 gen(stoul(argv[6]));
    uniform_int_distribution<int> pixel(0, 255), weight(-5, 5);

    ofstream ff(argv[4]);
    ff << n << ' ' << m << '\n';
    for (int i = 0; i < n; i ++) {
        for (int j = 0; j < m; j ++)
            ff << (j ? " " : "") << pixel(gen);
        ff << '\n';
    }

    ofstream fc(argv[5]);
    fc << k << '\n';
    for (int i = 0; i < k; i ++) {
        for (int j = 0; j < k; j ++)
            fc << (j ? " " : "") << weight(gen);
        fc << '\n';
    }

    return 0;
}

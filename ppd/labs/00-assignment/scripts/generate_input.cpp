#include <fstream>
#include <iostream>
#include <iomanip>
#include <random>
#include <string>

using namespace std;

// usage: generate_input <n> <output_file> <seed>
// writes n, then the n elements of A, then the n elements of B (random reals in [-1000, 1000])
int main(int argc, char **argv) {
    if (argc < 4) {
        cerr << "usage: " << argv[0] << " <n> <output_file> <seed>\n";
        return 1;
    }

    int n = stoi(argv[1]);
    ofstream fout(argv[2]);
    mt19937 gen(stoul(argv[3]));
    uniform_real_distribution<double> dist(-1000.0, 1000.0);

    fout << n << '\n' << setprecision(10);
    for (int i = 0; i < n; i ++)
        fout << dist(gen) << ' ';
    fout << '\n';
    for (int i = 0; i < n; i ++)
        fout << dist(gen) << ' ';
    fout << '\n';

    return 0;
}

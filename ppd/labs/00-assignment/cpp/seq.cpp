#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <iomanip>

using namespace std;
using Clock = chrono::steady_clock;

// usage: seq <input_file> <output_file>
// prints "<compute_ms> <total_ms>" to stdout
int main(int argc, char **argv) {
    if (argc < 3) {
        cerr << "usage: " << argv[0] << " <input_file> <output_file>\n";
        return 1;
    }
    auto program_start = Clock::now();

    ifstream fin(argv[1]);
    int n; fin >> n;
    vector<double> a(n), b(n), c(n);
    for (int i = 0; i < n; i ++) fin >> a[i];
    for (int i = 0; i < n; i ++) fin >> b[i];

    auto compute_start = Clock::now();
    for (int i = 0; i < n; i ++) {
        c[i] = a[i] + b[i];
    }
    auto compute_end = Clock::now();

    ofstream fout(argv[2]);
    fout << n << '\n' << fixed << setprecision(10);
    for (int i = 0; i < n; i ++)
        fout << c[i] << ' ';
    fout << '\n';
    fout.close();

    auto program_end = Clock::now();

    chrono::duration<double, milli> compute = compute_end - compute_start;
    chrono::duration<double, milli> total = program_end - program_start;
    cout << fixed << setprecision(6) << compute.count() << ' ' << total.count() << '\n';
    return 0;
}

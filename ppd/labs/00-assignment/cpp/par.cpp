#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <thread>
#include <string>

using namespace std;
using Clock = chrono::steady_clock;

// usage: par <input_file> <output_file> <threads>
// prints "<compute_ms> <total_ms>" to stdout
int main(int argc, char **argv) {
    if (argc < 4) {
        cerr << "usage: " << argv[0] << " <input_file> <output_file> <threads>\n";
        return 1;
    }
    auto program_start = Clock::now();
    int p = stoi(argv[3]);

    ifstream fin(argv[1]);
    int n; fin >> n;
    vector<double> a(n), b(n), c(n);
    for (int i = 0; i < n; i ++) fin >> a[i];
    for (int i = 0; i < n; i ++) fin >> b[i];

    auto compute_start = Clock::now();

    // thread t adds the block [t*n/p, (t+1)*n/p)
    vector<thread> threads;
    for (int t = 0; t < p; t ++) {
        int from = t * n / p, to = (t + 1) * n / p;
        threads.emplace_back([&, from, to] {
            for (int i = from; i < to; i ++) {
                c[i] = a[i] + b[i];
            }
        });
    }
    for (auto &th : threads) th.join();

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

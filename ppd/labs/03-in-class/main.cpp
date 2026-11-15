#include <iostream>
#include <cstdlib>
#include <chrono>
#include <vector>
#include <thread>
#include <mutex>

const int SIZE = 1e7;
double a[SIZE], b[SIZE], c1[SIZE], c2[SIZE], c3[SIZE];

double d1=0, d2=0, d3=0;

std::mutex mut;

void generateRandomArray(int n, double* v) {
    for (int i = 0; i < n; i ++) {
        v[i] = rand() % 100;
    }
}

void sumSerial(int n, double* a, double* b, double* c) {
    for (int i = 0; i < n; i ++) {
        c[i] = a[i] + b[i];
    }
}

void sumParallelLiniar(int n, int p, double* a, double* b, double* c) {
    int start = 0;
    int chunkSize = n/p;
    int rest = n % p;

    std::vector<std::thread> threads;

    for (int i = 0; i < n; i ++) {
        int end = start + chunkSize;
        if (rest) {
            rest --;
            end ++;
        }
        threads.emplace_back([a,b,c](int start, int end){
            for (int j = start; j < end; j ++) {
                c[j] = a[j] + b[j];
            }
        }, start, end); 

        start = end;
    }

    for (auto& thread: threads) {
        thread.join();
    }
}

void sumParallelCyclic(int n, int p, double* a, double* b, double* c) {
    std::vector<std::thread> threads;

    for (int index = 0; index < n; index ++) {
        threads.emplace_back([a,b,c,n,p](int index){
            for (int j = index; j < n; j += p) {
                c[j] = a[j] + b[j];
            }
        }, index); 
    }

    for (auto& thread: threads) {
        thread.join();
    }
}

void sumSerialDot(int n, double* a, double* b, double& d) {
    for (int i = 0; i < n; i ++) {
        d += a[i]*b[i];
    }
}

void sumParallelLiniarDot(int n, int p, double* a, double* b, double& d) {
    int start = 0;
    int chunkSize = n/p;
    int rest = n % p;

    std::vector<std::thread> threads;

    for (int i = 0; i < n; i ++) {
        int end = start + chunkSize;
        if (rest) {
            rest --;
            end ++;
        }
        threads.emplace_back([a,b,d](int start, int end){
            int local=0;
            for (int j = start; j < end; j ++) {
                local += a[j]*b[j];
                std::unique_lock<std::mutex> lock(mut);
            }
        }, start, end); 

        start = end;
    }

    for (auto& thread: threads) {
        thread.join();
    }
}

void sumParallelCyclicDot(int n, int p, double* a, double* b, double* c) {
    std::vector<std::thread> threads;

    for (int index = 0; index < n; index ++) {
        threads.emplace_back([a,b,c,n,p](int index){
            for (int j = index; j < n; j += p) {
                c[j] = a[j] + b[j];
            }
        }, index); 
    }

    for (auto& thread: threads) {
        thread.join();
    }
}

bool check(int n, double* a, double* b, double* c) {
    for (int i = 0; i < n; i ++) {
        if (abs(c[i] - a[i] - b[i]) >= 1e-5) return false;
    }
    return true;
}

int main(int argc, char* argv[]){
    int n = std::atoi(argv[1]);
    int p = std::atoi(argv[2]);

    generateRandomArray(n, a);
    generateRandomArray(n, b);

    auto startSerial = std::chrono::high_resolution_clock::now();
    sumSerial(n,a,b,c1);
    auto endSerial = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> executionTimeSerial = endSerial - startSerial;
    std::cout << executionTimeSerial.count() << '\n';

    std::cout << check(n,a,b,c1) << '\n';

    auto startParallelLiniar = std::chrono::high_resolution_clock::now();
    sumParallelLiniar(n,p,a,b,c2);
    auto endParallelLiniar = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> executionTimeParallelLiniar = endParallelLiniar - startParallelLiniar;
    std::cout << executionTimeParallelLiniar.count() << '\n';

    std::cout << check(n,a,b,c2) << '\n';

    auto startParallelCycle = std::chrono::high_resolution_clock::now();
    sumParallelCyclic(n,p,a,b,c3);
    auto endParallelCycle = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> executionTimeParallelCycle = endParallelCycle - startParallelCycle;
    std::cout << executionTimeParallelCycle.count() << '\n';

    std::cout << check(n,a,b,c3) << '\n';

    return 0;
}

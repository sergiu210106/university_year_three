#include <iostream>
#include <cstdlib>
#include <chrono>
#include <vector>
#include <thread>
#include <mutex>
using namespace std;
vector<double> a, b;
double d1=0,d2=0,d3=0;
mutex mtx;
void generateRandomArray(int n, vector<double>& v) {
    for (int i =0;i<n;i++) {
        v[i] = rand() % 100;
    }
}
void sumSerial(int n,vector<double>& a, vector<double>& b, double& d) {
    for (int i =0;i<n;i++) {
        d = d + a[i]*b[i];
    }
}
void sumParallelLiniar(int n,int p,vector<double>& a, vector<double>& b, double& c) {
    int start = 0;
    int chunk = n / p;
    int rest = n%p;
    int end = 0;
    vector<thread> threads;
    for (int i = 0;i < p;i++) {
        end = start + chunk;
        if (rest) {
            rest--;
            end++;
        }
        threads.emplace_back([&a,&b,&c](int start,int end) {
            double local = 0;
            for (int j = start;j<end;j++)
                local = local + (a[j] * b[j]);
            unique_lock<mutex> lock(mtx);
            c = c + local;
        },start,end);
        start = end;
    }
    for (auto& t : threads)
        t.join();
}
bool check(double a,double b) {
        if (abs(a-b)>=1e-6) {
            return false;
    }
    return true;
}
void cyclic(int n,int p,vector<double>& a,vector<double>& b,double& c) {
    int start = 0;
    int end = 0;
    vector<thread> threads;
    for (int i = 0;i < p;i++) {
        threads.emplace_back([&a,&b,&c,n,p](int index) {
            double local = 0;
            for (int j = index;j<n;j+=p)
                local = a[j] * b[j] + local;
            unique_lock<mutex> lock(mtx);
            c = c + local;
        },i);
    }
    for (auto& t : threads)
        t.join();
}
int main(int argc,char* argv[]) {

    int n = atoi(argv[1]);
    int p = atoi(argv[2]);

    a.reserve(n);
    b.reserve(n);

    generateRandomArray(n,a);
    generateRandomArray(n,b);
    auto start = chrono::high_resolution_clock::now();
    sumSerial(n,a,b,d1);
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double,std::milli> executionTime = end - start;
    auto start1 = chrono::high_resolution_clock::now();
    sumParallelLiniar(n,p,a,b,d2);
    auto end1 = chrono::high_resolution_clock::now();
    chrono::duration<double,std::milli> executionTime1 = end1 - start1;
    auto start2 = chrono::high_resolution_clock::now();
    cyclic(n,p,a,b,d3);
    auto end2 = chrono::high_resolution_clock::now();
    chrono::duration<double,std::milli> executionTime2 = end2 - start2;
    cout<<executionTime.count()<<' '<<executionTime1.count()<<' '<<executionTime2.count()<<"\n";
    cout<<check(d1,d2)<<' '<<check(d1,d3);

    return 0;
}
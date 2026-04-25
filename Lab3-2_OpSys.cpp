#include <iostream>
#include <omp.h>
#include <windows.h>

using namespace std;

const long long N = 100'000'000;
const int STUDENT_ID = 431'321;
const long long BLOCK_SIZE = 10 * STUDENT_ID;

int main() {
    double pi = 0.0;
    int num_threads = 4; // 1, 2, 4, 8, 12, 16

    omp_set_num_threads(num_threads);
    ULONGLONG start_time = GetTickCount64();

#pragma omp parallel for schedule(dynamic, BLOCK_SIZE)
    for (long long i = 0; i < N; ++i) {
        double x = (i + 0.5) / (double)N;
        double local_val = 4.0 / (1.0 + x * x);

#pragma omp critical(my_pi_sum) // критическую секция
        {
            pi += local_val;
        }
    }
    pi *= (1.0 / N);
    ULONGLONG end_time = GetTickCount64();

    cout << "OpenMP Pi: " << pi << 
            "\nThreads: " << num_threads <<
            "\nTime: " << (end_time - start_time) << " ms" << endl;

    return 0;
}
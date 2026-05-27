#include <iostream>
#include <windows.h>
#include <vector>

using namespace std;

const long long N = 100'000'000;
const int STUDENT_ID = 431'321;
const long long BLOCK_SIZE = 10 * STUDENT_ID;

double total_pi = 0.0;
CRITICAL_SECTION cs;

struct ThreadData {
    long long start_i;
    long long end_i;
    bool is_busy;
    bool terminate;
};

// Функция, которую выполняет каждый поток
DWORD WINAPI ThreadProc(LPVOID lpParam) {
    ThreadData* data = (ThreadData*)lpParam;

    while (true) {
        if (data->terminate) break;

        double local_sum = 0.0;
        for (long long i = data->start_i; i < data->end_i; ++i) {
            double x = (i + 0.5) / (double)N;
            local_sum += 4.0 / (1.0 + x * x);
        }

        // Вход в критическую секцию
        EnterCriticalSection(&cs);
        total_pi += local_sum; // безопасно увлеичиваем счётчик
        LeaveCriticalSection(&cs);
        // Выход

        data->is_busy = false;
        SuspendThread(GetCurrentThread());
    }
    return 0;
}

int main() {
    int num_threads = 16; // 1, 2, 4, 8, 12, 16
    InitializeCriticalSection(&cs);

    vector<HANDLE> hThreads(num_threads);
    vector<ThreadData> td(num_threads);

    ULONGLONG start_time = GetTickCount64();
    for (int i = 0; i < num_threads; ++i) {
        td[i].is_busy = false;
        td[i].terminate = false;
        hThreads[i] = CreateThread(NULL, 0, ThreadProc, &td[i], CREATE_SUSPENDED, NULL);
    }

    long long current_i = 0;
    while (current_i < N) {
        bool task_assigned = false;
        for (int i = 0; i < num_threads && current_i < N; ++i) {
            if (!td[i].is_busy) {
                td[i].start_i = current_i;
                td[i].end_i = min(current_i + BLOCK_SIZE, N);
                td[i].is_busy = true;
                ResumeThread(hThreads[i]);
                current_i = td[i].end_i;
                task_assigned = true;
            }
        }
        if (!task_assigned) Sleep(1); // без этого программа выполняется ооочень долго
    }

    bool working = true;
    while (working) {
        working = false;
        for (int i = 0; i < num_threads; i++) {
            if (td[i].is_busy) {
                working = true;
                break;
            }
        }
        if (working) Sleep(1);
    }

    for (int i = 0; i < num_threads; i++) {
        td[i].terminate = true;
        ResumeThread(hThreads[i]);
    }
    WaitForMultipleObjects(num_threads, hThreads.data(), TRUE, INFINITE);

    total_pi *= (1.0 / N);
    ULONGLONG end_time = GetTickCount64();

    cout << "Win32 API Pi: " << total_pi <<
            "\nThreads: " << num_threads << 
            "\nTime: " << (end_time - start_time) << " ms" << endl;

    // закрытие дескрипторов потоков, чтобы не утекала память
    for (int i = 0; i < num_threads; ++i) CloseHandle(hThreads[i]);
    DeleteCriticalSection(&cs);

    return 0;
}

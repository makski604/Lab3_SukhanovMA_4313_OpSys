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
    long long start_i; // начало блока итераций
    long long end_i; // конец блока итераций
    bool is_busy; // true - поток занят
    bool terminate; // true - поток завершён
};

DWORD WINAPI ThreadProc(LPVOID lpParam) {
    ThreadData* data = (ThreadData*)lpParam;

    while (true) {
        if (data->terminate) { // есть ли сигнал на завершение потока?
            break; // выходим из цикла, завершая поток
        }

        // вычисляем пи:
        double local_sum = 0.0;
        for (long long i = data->start_i; i < data->end_i; ++i) {
            double x = (i + 0.5) / (double)N;
            local_sum += 4.0 / (1.0 + x * x);
        }

        // в критической секции безопасно накапливаем сумму в общую переменную:
        EnterCriticalSection(&cs);
        total_pi += local_sum; 
        LeaveCriticalSection(&cs);

        data->is_busy = false; // сигнал гл. потоку, что блок обработан, рабочий поток освободился
        // ГОНКА 1:
        // Здесь главный поток может вызвать ResumeThread раньше, чем текущий поток приостановит себя
        // (т.к. гл. поток видит, что текущий рабочий поток подал сигнал data->is_busy = false;),
        // и в ИТОГЕ: текущий поток засыпает НАВСЕГДА!!!!!!!!!!!!!!!!
        SuspendThread(GetCurrentThread()); // рабочий поток приостонавлиявает себя
    }
    return 0;
}

int main() {
    int num_threads = 18; // 1, 2, 4, 8, 12, 16
    InitializeCriticalSection(&cs);
    vector<HANDLE> hThreads(num_threads);
    vector<ThreadData> td(num_threads);

    ULONGLONG start_time = GetTickCount64(); // засекаем время.
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
                // Из теории:
                /* Выполнение отдельного потока можно приостанавливать несколько раз.
                Количество приостановок сохраняется в атрибутах
                объекта типа «поток» с дескриптором hThread. */

                // Решение ГОНКИ 1 (см. выше):
                while (true) {
                    DWORD suspend_count = SuspendThread(hThreads[i]); // Пполучаем прошлое значение счетчика:
                    if (suspend_count == 1) { // если счёчик был 1, значит рабочий поток успел себя приостановить,
                        break; // поэтому мы выходим из цикла, и вызываем resumethread ДВАЖДЫ (чтобы уменьшить счёчик с 2 до 0).
                    }
                    // Если счёчик == 0, то рабочик поток не успел приостановить себя (гл. поток опередил его).
                    ResumeThread(hThreads[i]); // Сбрасывем счётчик suspend обратно в 0.
                    SwitchToThread(); // Гл. поток принудительно останавливает себя,
                                      // чтобы дать возможность рабочему потоку приостановить себя.
                }

                td[i].start_i = current_i;
                td[i].end_i = min(current_i + BLOCK_SIZE, N);
                td[i].is_busy = true;
                // ДВАЖДЫ уменьшаем счёчик:
                ResumeThread(hThreads[i]);
                ResumeThread(hThreads[i]);
                current_i = td[i].end_i;
                task_assigned = true; // <-- успешное распределение блоков итераций
            }
        }
        if (!task_assigned) { // Если в цикле выше блоки итераций не были распределены, 
            SwitchToThread(); // то гл. поток приостанавливает себя и даёт возможность
                              // выполнять потоки с более низким приоритетом.
        }
    }

    bool working = true;
    while (working) { // Пока вычисления продолжаются:
        working = false;
        for (int i = 0; i < num_threads; i++) {
            if (td[i].is_busy) { 
                working = true;
                break;
            }
        }
        if (working) SwitchToThread();
    }

    // Все потоки завершили свою работу.
    // Меняем флаг завершения каждого из них (terminate = true),
    // чтобы потоки вышли из своего цикла вычислений:
    for (int i = 0; i < num_threads; i++) {
        // Снова проверка ГОНКИ 1:
        while (true) {
            DWORD suspend_count = SuspendThread(hThreads[i]);
            if (suspend_count == 1) break;
            ResumeThread(hThreads[i]);
            SwitchToThread();
        }
        td[i].terminate = true;
        ResumeThread(hThreads[i]);
        ResumeThread(hThreads[i]);
    }
    WaitForMultipleObjects(num_threads, hThreads.data(), TRUE, INFINITE);

    // Окончательное вычисление числа пи,
    // с точностью N = 100'000'000 знаков после запятой:
    total_pi *= (1.0 / N);
    ULONGLONG end_time = GetTickCount64(); // фиксируем время.

    cout << "Win32 API Pi: " << total_pi <<
            "\nThreads: " << num_threads << 
            "\nTime: " << (end_time - start_time) << " ms" << endl;
    for (int i = 0; i < num_threads; ++i) CloseHandle(hThreads[i]);
    DeleteCriticalSection(&cs);
    return 0;
}

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <semaphore.h>
#include <chrono>
#include <vector>
#include <atomic>
#include <random>

using namespace std;

const int NUM_THREADS = 3; // Количество потоков для каждого примитива
const int TARGET_COUNT = 5; // Целевое количество символов для вывода

// Глобальные переменные для синхронизации
mutex mtx; 
sem_t semaphore; 
mutex semaphoreSlimMutex; 
condition_variable semaphoreSlimCV; // Условная переменная для SemaphoreSlim
int semaphoreSlimCount = 1; // Счетчик для SemaphoreSlim
atomic_flag spinLock = ATOMIC_FLAG_INIT; 
mutex outputMutex; // Для синхронизации
condition_variable monitorCV; // Условная переменная для Monitor
int monitorCount = 0; // Счетчик для Monitor

atomic<int> activeThreadsCount(0); // Количество активных потоков

class SpinWait {
public:
    void wait() {
        while (!ready.load(memory_order_acquire)) {
            this_thread::yield();  // Используем yield для активного ожидания
        }
    }

    void notify() {
        ready.store(true, memory_order_release);  // Устанавливаем флаг готовности
    }

    void reset() {
        ready.store(false, memory_order_release); // Сбрасываем флаг готовности
    }

private:
    atomic<bool> ready{false}; // Флаг готовности
};

// Реализация барьера
class Barrier {
public:
    Barrier(int count) : thread_count(count), waiting_count(0) {}

    void arrive_and_wait() {
        unique_lock<mutex> lock(mtx);
        waiting_count++;
        if (waiting_count == thread_count) {
            waiting_count = 0; // Сброс счетчика для следующей гонки
            cv.notify_all(); // Разбудить всех ожидающих
        } else {
            cv.wait(lock); // Ожидание
        }
    }

private:
    int thread_count;
    int waiting_count;
    mutex mtx;
    condition_variable cv;
};

Barrier barrier(NUM_THREADS); // Создание барьера

// Класс для измерения времени
class StopWatch {
private:
    chrono::time_point<chrono::high_resolution_clock> start;

public:
    StopWatch() {
        start = chrono::high_resolution_clock::now();
    }

    double elapsed() const {
        return chrono::duration<double, milli>(chrono::high_resolution_clock::now() - start).count();
    }
};

// Функция для генерации случайного символа ASCII
char getRandomAsciiChar() {
    return static_cast<char>(rand() % 95 + 32); 
}

// Функция для гонки с разными примитивами
void race(int threadID, int primitiveType) {
    StopWatch stopwatch; // Запуск таймера
    SpinWait spinwait; // Создание объекта SpinWait
    int count = 0; // Счетчик вывода символов

    // Обеспечиваем корректный порядок уведомления
    if (primitiveType == 5) {
        spinwait.notify(); // Первое уведомление перед началом цикла
    }

    while (count < TARGET_COUNT) {
        switch (primitiveType) {
            case 0: // Mutex
                {
                    lock_guard<mutex> lock(mtx);
                    {
                        lock_guard<mutex> outputLock(outputMutex);
                        cout << "Поток " << threadID << " (Mutex): " << getRandomAsciiChar() << endl;
                    }
                    count++;
                }
                break;
            case 1: // Semaphore
                sem_wait(&semaphore);
                {
                    activeThreadsCount+=2;
                    {
                        lock_guard<mutex> outputLock(outputMutex);
                        cout << "Поток " << threadID << " (Semaphore): " << getRandomAsciiChar() << endl;
                    }
                    count++;
                }
                sem_post(&semaphore);
                activeThreadsCount-=2;
                break;
            case 2: // SemaphoreSlim
                {
                    unique_lock<mutex> lock(semaphoreSlimMutex);
                    while (semaphoreSlimCount <= 0) {
                        semaphoreSlimCV.wait(lock);
                    }
                    semaphoreSlimCount-=2;
                    {
                        lock_guard<mutex> outputLock(outputMutex);
                        cout << "Поток " << threadID << " (SemaphoreSlim): " << getRandomAsciiChar() << endl;
                    }
                    count++;
                    semaphoreSlimCount+=2;
                    semaphoreSlimCV.notify_one();
                }
                break;
            case 3: // SpinLock
                while (spinLock.test_and_set(memory_order_acquire)); // Активное ожидание
                {
                    lock_guard<mutex> outputLock(outputMutex);
                    cout << "Поток " << threadID << " (SpinLock): " << getRandomAsciiChar() << endl;
                }
                count++;
                spinLock.clear(memory_order_release); // Освобождение SpinLock
                break;
            case 4: // Monitor (обновленная реализация)
                {
                    unique_lock<mutex> lock(mtx);
                    while (monitorCount >= TARGET_COUNT) {
                        monitorCV.wait(lock); // Ожидание, пока не будет разрешено продолжение
                    }
                    {
                        lock_guard<mutex> outputLock(outputMutex);
                        cout << "Поток " << threadID << " (Monitor): " << getRandomAsciiChar() << endl;
                    }
                    count++;
                    monitorCount++;
                    monitorCV.notify_all(); // Уведомляем другие потоки
                    if (count < TARGET_COUNT) {
                        this_thread::sleep_for(chrono::milliseconds(100)); // Задержка для видимости гонки
                    }
                }
                break;
            case 5: // SpinWait
                spinwait.wait();  // Ожидание уведомления для продолжения
                {
                    lock_guard<mutex> outputLock(outputMutex);
                    cout << "Поток " << threadID << " (SpinWait): " << getRandomAsciiChar() << endl;
                }
                count++;
                spinwait.reset(); // Сброс готовности для следующего цикла
                spinwait.notify(); // Уведомление, что поток завершил свою работу
                break;
            case 6: // Barrier
                {
                    {
                        lock_guard<mutex> outputLock(outputMutex);
                        cout << "Поток " << threadID << " (Barrier): " << getRandomAsciiChar() << endl;
                    }
                    count++;
                    barrier.arrive_and_wait(); // Ожидание других потоков
                }
                break;
        }
    }
    {
        lock_guard<mutex> outputLock(outputMutex);
        cout << "Поток " << threadID << " завершил гонку за " << stopwatch.elapsed() << " миллисекунд." << endl;
    }
}

int main() {
    srand(static_cast<unsigned int>(time(0))); // Инициализация генератора случайных чисел

    // Инициализация Semaphore
    sem_init(&semaphore, 0, 1); // Инициализация семафора с 1 разрешением
    
    // Создание и запуск потоков для всех примитивов
    vector<thread> threads;
    for (int primitiveType = 0; primitiveType <= 6; ++primitiveType) {
        for (int j = 1; j <= NUM_THREADS; ++j) {
            threads.emplace_back(race, j, primitiveType);
        }
    }

    // Ожидание завершения всех потоков
    for (auto& th : threads) {
        th.join();
    }

    // Очистка ресурсов
    sem_destroy(&semaphore);
    return 0;
}
#include <iostream>
#include <thread>
#include <vector>
#include <shared_mutex>
#include <mutex>
#include <condition_variable>
#include <chrono>

using namespace std;

shared_mutex rwLock;             // Мьютекс для синхронизации чтения и записи
mutex priorityLock;              // Мьютекс для управления очередностью
mutex consoleMutex;              // Мьютекс для синхронизации вывода в консоль
condition_variable cv;           // Условная переменная для переключения между группами
int sharedResource = 0;          // Общий ресурс
int activeWriters = 0;           // Количество активных писателей
int activeReaders = 0;           // Количество активных читателей
int completedWriters = 0;        // Счетчик завершенных писателей в группе
int completedReaders = 0;        // Счетчик завершенных читателей в группе
bool writerTurn = true;          // Флаг очередности: true - писатели, false - читатели

const int WRITERS_GROUP = 3;     // Количество писателей в группе
const int READERS_GROUP = 5;     // Количество читателей в группе
const int NUM_OPERATIONS = 10;   // Количество операций для каждого потока

// Чтение с учетом очередности
void reader(int id, int operations) {
    for (int i = 0; i < operations; ++i) {
        unique_lock<mutex> lock(priorityLock);
        cv.wait(lock, [] { return !writerTurn; }); // Ждем своей очереди (читателей)

        ++activeReaders; // Увеличиваем количество активных читателей
        lock.unlock();

        rwLock.lock_shared(); // Захват блокировки для чтения
        {
            lock_guard<mutex> consoleLock(consoleMutex);
            cout << "Reader " << id << " reads value: " << sharedResource << endl;
        }
        this_thread::sleep_for(chrono::milliseconds(100));
        rwLock.unlock_shared(); // Освобождаем блокировку для чтения

        lock.lock();
        ++completedReaders;
        --activeReaders;
        if (completedReaders == READERS_GROUP) { // Если группа читателей завершена
            writerTurn = true;
            completedReaders = 0; // Сбрасываем счетчик завершенных читателей
            cv.notify_all();      // Передаем очередь писателям
        }
        lock.unlock();
    }
}

// Запись с учетом очередности
void writer(int id, int operations) {
    for (int i = 0; i < operations; ++i) {
        unique_lock<mutex> lock(priorityLock);
        cv.wait(lock, [] { return writerTurn; }); // Ждем своей очереди (писателей)

        ++activeWriters; // Увеличиваем количество активных писателей
        lock.unlock();

        rwLock.lock(); // Захват блокировки для записи
        sharedResource += id;
        {
            lock_guard<mutex> consoleLock(consoleMutex);
            cout << "Writer " << id << " writes value: " << sharedResource << endl;
        }
        this_thread::sleep_for(chrono::milliseconds(150));
        rwLock.unlock(); // Освобождаем блокировку для записи

        lock.lock();
        ++completedWriters;
        --activeWriters;
        if (completedWriters == WRITERS_GROUP) { // Если группа писателей завершена
            writerTurn = false;
            completedWriters = 0; // Сбрасываем счетчик завершенных писателей
            cv.notify_all();      // Передаем очередь читателям
        }
        lock.unlock();
    }
}

int main() {
    vector<thread> threads;
    int numReaders = 5;
    int numWriters = 3;

    // Создаем потоки писателей
    for (int i = 0; i < numWriters; ++i) {
        threads.emplace_back(writer, i + 1, NUM_OPERATIONS);
    }

    // Создаем потоки читателей
    for (int i = 0; i < numReaders; ++i) {
        threads.emplace_back(reader, i + 1, NUM_OPERATIONS);
    }

    // Ожидание завершения всех потоков
    for (auto& thread : threads) {
        thread.join();
    }

    {
        lock_guard<mutex> consoleLock(consoleMutex);
        cout << "All threads completed. Final shared value: " << sharedResource << endl;
    }

    return 0;
}
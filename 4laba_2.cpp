#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <fstream>
#include <random>
#include <chrono>

using namespace std;

// Структура для хранения данных о покупке
struct Purchase {
    int checkNumber;  // Номер чека
    string product;   // Название продукта
    int price;        // Цена продукта
    int quantity;     // Количество продукта (в данном случае всегда 1)
};

// Функция для генерации случайных покупок
void generateRandomPurchases(vector<Purchase>& purchases, int numProducts, int maxPrice) {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> productDist(0, 9);
    uniform_int_distribution<> priceDist(1, maxPrice); 

    string products[] = {"apple", "banana", "orange", "grape", "pear", "mango", "pineapple", "strawberry", "blueberry", "kiwi"};

    for (int i = 0; i < numProducts; ++i) {
        Purchase purchase;
        purchase.checkNumber = i + 1;
        purchase.product = products[productDist(gen)];
        purchase.price = priceDist(gen);
        purchase.quantity = 1;  // Каждая покупка содержит только один продукт
        purchases.push_back(purchase);
    }
}

// Функция для записи покупок в текстовый файл
void writePurchasesToFile(const vector<Purchase>& purchases, const string& filename) {
    ofstream file(filename);
    if (file.is_open()) {
        for (const auto& purchase : purchases) {
            file << purchase.checkNumber << " " << purchase.product << " " << purchase.price << endl;
        }
        file.close();
    } else {
        cerr << "Unable to open file: " << filename << endl;
    }
}

// Функция для вычисления средней цены всех продуктов
double calculateAveragePrice(const vector<Purchase>& purchases) {
    double totalPrice = 0.0;
    for (const auto& purchase : purchases) {
        totalPrice += purchase.price;
    }
    return totalPrice / purchases.size();
}

// Функция для последовательной обработки покупок
void sequentialProcessing(const vector<Purchase>& purchases) {
    double totalPrice = 0.0;
    for (const auto& purchase : purchases) {
        totalPrice += purchase.price;
    }
}

// Функция для параллельной обработки покупок
void parallelProcessing(const vector<Purchase>& purchases, int numThreads) {
    vector<thread> threads;
    int chunkSize = purchases.size() / numThreads;
    int remainder = purchases.size() % numThreads;

    mutex mtx;
    double totalPrice = 0.0;

    for (int i = 0; i < numThreads; ++i) {
        int start = i * chunkSize;
        int end = start + chunkSize + (i == numThreads - 1 ? remainder : 0);
        threads.emplace_back([&purchases, start, end, &totalPrice, &mtx]() {
            double localTotalPrice = 0.0;
            for (int j = start; j < end; ++j) {
                localTotalPrice += purchases[j].price;
            }
            lock_guard<mutex> lock(mtx);
            totalPrice += localTotalPrice;
        });
    }

    for (auto& t : threads) {
        t.join();
    }
}

int main() {
    int numProducts;
    int maxPrice; 
    int numThreads;

    cout << "Enter the number of products: ";
    cin >> numProducts;

    cout << "Enter the maximum price for a product: ";
    cin >> maxPrice;

    cout << "Enter the number of parallel threads: ";
    cin >> numThreads;

    vector<Purchase> purchases;
    generateRandomPurchases(purchases, numProducts, maxPrice);
    writePurchasesToFile(purchases, "purchases.txt");

    // Измерение времени выполнения последовательной обработки
    auto startSequential = chrono::high_resolution_clock::now();
    sequentialProcessing(purchases);
    auto endSequential = chrono::high_resolution_clock::now();
    chrono::duration<double, milli> sequentialDuration = endSequential - startSequential;

    cout << "Sequential processing time: " << sequentialDuration.count() << " ms" << endl;

    // Измерение времени выполнения параллельной обработки
    auto startParallel = chrono::high_resolution_clock::now();
    parallelProcessing(purchases, numThreads);
    auto endParallel = chrono::high_resolution_clock::now();
    chrono::duration<double, milli> parallelDuration = endParallel - startParallel;

    cout << "Parallel processing time: " << parallelDuration.count() << " ms" << endl;

    // Вычисление средней цены всех продуктов
    double averagePrice = calculateAveragePrice(purchases);
    cout << "Average price of all products: " << averagePrice << endl;

    return 0;
}
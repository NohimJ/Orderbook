#include <iostream>
#include <chrono>
#include "Benchmark.cpp"
#include "orderbook.h"

int main()
{
    Orderbook orderbook;

    auto start = std::chrono::high_resolution_clock::now();

    std::thread Worker1(AddOrdersWorker, std::ref(orderbook), 0, 100000);
    std::thread Worker2(AddOrdersWorker, std::ref(orderbook), 100000, 100000);
    std::thread Worker3(AddOrdersWorker, std::ref(orderbook), 200000, 100000);
    std::thread Worker4(AddOrdersWorker, std::ref(orderbook), 300000, 100000);
    Worker1.join();
    Worker2.join();
    Worker3.join();
    Worker4.join();
    
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    double per_order = (double)ns / 800000.0;

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Added 400000 orders across 4 threads in " << duration.count() << " ms" << std::endl;
    std::cout << "Total ns: " << ns << "\n";
    std::cout << "ns/order: " << per_order << "\n";
    std::cout << "orders/sec: " << 800000.0 / (ns / 1e9) << "\n";
    std::cout << "Final book size: " << orderbook.Size() << std::endl;
    
}
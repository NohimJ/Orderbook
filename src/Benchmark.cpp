#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include <mach/mach_time.h>
#include "orderbook.h"

// Throughput benchmark: 4 threads, measure total time
void AddOrdersWorker(Orderbook& orderbook, OrderId startId, int count)
{
    for (int i = 0; i < count; i++)
    {
        OrderId id = startId + i;
        orderbook.AddOrder(std::make_unique<Order>(OrderType::GoodTillCancel, id, Side::Buy, 100, 10));
    }
}

void BenchmarkOrders() //measuring 400k orders
{
    std::cout << "\n=== Order Benchmark (4 threads, 400k orders) ===\n";
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

    double per_order = (double)ns / 400000.0;
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Added 400000 orders across 4 threads in " << duration.count() << " ms" << std::endl;
    std::cout << "ns/order: " << per_order << "\n";
    std::cout << "orders/sec: " << 400000.0 / (ns / 1e9) << "\n";
    std::cout << "Final book size: " << orderbook.Size() << std::endl;
}

// Latency benchmark: single thread, measure p50/p95/p99, cant use RDTSC since on mac m4
void BenchmarkLatency()
{
    std::cout << "\n=== Latency Benchmark (single thread, 100k orders) ===\n";
    Orderbook orderbook;
    std::vector<uint64_t> latencies;
    latencies.reserve(100000);

    // Get mach_timebase_info for ns conversion
    static mach_timebase_info_data_t timebase = {};
    if (timebase.denom == 0) {
        mach_timebase_info(&timebase);
    }

    for (OrderId i = 0; i < 100000; i++)
    {
        uint64_t start_cycles = mach_absolute_time();
        orderbook.AddOrder(std::make_unique<Order>(OrderType::GoodTillCancel, i, Side::Buy, 100 + (i % 50), 10));
        uint64_t end_cycles = mach_absolute_time();

        uint64_t elapsed_ns = (end_cycles - start_cycles) * timebase.numer / timebase.denom;
        latencies.push_back(elapsed_ns);
    }

    // Calculate percentiles
    std::sort(latencies.begin(), latencies.end());

    auto percentile = [&](int p) -> uint64_t {
    size_t idx = std::min((size_t)(latencies.size() * p / 100), latencies.size() - 1);
    return latencies[idx];
    };

    std::cout << "p50:  " << percentile(50) << " ns\n";
    std::cout << "p95:  " << percentile(95) << " ns\n";
    std::cout << "p99:  " << percentile(99) << " ns\n";
    std::cout << "max:  " << latencies.back() << " ns\n";
    std::cout << "Final book size: " << orderbook.Size() << std::endl;
}

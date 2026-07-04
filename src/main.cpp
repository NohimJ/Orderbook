#include <iostream>
#include <chrono>
#include "Benchmark.cpp"
#include "orderbook.h"

int main()
{
    BenchmarkOrders();
    BenchmarkLatency();
    
}
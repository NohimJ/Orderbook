#include <iostream>
#include <chrono>
#include "Benchmark.cpp"
#include "orderbook.h"

int main()
{
    Orderbook orderbook;
    
    AddOrdersWorker(orderbook, 1, 4);
    
    
    
    
    
    /*
        // Two resting asks: 5 units @ 100, 5 units @ 105
    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Sell, 100, 5));
    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 2, Side::Sell, 105, 5));

    // Market buy for 8 — should sweep through both levels
    auto trades = orderbook.AddOrder(std::make_shared<Order>(3, Side::Buy, 8));

    std::cout << "Trades: " << trades.size() << std::endl;       // expect 2 (one trade per price level consumed)
    std::cout << "Book size: " << orderbook.Size() << std::endl; // expect 1 (2 remaining units resting at 105)
   */
   
    /*
   Orderbook orderbook;

    // Only 5 available at price 100 on the ask side
    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Sell, 100, 5));

    // Try to FillOrKill buy 10 — should be rejected entirely, size stays the same
    auto trades = orderbook.AddOrder(std::make_shared<Order>(OrderType::FillOrKill, 2, Side::Buy, 100, 10));

    std::cout << "Trades: " << trades.size() << std::endl;     // expect 0
    std::cout << "Book size: " << orderbook.Size() << std::endl; // expect 1 (only the original ask remains)
    */
   
   
   
    /*
    Orderbook orderbook;
    const OrderId orderId = 1;
    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, orderId, Side::Sell, 100, 5));
    std::cout << orderbook.Size() << std::endl;
    orderbook.CancelOrder(orderId);
    std::cout << orderbook.Size() << std::endl; // 0
    return 0;
   */
}
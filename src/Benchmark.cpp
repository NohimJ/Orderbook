#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include "orderbook.h"

void AddOrdersWorker(Orderbook& orderbook, OrderId startId, int count)
{
    for (int i = 0; i < count; i++)
    {
        OrderId id = startId + i;
        orderbook.AddOrder(std::make_unique<Order>(OrderType::GoodTillCancel, id, Side::Buy, 100, 10));
    }
}
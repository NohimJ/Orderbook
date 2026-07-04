#pragma once

#include <map>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "usings.h"
#include "order.h"
#include "modify_order.h"
#include "trade.h"
#include "orderbook_level_infos.h"

class Orderbook
{
private:

    mutable std::mutex ordersMutex_;
    std::thread ordersPruneThread_;
    std::condition_variable shutdownConditionVariable_;
    std::atomic<bool> shutdown_{ false };

    struct LevelData
    {
        Quantity quantity_{ };
        Quantity count_{ };

        enum class Action
        {
            Add,
            Remove,
            Match,
        };
    };

    struct OrderEntry
    {
        Order* order_{ nullptr };
        OrderPointers::iterator location_;
    };

    std::unordered_map<Price, LevelData> data_;
    std::map<Price, OrderPointers, std::greater<Price>> bids_;
    std::map<Price, OrderPointers, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderEntry> orders_;

    void UpdateLevelData(Price price, Quantity quantity, LevelData::Action action);
    bool CanMatch(Side side, Price price) const;
    bool CanFullyFill(Side side, Price price, Quantity quantity) const;
    void PruneGoodForDayOrders();
    void CancelOrderInternal(OrderId orderId);
    Trades AddOrderInternal(OrderPointer&& order);
    Trades MatchOrders();

public:

    Orderbook();
    Orderbook(const Orderbook&) = delete;
    Orderbook& operator=(const Orderbook&) = delete;
    Orderbook(Orderbook&&) = delete;
    Orderbook& operator=(Orderbook&&) = delete;
    ~Orderbook();

    Trades AddOrder(OrderPointer order);
    void CancelOrder(OrderId orderId);
    Trades MatchOrder(ModifyOrder order);

    std::size_t Size() const;
    OrderbookLevelInfos GetOrderInfos() const;
    
};
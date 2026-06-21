#pragma once

#include <map>
#include <unordered_map>

#include "usings.h"
#include "order.h"
#include "modify_order.h"
#include "trade.h"
#include "orderbook_level_infos.h"

class Orderbook
{
private:

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
        OrderPointer order_{ nullptr };
        OrderPointers::iterator location_;
    };

    std::unordered_map<Price, LevelData> data_;
    std::map<Price, OrderPointers, std::greater<Price>> bids_;
    std::map<Price, OrderPointers, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderEntry> orders_;

    bool CanMatch(Side side, Price price) const;
    bool CanFullyFill(Side side, Price price, Quantity quantity) const;
    Trades MatchOrders();

public:

    Orderbook() = default;
    Orderbook(const Orderbook&) = delete;
    Orderbook& operator=(const Orderbook&) = delete;
    Orderbook(Orderbook&&) = delete;
    Orderbook& operator=(Orderbook&&) = delete;
    ~Orderbook() = default;

    Trades AddOrder(OrderPointer order);
    void CancelOrder(OrderId orderId);
    Trades MatchOrder(ModifyOrder order);

    std::size_t Size() const;
    OrderbookLevelInfos GetOrderInfos() const;
    void UpdateLevelData(Price price, Quantity quantity, LevelData::Action action);
};
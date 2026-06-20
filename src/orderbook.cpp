#include <numeric>
#include <algorithm>

#include "orderbook.h"

bool Orderbook::CanMatch(Side side, Price price) const
{
    if (side == Side::Buy)
    {
        if (asks_.empty())
            return false;

        const auto& [bestAsk, _] = *asks_.begin();
        return price >= bestAsk;
    }
    else
    {
        if (bids_.empty())
            return false;

        const auto& [bestBid, _] = *bids_.begin();
        return price <= bestBid;
    }
}

Trades Orderbook::MatchOrders()
{
    Trades trades;
    trades.reserve(orders_.size());

    while (true)
    {
        if (bids_.empty() || asks_.empty())
            break;

        auto bidIt = bids_.begin();
        auto askIt = asks_.begin();
        if (bidIt == bids_.end() || askIt == asks_.end())
            break;

        auto bidPrice = bidIt->first;
        auto askPrice = askIt->first;
        auto& bids = bidIt->second;
        auto& asks = askIt->second;

        if (bidPrice < askPrice)
            break;

        while (!bids.empty() && !asks.empty())
        {
            auto& bid = bids.front();
            auto& ask = asks.front();

            Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());

            bid->Fill(quantity);
            ask->Fill(quantity);

            if (bid->isFilled())
            {
                bids.pop_front();
                orders_.erase(bid->GetOrderId());
            }

            if (ask->isFilled())
            {
                asks.pop_front();
                orders_.erase(ask->GetOrderId());
            }

            trades.push_back(Trade{
                TradeInfo{ bid->GetOrderId(), bid->GetPrice(), quantity },
                TradeInfo{ ask->GetOrderId(), ask->GetPrice(), quantity }
            });
        }

        if (bids.empty())
            bids_.erase(bidPrice);
        if (asks.empty())
            asks_.erase(askPrice);
    }

    // After matching, cancel any remaining FillAndKill order sitting at
    // the top of either book, since it should not rest unmatched.
    if (!bids_.empty())
    {
        auto& bids = bids_.begin()->second;
        if (!bids.empty())
        {
            auto& order = bids.front();
            if (order->GetOrderType() == OrderType::FillAndKill)
                CancelOrder(order->GetOrderId());
        }
    }
    if (!asks_.empty())
    {
        auto& asks = asks_.begin()->second;
        if (!asks.empty())
        {
            auto& order = asks.front();
            if (order->GetOrderType() == OrderType::FillAndKill)
                CancelOrder(order->GetOrderId());
        }
    }

    return trades;
}

Trades Orderbook::AddOrder(OrderPointer order)
{
    if (orders_.find(order->GetOrderId()) != orders_.end())
        return { };

    if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
        return { };

    OrderPointers::iterator iterator;

    if (order->GetSide() == Side::Buy)
    {
        auto& orders = bids_[order->GetPrice()];
        orders.push_back(order);
        iterator = std::next(orders.begin(), orders.size() - 1);
    }
    else
    {
        auto& orders = asks_[order->GetPrice()];
        orders.push_back(order);
        iterator = std::next(orders.begin(), orders.size() - 1);
    }

    orders_.insert({ order->GetOrderId(), OrderEntry{ order, iterator } });
    return MatchOrders();
}

void Orderbook::CancelOrder(OrderId orderId)
{
    if (orders_.find(orderId) == orders_.end())
        return;

    const auto& [order, iterator] = orders_.at(orderId);

    if (order->GetSide() == Side::Sell)
    {
        auto price = order->GetPrice();
        auto& orders = asks_.at(price);
        orders.erase(iterator);
        if (orders.empty())
            asks_.erase(price);
    }
    else
    {
        auto price = order->GetPrice();
        auto& orders = bids_.at(price);
        orders.erase(iterator);
        if (orders.empty())
            bids_.erase(price);
    }
    orders_.erase(orderId);
}

Trades Orderbook::MatchOrder(ModifyOrder order)
{
    if (orders_.find(order.GetOrderId()) == orders_.end())
        return { };

    // Copy the existing order type out by value BEFORE cancelling.
    // CancelOrder erases the entry from orders_, which would leave a
    // structured-binding reference into that entry dangling if we tried
    // to read from it afterward.
    const auto existingOrderType = orders_.at(order.GetOrderId()).order_->GetOrderType();
    CancelOrder(order.GetOrderId());
    return AddOrder(order.ToOrderPointer(existingOrderType));
}

std::size_t Orderbook::Size() const { return orders_.size(); }

OrderbookLevelInfos Orderbook::GetOrderInfos() const
{
    LevelInfos bidInfos, askInfos;
    bidInfos.reserve(orders_.size());
    askInfos.reserve(orders_.size());

    auto CreateLevelInfos = [](Price price, const OrderPointers& orders)
    {
        return LevelInfo{ price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
            [](Quantity runningSum, const OrderPointer& order)
            { return runningSum + order->GetRemainingQuantity(); }) };
    };

    for (auto it = bids_.begin(); it != bids_.end(); ++it)
        bidInfos.push_back(CreateLevelInfos(it->first, it->second));

    for (auto it = asks_.begin(); it != asks_.end(); ++it)
        askInfos.push_back(CreateLevelInfos(it->first, it->second));

    return OrderbookLevelInfos{ bidInfos, askInfos };
}
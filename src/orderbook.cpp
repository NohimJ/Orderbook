#include <numeric>
#include <algorithm>

#include "orderbook.h"

Orderbook::Orderbook()
    : ordersPruneThread_{ [this] { PruneGoodForDayOrders(); } }
{ }

Orderbook::~Orderbook()
{
    shutdown_.store(true, std::memory_order_release);
    shutdownConditionVariable_.notify_one();
    ordersPruneThread_.join();
}

void Orderbook::PruneGoodForDayOrders()
{
    using namespace std::chrono;

    while (true)
    {
        const auto end = []
        {
            const auto now = system_clock::now();
            const auto rawTime = system_clock::to_time_t(now);
            std::tm timeInfo{};
            localtime_r(&rawTime, &timeInfo);

            timeInfo.tm_hour = 0;
            timeInfo.tm_min = 0;
            timeInfo.tm_sec = 0;
            timeInfo.tm_mday += 1;

            return system_clock::from_time_t(std::mktime(&timeInfo));
        }();

        std::unique_lock ordersLock{ ordersMutex_ };

        if (shutdownConditionVariable_.wait_until(ordersLock, end, [this] { return shutdown_.load(std::memory_order_acquire); }))
        {
            return;
        }

        // reached end-of-day, not shutdown — proceed with pruning
        std::vector<OrderId> orderIds;

        for (const auto& [_, entry] : orders_)
        {
            const auto& order = entry.order_;
            if (order->GetOrderType() == OrderType::GoodForDay)
                orderIds.push_back(order->GetOrderId());
        }

        for (const auto& orderId : orderIds)
            CancelOrderInternal(orderId);
    }
}

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

            // Capture everything needed for the trade record now, before either
            // order is potentially popped from its list below — pop_front
            // invalidates bid/ask, so reading from them afterward is a use-after-free.
            auto bidOrderId = bid->GetOrderId();
            auto bidPriceForTrade = bid->GetPrice();
            auto askOrderId = ask->GetOrderId();
            auto askPriceForTrade = ask->GetPrice();

            UpdateLevelData(bidPriceForTrade, quantity, LevelData::Action::Match);
            UpdateLevelData(askPriceForTrade, quantity, LevelData::Action::Match);

            if (bid->isFilled())
            {
                UpdateLevelData(bidPriceForTrade, 0, LevelData::Action::Remove);
                bids.pop_front();
                orders_.erase(bidOrderId);
            }

            if (ask->isFilled())
            {
                UpdateLevelData(askPriceForTrade, 0, LevelData::Action::Remove);
                asks.pop_front();
                orders_.erase(askOrderId);
            }

            trades.push_back(Trade{
                TradeInfo{ bidOrderId, bidPriceForTrade, quantity },
                TradeInfo{ askOrderId, askPriceForTrade, quantity }
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
                CancelOrderInternal(order->GetOrderId());
        }
    }
    if (!asks_.empty())
    {
        auto& asks = asks_.begin()->second;
        if (!asks.empty())
        {
            auto& order = asks.front();
            if (order->GetOrderType() == OrderType::FillAndKill)
                CancelOrderInternal(order->GetOrderId());
        }
    }

    return trades;
}

Trades Orderbook::AddOrderInternal(OrderPointer order)      //internals dont contain locks to prevent unncesary locking twice
{

    if (order->GetOrderType() == OrderType::Market)
        {
        if (order->GetSide() == Side::Buy && !asks_.empty())
        {
            const auto& [worstAsk, _] = *asks_.rbegin();        //fills market order with worst ask (i.e highest asking price)
            order->ToGoodTillCancel(worstAsk);
        }
        else if (order->GetSide() == Side::Sell && !bids_.empty())
        {
            const auto& [worstBid, _] = *bids_.rbegin();        //fills market order with worst bid (i.e lowest bidding price)
            order->ToGoodTillCancel(worstBid);
        }
        else
        {
            return { };
        }
    }
    if (orders_.find(order->GetOrderId()) != orders_.end())
        return { };

    if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
        return { };
    
    if (order->GetOrderType() == OrderType::FillOrKill && !CanFullyFill(order->GetSide(), order->GetPrice(), order->GetRemainingQuantity()))
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
    UpdateLevelData(order->GetPrice(), order->GetRemainingQuantity(), LevelData::Action::Add);
    return MatchOrders();
}

Trades Orderbook::AddOrder(OrderPointer order)
{
    std::scoped_lock ordersLock{ ordersMutex_ };
    return AddOrderInternal(order);
}

void Orderbook::CancelOrderInternal(OrderId orderId)
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
    UpdateLevelData(order->GetPrice(), order->GetRemainingQuantity(), LevelData::Action::Remove);
    orders_.erase(orderId);
}

Trades Orderbook::MatchOrder(ModifyOrder order)
{
    std::scoped_lock ordersLock{ ordersMutex_ };

    if (orders_.find(order.GetOrderId()) == orders_.end())
        return { };

    
    const auto existingOrderType = orders_.at(order.GetOrderId()).order_->GetOrderType();
    CancelOrderInternal(order.GetOrderId());
    return AddOrderInternal(order.ToOrderPointer(existingOrderType));
}

void Orderbook::CancelOrder(OrderId orderId)
{
    std::scoped_lock ordersLock{ ordersMutex_ };
    CancelOrderInternal(orderId);
}

std::size_t Orderbook::Size() const 
{ 
    std::scoped_lock ordersLock{ ordersMutex_ };
    return orders_.size(); 
}

OrderbookLevelInfos Orderbook::GetOrderInfos() const
{
    std::scoped_lock ordersLock{ ordersMutex_ };
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

void Orderbook::UpdateLevelData(Price price, Quantity quantity, LevelData::Action action)
{
        auto& data = data_[price];

        switch (action)
    {
        case LevelData::Action::Add:
            data.count_ += 1;
            data.quantity_ += quantity; //quantity gets added by how much new order bought
            break;
        case LevelData::Action::Remove:
            data.count_ -= 1;
            data.quantity_ -= quantity;
            break;
        case LevelData::Action::Match:
            data.quantity_ -= quantity;
            break;
    }     

    if (data.quantity_ == 0 && data.count_ == 0)
        data_.erase(price);

}

bool Orderbook::CanFullyFill(Side side, Price price, Quantity quantity) const
{
    if (!CanMatch(side, price))
        return false;
    if (side == Side::Buy)
    {
        Quantity totalAvailable = 0;

        for (const auto& [levelPrice, levelOrders] : asks_)
        {
            if (levelPrice > price)
                break; // no further levels can qualify, since asks_ is sorted ascending

            totalAvailable += data_.at(levelPrice).quantity_;

            if (totalAvailable >= quantity)
                return true;
        }

        return false; // parsed through everything that qualifies
    }
    else
    {
        Quantity totalAvailable = 0;

        for (const auto& [levelPrice, levelOrders] : bids_)
        {
            if (levelPrice < price)
                break; // no further levels can qualify, since bids_ is sorted descending

            totalAvailable += data_.at(levelPrice).quantity_;

            if (totalAvailable >= quantity)
                return true;
        }

        return false; // parsed through everything that qualifies

    }
}
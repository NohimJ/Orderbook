#include <iostream>
#include <vector>
#include <array>
#include <unordered_map>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <numeric>
#include <sstream>
#include <cstdint>

//class for labeling specific order types
enum class OrderType     
{
    GoodTillCancel,
    FillAndKill
};

enum class Side
{
    Buy,
    Sell

};

//alias simple types to give code more readability

using Price = std::int32_t;  //prices can be negative
using Quantity = uint32_t;
using OrderId = std::uint64_t;

struct LevelInfo //will be used in punlic apis to get info about orderbook
{
    Price price_;
    Quantity quantity_;

};

using LevelInfos = std::vector<LevelInfo>;

class OrderbookLevelInfos
{
public:
    OrderbookLevelInfos(const LevelInfos& bids, const LevelInfos& asks)
        : bids_{ bids }
        , asks_{ asks }
    { }

    const LevelInfos& GetBids() const { return bids_; }
    const LevelInfos& GetAsks() const { return asks_; }

    private:
        LevelInfos bids_;
        LevelInfos asks_;
    
};

class Order
{
public:
    Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity)
        : orderType_{ orderType }
        , orderId_{ orderId }
        , side_{ side }
        , price_{ price }
        , initialQuantity_{ quantity } 
        , remainingQuantity_{ quantity }
    { }

    OrderType GetOrderType() const {return orderType_;}
    OrderId GetOrderId() const {return orderId_;}
    Side GetSide() const {return side_; }
    Price GetPrice() const { return price_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    Quantity GetFilledQuantity() const { return GetInitialQuantity() - GetRemainingQuantity(); }
    bool isFilled() const { return GetRemainingQuantity() == 0; }
    void Fill(Quantity quantity)
    {
        if(quantity > GetRemainingQuantity()) {
            std::ostringstream oss;
            oss << "Order (" << GetOrderId() << ") cannot be filled for more than the remaining quantity.";
            throw std::logic_error(oss.str());
        }
        remainingQuantity_ -= quantity;
    }

private:
    OrderType orderType_;       //priivate members that cannot. be modified by anything outsdie the class
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};
//copying large orders can be expensive,thats why we use a shared ptr
using OrderPointer = std::shared_ptr<Order>;  //order can be both stored in a dict (price, ticker) or bid ask based dict
using OrderPointers = std::list<OrderPointer>;

class ModifyOrder
{
public:
    ModifyOrder(OrderId orderId, Side side, Price price, Quantity quantity)
        : orderId_{ orderId }
        , price_{ price }
        , side_{ side }
        , quantity_{ quantity }
    { }

    OrderId GetOrderId() const { return orderId_; }
    Price GetPrice() const  { return price_; }      //APIS
    Side GetSide() const { return side_; }
    Quantity GetQuantity() const { return quantity_; }

    OrderPointer ToOrderPointer(OrderType type) const //currently only works with GoodTillCancel, but want to futureproof code if we add more order types in the future
    {
        return std::make_shared<Order>(type, GetOrderId(), GetSide(), GetPrice(), GetQuantity());
    }

private:
    OrderId orderId_;
    Price price_;
    Side side_;
    Quantity quantity_;
};

struct TradeInfo
{
    OrderId orderId_;
    Price price_;
    Quantity quantity_;
};

class Trade
{
public:
    Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade)
        : bidTrade_ { bidTrade }
        , askTrade_ { askTrade }
    { }

    const TradeInfo& GetBidTrade() const { return bidTrade_; }
    const TradeInfo& GetAskTrade() const { return askTrade_; }
    
private:
    TradeInfo bidTrade_;
    TradeInfo askTrade_;
};

using Trades = std::vector<Trade>; //return a vector of trades, (can be different order types in the future)

class Orderbook
{
private:

    struct OrderEntry
    {
        OrderPointer order_{ nullptr };
        OrderPointers::iterator location_;

    };

    std::map<Price, OrderPointers, std::greater<Price>> bids_;
    std::map<Price, OrderPointers, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderEntry> orders_;

    bool CanMatch(Side side, Price price) const
    {
        if (side == Side::Buy)
        {
            if(asks_.empty())   //if no asks, can't match, so return false
                return false;
            
            const auto& [bestAsk, _] = *asks_.begin();
            return price >= bestAsk; 
        }
        else
        {
            if(bids_.empty())
                return false;
            
            const auto& [bestBid, _] = *bids_.begin();
            return price <= bestBid;
        }
    }

    Trades MatchOrders()
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
                    orders_.erase(bid->GetOrderId());   //if order filled, not order anymore, so erase OrderId of that specific order
                }

                if (ask->isFilled())
                {
                    asks.pop_front();
                    orders_.erase(ask->GetOrderId());
                }


                trades.push_back(Trade{ 
                    TradeInfo{ bid->GetOrderId(), bid->GetPrice(), quantity },
                    TradeInfo{ ask->GetOrderId(), ask->GetPrice(), quantity}
                });
            }
        if (bids.empty())
            bids_.erase(bidPrice);
        if (asks.empty())
            asks_.erase(askPrice);
        }
    // Changed: replaced structured bindings with explicit iterators and added runtime checks for C++17 compatibility and safety
    if (!bids_.empty())
    {
        auto& bids = bids_.begin()->second;
        if (!bids.empty()) {
            auto& order = bids.front();
            if (order->GetOrderType() == OrderType::FillAndKill)
                CancelOrder(order->GetOrderId());
        }
    }
    if (!asks_.empty())
    {
        auto& asks = asks_.begin()->second;
        if (!asks.empty()) {
            auto& order = asks.front();
            if (order->GetOrderType() == OrderType::FillAndKill)
                CancelOrder(order->GetOrderId());
        }
    }
        return trades;
    }

public:

    Trades AddOrder(OrderPointer order)
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
            iterator = std::next(orders.begin(), orders.size()-1);
        }
        else
        {
            auto& orders = asks_[order->GetPrice()];
            orders.push_back(order);
            iterator = std::next(orders.begin(), orders.size() - 1);
        }

        orders_.insert({ order-> GetOrderId(), OrderEntry{ order, iterator} });
        return MatchOrders(); //run matching algorithmn
    }

    void CancelOrder(OrderId orderId) 
    {
        if (orders_.find(orderId) == orders_.end()) //if no orders return nothing
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

    Trades MatchOrder(ModifyOrder order) //ModifyOrder API
    {
        if (orders_.find(order.GetOrderId()) == orders_.end())
            return { };

        const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
        CancelOrder(order.GetOrderId());
        return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
    }

    std::size_t Size() const { return orders_.size();}

    OrderbookLevelInfos GetOrderInfos() const
    {
        LevelInfos bidInfos, askInfos;
        bidInfos.reserve(orders_.size());
        askInfos.reserve(orders_.size());

        auto CreateLevelInfos = [](Price price, const OrderPointers& orders)
        {
            return LevelInfo{ price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
            [](Quantity runningSum, const OrderPointer& order)
            { return runningSum + order->GetRemainingQuantity(); })};
        };

        for (auto it = bids_.begin(); it != bids_.end(); ++it) {
            bidInfos.push_back(CreateLevelInfos(it->first, it->second));
        }
        for (auto it = asks_.begin(); it != asks_.end(); ++it) {
            askInfos.push_back(CreateLevelInfos(it->first, it->second));
        }
        return OrderbookLevelInfos{ bidInfos, askInfos };
    }



};


int main()
{
    Orderbook orderbook;
    const OrderId orderId = 1;
    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, orderId, Side::Buy, 100, 10));
    std::cout << orderbook.Size() << std::endl;
    orderbook.CancelOrder(orderId);
    std::cout << orderbook.Size() << std::endl; //0
    return 0;
}
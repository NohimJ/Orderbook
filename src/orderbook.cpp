#include <iostream>
#include <vector>
#include <array>
#include <unordered_map>
#include <deque>
#include <list>

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
    Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity)
        : orderType_{ orderType }
        , orderId_{ orderId }
        , side_{ side }
        , price_{ price }
        , initialQuantity_{ quantity } 
        , remainingQuantity_{ quantity }
    { }

    OrderId GetOrderId() const {return orderId_;}
    Side GetSide() const {return side_; }
    Price GetPrice() const { return price_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    Quantity GetFilledQuantity() const { return GetInitialQuantity() - GetRemainingQuantity(); }
    void Fill(Quantity quantity)
    {
        if(quantity > GetRemainingQuantity())
            throw std::logic_error(std::format("Order ({}) cannot be filled for more than the remaining quantit. ", GetOrderId()));

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



int main()
{

    return 0;
}
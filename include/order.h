#pragma once

#include <list>
#include <memory>
#include <sstream>
#include <stdexcept>

#include "order_type.h"
#include "side.h"
#include "usings.h"

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

    Order(OrderId orderId, Side side, Quantity quantity)
    : Order(OrderType::Market, orderId, side, InvalidPrice, quantity)
    { }

    OrderType GetOrderType() const { return orderType_; }
    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    Quantity GetFilledQuantity() const { return GetInitialQuantity() - GetRemainingQuantity(); }
    bool isFilled() const { return GetRemainingQuantity() == 0; }

    void Fill(Quantity quantity)
    {
        if (quantity > GetRemainingQuantity())
        {
            std::ostringstream oss;
            oss << "Order (" << GetOrderId() << ") cannot be filled for more than the remaining quantity.";
            throw std::logic_error(oss.str());
        }
        remainingQuantity_ -= quantity;
    }
    void ToGoodTillCancel(Price price)
    {
        price_ = price;
        orderType_ = OrderType::GoodTillCancel;
    }

private:
    OrderType orderType_;
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};

// Orders are stored via shared_ptr since copying a large Order is expensive
// and the same order needs to be referenced from multiple containers
// (the price-level map and the orderId lookup map).
using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;
#include <gtest/gtest.h>
#include "orderbook.h"

TEST(OrderbookTests, GoodTillCancel_RestsInBook)
{
    Orderbook orderbook;

    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Buy, 100, 10));

    EXPECT_EQ(orderbook.Size(), 1);
}

TEST(OrderbookTests, FillOrKill_RejectsWhenNotEnoughLiquidity)
{
    Orderbook orderbook;

    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Sell, 100, 5));

    auto trades = orderbook.AddOrder(std::make_shared<Order>(OrderType::FillOrKill, 2, Side::Buy, 100, 10));

    EXPECT_EQ(trades.size(), 0);
    EXPECT_EQ(orderbook.Size(), 1);
}

TEST(OrderbookTests, Market_FillsAcrossAskLevels)
{
    Orderbook orderbook;

    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Sell, 100, 5));
    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 2, Side::Sell, 105, 5));

    auto trades = orderbook.AddOrder(std::make_shared<Order>(3, Side::Buy, 8));
    EXPECT_EQ(trades.size(), 2);
    EXPECT_EQ(orderbook.Size(), 1);

}

TEST(OrderbookTests, MatchOrders_FullFillDoesNotCrash)
{
    Orderbook orderbook;

    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Sell, 100, 5));

    auto trades = orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 2, Side::Buy, 100, 5));

    EXPECT_EQ(trades.size(), 1);
    EXPECT_EQ(orderbook.Size(), 0);
}

TEST(OrderbookTests, GoodForDay_RestsInBookLikeGoodTillCancel)
{
    Orderbook orderbook;

    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodForDay, 1, Side::Buy, 100, 10));

    EXPECT_EQ(orderbook.Size(), 1);
}
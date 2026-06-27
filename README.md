# C++ Order Book

A limit order book matching engine in C++, supporting multiple order types, level-aggregated quantity tracking, and a background thread for time-based order expiry, built as a learning project to understand the data structures and concurrency patterns used in trading systems.

## Order Types

- **GoodTillCancel** — rests in the book until matched or explicitly cancelled.
- **FillAndKill** — fills whatever it can immediately, any unfilled amount is cancelled rather than resting.
- **FillOrKill** — If the book can't completely fill the order right now, none of it fills. Checked via `CanFullyFill`, which sums quantity across price levels (using level-aggregated data, not a per-order scan) before accepting the order.
- **Market** — no price specified. On arrival, it's assigned the worst price on the opposing side (via `Order::ToGoodTillCancel`), so it sweeps through every price level needed to fill, then rests as a normal `GoodTillCancel` order if anything remains unfilled.
- **GoodForDay** — behaves exactly like `GoodTillCancel` during the day, but is automatically cancelled by a background thread at midnight.

## Design

- **`std::map<Price, OrderPointers, std::greater<Price>>` for bids, `std::less<Price>` for asks** — keeps each side sorted so the best price is always at `begin()`, with O(log n) insert/erase.

- **`std::unordered_map<OrderId, OrderEntry>`** for O(1) order lookup by id, storing both the order and a `std::list` iterator so a specific order can be erased from its price level in O(1). Without this, cancelling an order would require scanning the whole list which is much more expensive.

- **`LevelData`** (`quantity_`, `count_`) is maintained per price level in `data_`, updated incrementally on every add/remove/match via `UpdateLevelData`. This is what makes `CanFullyFill` O(price levels touched) instead of O(orders at every level). It reads precomputed totals rather than summing every order's remaining quantity on each check.

- **`std::shared_ptr<Order>`** for order ownership, since the same order is referenced from both a price-level list and the id lookup map.

## Concurrency Usage

`GoodForDay` requires a background thread that wakes at midnight and cancels expired orders, this means the order book now has two threads touching shared state, so every public method is mutex-protected.

- **`mutable std::mutex ordersMutex_`** guards `bids_`, `asks_`, `orders_`, `data_`.

- Each public method (`AddOrder`, `CancelOrder`, `MatchOrder`) is a thin wrapper: lock, then call a private `*Internal` version containing the actual logic with **no locking**. This split exists because the public methods can call each other internally (e.g. `MatchOrder` cancels and re-adds) — without it, a thread that already holds the lock would try to lock the same non-reentrant mutex again and deadlock against itself.

- The pruning thread uses `std::condition_variable::wait_until` with a **predicate** (`[this] { return shutdown_.load(...); }`), not a bare timeout check. An earlier version checked the shutdown flag *after* waking instead of as a predicate, which created a lost-wakeup race: if the destructor signalled shutdown before the thread had reached the wait call, the notification had no effect and the thread would sleep for the full duration until midnight, hanging `.join()` in the destructor. The predicate form closes that gap by being re-checked atomically every time the thread wakes or is notified.

## Performance: a real bug found by benchmarking, not unit testing

The correctness tests below all use small numbers of orders (1–2 per test), which is enough to verify behavior but not performance. A separate stress benchmark, 4 threads concurrently calling `AddOrder` on the same book, each with a non-overlapping `OrderId` range to isolate the mutex as the shareable resource.

```cpp
auto& orders = bids_[order->GetPrice()];
orders.push_back(order);
iterator = std::next(orders.begin(), orders.size() - 1);   // O(n) walk on a std::list
```

`std::list` has no random access, so finding "the last element" by walking forward from `begin()` is O(n) — and since this runs on *every* insertion, total cost across n insertions at one price level is O(n²). The fix uses the iterator `push_back` already makes available:

```cpp
iterator = std::prev(orders.end());   // O(1)
```

| Orders (single price level, 4 threads) | Before | After |
|---|---|---|
| 80,000 | 5,559 ms | 177 ms |
| 400,000 | did not complete in reasonable time | 1,055 ms |

~31x faster at 80k orders. This bug was invisible at small and unrealistic amounts and only showed up under a realistic amount of orders which is why this benchmark exists alongside the test suite, not just to produce a number.

## Tests

Built with GoogleTest via CMake's `FetchContent`. Covers each order type, a regression test for a use-after-free found and fixed in `MatchOrders` (reading from `bid`/`ask` references after `pop_front()` had already freed the node — found with AddressSanitizer), and a regression test for the `std::list` iterator fix above (cancelling 1,000 orders at one price level to confirm each order's stored iterator is still correct).

```
cd build
cmake ..
cmake --build .
ctest
```

## Build & Run

```
cd build
cmake ..
cmake --build .
./C++Orderbook
```

## Acknowledgment

The overall architecture (price-level maps, `LevelData` aggregation, order type set) follows the structure of [Tzadiko/Orderbook](https://github.com/Tzadiko/Orderbook), used here as a reference while building and debugging each piece independently.

## What I'd add next

- ThreadSanitizer pass over the concurrent benchmark to validate the locking under contention, not just absence-of-crash
- Spread benchmark orders across multiple price levels to also measure matching throughput, not just insertion
- A FIX protocol parser as a front end, translating wire-format messages into `Order` objects
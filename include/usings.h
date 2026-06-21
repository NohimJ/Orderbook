#pragma once

#include <cstdint>

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;

constexpr Price InvalidPrice = std::numeric_limits<Price>::max();
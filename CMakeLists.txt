cmake_minimum_required(VERSION 3.15)
project(C++Orderbook CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(C++Orderbook src/orderbook.cpp)
target_include_directories(C++Orderbook PRIVATE include)

cmake_minimum_required(VERSION 3.16)
project(orderflow CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release flags for real benchmarks
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_options(-O3 -march=native)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-g)
endif()

include_directories(include)

# Core library
add_library(orderflow_lib
  src/itch_parser.cpp
  src/order_book.cpp
  src/features.cpp
  src/benchmark.cpp
)

# Main replay binary
add_executable(orderflow src/main.cpp)
target_link_libraries(orderflow PRIVATE orderflow_lib)

# Benchmark binary
add_executable(bench_replay benchmarks/bench_replay.cpp)
target_link_libraries(bench_replay PRIVATE orderflow_lib)

# Tests (GoogleTest)
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(test_parser   tests/test_parser.cpp)
add_executable(test_book     tests/test_order_book.cpp)
add_executable(test_features tests/test_features.cpp)

foreach(t test_parser test_book test_features)
  target_link_libraries(${t} PRIVATE orderflow_lib GTest::gtest_main)
  add_test(NAME ${t} COMMAND ${t})
endforeach()

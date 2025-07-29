cmake_minimum_required(VERSION 3.14)
project(Glimpse)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(src)

include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
    DOWNLOAD_EXTRACT_TIMESTAMP true
)

FetchContent_Declare(
    googlebenchmark
    URL https://github.com/google/benchmark/archive/refs/heads/main.zip
    DOWNLOAD_EXTRACT_TIMESTAMP true
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(run_tests
    tests/MPMCQueueTests.cpp
)

target_link_libraries(run_tests
    gtest_main
)

target_include_directories(run_tests PRIVATE
    ${gtest_SOURCE_DIR}/include
    ${gtest_BINARY_DIR}/include
)

add_executable(run_benchmarks
    benchmarks/MPMCQueueBenchmark.cpp
)

target_link_libraries(run_benchmarks
    benchmark::benchmark
)

cmake_minimum_required(VERSION 3.10)
project(KVCache CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ThreadSanitizer option
option(USE_TSAN "Enable ThreadSanitizer" OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3")

if(USE_TSAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -g -O1")
    # Reset O3 to O1 for better TSAN stack traces
endif()

# Core cache library
add_library(cache_core
    cache/src/lru_list.cpp
    cache/src/shard.cpp
    cache/src/stats.cpp
    cache/src/kv_cache.cpp
    cache/src/protocol.cpp
    cache/src/tcp_server.cpp
    cache/src/ttl_sweeper.cpp
    cache/src/logger.cpp
)
target_include_directories(cache_core PUBLIC cache/include)
find_package(Threads REQUIRED)
target_link_libraries(cache_core Threads::Threads)

# Server executable
add_executable(kv_server cache/src/main.cpp)
target_link_libraries(kv_server cache_core)

# Client executable
add_executable(kv_cli client/kv_cli.cpp)
target_link_libraries(kv_cli cache_core)

# Benchmark executable
add_executable(kv_bench bench/kv_bench.cpp)
target_link_libraries(kv_bench cache_core)
